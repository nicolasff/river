#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>

#include "channel.h"
#include "http.h"
#include "json.h"

#include <unistd.h>
#include <pthread.h>

#define LOG_BUFFER_SIZE	20

#define LOG_CUR(c) (c->log_pos)
#define LOG_NEXT(pos) ((pos + 1) % LOG_BUFFER_SIZE)
#define LOG_PREV(pos) ((pos + LOG_BUFFER_SIZE -1) % LOG_BUFFER_SIZE)

/**
 * This is the hash table of all channels.
 */
static dict *__channels = NULL;
static pthread_mutex_t channels_lock;

void
channel_init() {
	if(NULL == __channels) {
		pthread_mutex_init(&channels_lock, NULL);
		__channels = dictCreate(&dictTypeCopyNoneFreeNone, NULL);

	}
}

struct p_channel *
channel_new(const char *name) {

	struct p_channel *channel = calloc(1, sizeof(struct p_channel));

	if(NULL == channel) {
		return NULL;
	}

	channel->name = strdup(name);
	if(NULL == channel->name) {
		free(channel);
		return NULL;
	}
	channel->name_len = strlen(name);

	channel->log_buffer = calloc(LOG_BUFFER_SIZE, sizeof(struct p_channel_message));
	if(NULL == channel->log_buffer) {
		free(channel->name);
		free(channel);
		return NULL;
	}


	/* channel lock */
	pthread_mutex_init(&channel->lock, NULL);

	/* add channel to a global list of channels */
	pthread_mutex_lock(&channels_lock);
	dictAdd(__channels, channel->name, channel, 0);
	pthread_mutex_unlock(&channels_lock);

	return channel;
}

struct p_channel *
channel_find(const char *name) {

	dictEntry *de;
	if((de = dictFind(__channels, name))) {
		return (struct p_channel*)de->val;
	}
	return NULL;
}

void
channel_free(struct p_channel * p) {

	free(p->name);
	free(p);
}

struct p_channel_user *
channel_new_connection(int fd, int keep_connected, const char *jsonp, write_function wfun) {

	struct p_channel_user *pcu = calloc(1, sizeof(struct p_channel_user));
	pcu->wfun = wfun;
	pcu->fd = fd;
	pcu->free_on_remove = 1;
	pcu->keep_connected = keep_connected;

	if(jsonp && *jsonp) {
		char prefix[] = "(";
		pcu->jsonp_len = strlen(jsonp) + sizeof(prefix) - 1;
		pcu->jsonp = calloc(pcu->jsonp_len + 1, 1);

		memcpy(pcu->jsonp, jsonp, pcu->jsonp_len - (sizeof(prefix)-1));
		memcpy(pcu->jsonp + pcu->jsonp_len - (sizeof(prefix)-1),
				prefix, sizeof(prefix)-1);
	}

	return pcu;
}

void
channel_add_connection(struct p_channel *channel, struct p_channel_user *pcu) {

	/* add user to the front of the list */
	if(channel->user_list) {
		channel->user_list->prev = pcu;
	}
	pcu->next = channel->user_list;
	channel->user_list = pcu;
}

void
channel_del_connection(struct p_channel *channel, struct p_channel_user *pcu) {

	/* remove from list */
	if(pcu->next) {
		pcu->next->prev = pcu->prev;
	}
	if(pcu->prev) {
		pcu->prev->next = pcu->next;
	} else {
		if(channel->user_list) {
			channel->user_list = channel->user_list->next;
		}
		if(channel->user_list) {
			channel->user_list->prev = NULL;
		}
	}
	if(channel->user_list == pcu) {
		channel->user_list = NULL;
	}
	if(pcu->free_on_remove) {
		free(pcu->jsonp);
		free(pcu);
	}
}

void
channel_write(struct p_channel *channel, const char *data, size_t data_len) {

	struct p_channel_user *pcu;
	struct p_channel_message *msg;

	CHANNEL_LOCK(channel);

	/* get next pointer to a log message. */
	msg = &channel->log_buffer[channel->log_pos];

	/* use channel sequence number */
	msg->seq = ++(channel->seq);

	free(msg->data); /* free old log message */

	/* copy log data */
	msg->data = json_msg(channel->name, channel->name_len,
			msg->seq,
			data, data_len,
			&msg->data_len);

	/* incr log pointer */
	channel->log_pos = LOG_NEXT(channel->log_pos);

	/* push message to connected users */
	for(pcu = channel->user_list; pcu; ) {
		struct p_channel_user *next = pcu->next;
		/* write message to connected user */
		
		int ret, total = 0, expected_len = msg->data_len;
		char jsonp_end[] = ");\r\n";
		if(pcu->jsonp) {
			expected_len += pcu->jsonp_len + sizeof(jsonp_end)-1;
			total += pcu->wfun(pcu->fd, pcu->jsonp, pcu->jsonp_len);
		}
		total += (ret = pcu->wfun(pcu->fd, msg->data, msg->data_len));
		if(pcu->jsonp) {
			total += pcu->wfun(pcu->fd, jsonp_end, sizeof(jsonp_end)-1);
		}
		if(total != expected_len) { /* failed write */
			shutdown(pcu->fd, SHUT_RDWR);
			close(pcu->fd);
			channel_del_connection(channel, pcu);
		} else if(!pcu->keep_connected) {
			http_streaming_end(pcu->fd);
			channel_del_connection(channel, pcu);
		}
		pcu = next;
	}
	CHANNEL_UNLOCK(channel);
}

http_action
channel_catchup_user(struct p_channel *channel, struct p_channel_user *pcu, unsigned long long seq) {

	struct p_channel_message *msg;
	int pos, first, last, ret, sent_data = 0;
	int success = 1, found = 0;

	last = LOG_CUR(channel);
	first = pos = LOG_PREV(last);

	for(;;) {
		msg = &channel->log_buffer[pos];

		if(last == LOG_PREV(pos) || !msg->seq || msg->seq <= seq) {
			/* found all we could. */
			break;
		}
		first = pos;
		pos = LOG_PREV(pos);
		found = 1;
	}

	if(!found || (first +1 == last && channel->log_buffer[first].seq <= seq)) {
		return HTTP_KEEP_CONNECTED;
	}

	for(pos = first; pos != last; pos = LOG_NEXT(pos)) {

		msg = &channel->log_buffer[pos];

		ret = pcu->wfun(pcu->fd, msg->data, msg->data_len);
		if(ret != (int)msg->data_len) { /* failed write */
			success = 0;
			break;
		} else {
			sent_data = 1;
		}
	}

	if(0 == success) {
		return HTTP_DISCONNECT;
	}
	if(sent_data && (!pcu->keep_connected)) {
		http_streaming_end(pcu->fd);
		return HTTP_DISCONNECT;
	}
	return HTTP_KEEP_CONNECTED;
}

