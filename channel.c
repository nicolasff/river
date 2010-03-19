#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "channel.h"
#include "user.h"
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
channel_add_connection(struct p_channel *channel, struct p_user *user, int fd) {

	struct p_channel_user *pcu = calloc(1, sizeof(struct p_channel_user));
	pcu->user = user;
	pcu->fd = fd;

	CHANNEL_LOCK(channel);

	/* add user to the front of the list */
	if(channel->user_list) {
		channel->user_list->prev = pcu;
	}
	pcu->next = channel->user_list;
	channel->user_list = pcu;

	printf("fd %d added to the channel\n", fd);
	CHANNEL_UNLOCK(channel);

	return pcu;
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
		channel->user_list = channel->user_list->next;
		if(channel->user_list) {
			channel->user_list->prev = NULL;
		}
	}
	printf("fd %d removed from the channel\n", pcu->fd);

	free(pcu);
}

void
channel_write(struct p_channel *channel, long uid, const char *data, size_t data_len) {

	char *json;
	struct p_channel_user *pcu;
	struct p_channel_message *msg;

	CHANNEL_LOCK(channel);

	/* get next pointer to a log message. */
	msg = &channel->log_buffer[channel->log_pos];

	msg->ts = time(NULL); /* timestamp */

	free(msg->data); /* free old log message */

	/* copy log data */
	msg->data = calloc(data_len, 1);
	json = json_msg(channel->name, uid, msg->ts, data, &msg->data_len);
	msg->data = json;
	msg->uid = uid;

	/* incr log pointer */
	channel->log_pos = LOG_NEXT(channel->log_pos);

	/* push message to connected users */
	for(pcu = channel->user_list; pcu; pcu = pcu->next) {

		/* write message to connected user */
		printf("sending json to fd %d\n", pcu->fd);
		int ret = http_streaming_chunk(pcu->fd, msg->data, msg->data_len);
		if(ret != (int)msg->data_len) { /* failed write */
			printf("failed write, closing fd %d\n", pcu->fd);
			close(pcu->fd);
			channel_del_connection(channel, pcu);
		}
	}
	CHANNEL_UNLOCK(channel);
}

int
channel_catchup_user(struct p_channel *channel, struct p_channel_user *pcu, time_t timestamp) {

	struct p_channel_message *msg;
	int pos, first, last, ret;

	last = LOG_CUR(channel);
	first = pos = LOG_PREV(last);
	printf("catch-up on fd %d\n", pcu->fd);

	for(;;) {
		msg = &channel->log_buffer[pos];

		if(last == LOG_PREV(pos) || !msg->ts || msg->ts < timestamp) {
			/* found all we could. */
			break;
		}
		first = pos;
		pos = LOG_PREV(pos);
	}

	if(first +1 == last && channel->log_buffer[first].ts < timestamp) {
		return 1;
	}

	http_streaming_chunk(pcu->fd, "[", 1);
	for(pos = first; pos != last; pos = LOG_NEXT(pos)) {
		int success = 1;

		msg = &channel->log_buffer[pos];

		ret = http_streaming_chunk(pcu->fd, msg->data, msg->data_len);
		if(ret != (int)msg->data_len) { /* failed write */
			success = 0;
		}

		if(pos != LOG_PREV(last)) {
			/* TODO: check return value for each call. */
			ret = http_streaming_chunk(pcu->fd, ", ", 2);
			if(ret != 2) {
				success = 0;
			}
		}
		if(0 == success) {
			close(pcu->fd);
			channel_del_connection(channel, pcu);
			printf("failed write, closing fd %d\n", pcu->fd);
			return 0;
		}

	}
	http_streaming_chunk(pcu->fd, "]", 1);
	http_streaming_end(pcu->fd);

	return 0;
}

