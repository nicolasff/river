#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>

#include "channel.h"
#include "http.h"
#include "json.h"
#include "server.h"
#include "socket.h"

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

struct channel *
channel_new(const char *name) {

	struct channel *channel = calloc(1, sizeof(struct channel));

	if(NULL == channel) {
		return NULL;
	}

	channel->name = strdup(name);
	if(NULL == channel->name) {
		free(channel);
		return NULL;
	}
	channel->name_len = strlen(name);

	channel->log_buffer = calloc(LOG_BUFFER_SIZE, sizeof(struct channel_message));
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

struct channel *
channel_find(const char *name) {

	dictEntry *de;
	if((de = dictFind(__channels, name))) {
		return (struct channel*)de->val;
	}
	return NULL;
}

void
channel_free(struct channel * p) {

	free(p->name);
	free(p);
}

struct channel_user *
channel_new_connection(struct connection *cx, int keep_connected, const char *jsonp, write_function wfun) {

	struct channel_user *cu = calloc(1, sizeof(struct channel_user));
	cu->wfun = wfun;
	cu->cx = cx;
	cu->free_on_remove = 1;
	cu->keep_connected = keep_connected;

	if(jsonp && *jsonp) {
		cu->jsonp_len = strlen(jsonp);
		cu->jsonp = calloc(cu->jsonp_len + 1, 1);
		memcpy(cu->jsonp, jsonp, cu->jsonp_len);
	}

	return cu;
}

void
channel_add_connection(struct channel *channel, struct channel_user *cu) {

	/* add user to the front of the list */
	if(channel->user_list) {
		channel->user_list->prev = cu;
	}
	cu->next = channel->user_list;
	channel->user_list = cu;
}

void
channel_del_connection(struct channel *channel, struct channel_user *cu) {

	/* remove from list */
	if(cu->next) {
		cu->next->prev = cu->prev;
	}
	if(cu->prev) {
		cu->prev->next = cu->next;
	} else {
		if(channel->user_list) {
			channel->user_list = channel->user_list->next;
		}
		if(channel->user_list) {
			channel->user_list->prev = NULL;
		}
	}
	if(channel->user_list == cu) {
		channel->user_list = NULL;
	}
	if(cu->free_on_remove) {
		free(cu->jsonp);
		/* printf("free %p\n", cu); */
		free(cu);
	}
}

void
channel_write(struct channel *channel, const char *data, size_t data_len) {

	struct channel_user *cu;
	struct channel_message *msg;

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
	for(cu = channel->user_list; cu; ) {
		struct channel_user *next = cu->next;
		/* write message to connected user */

		int ret;
		char *buffer;
		size_t sz;

		if(cu->jsonp) {
			buffer = json_wrap(msg->data, msg->data_len, cu->jsonp, cu->jsonp_len, &sz);
		} else {
			buffer = msg->data;
			sz = msg->data_len;
		}

		ret = cu->wfun(cu->cx, buffer, sz);
		if(cu->jsonp) {
			free(buffer);
		}

		if(ret != (int)sz) { /* failed write */
			/*
			printf("failed write on %p\n", cu);
			channel_del_connection(channel, cu);
			*/
		} else if(!cu->keep_connected) {
			http_streaming_end(cu->cx);
			/* printf("cx_remove from %s:%d\n", __FILE__, __LINE__); */
			cx_remove(cu->cx);
			/* printf("calling socket_shutdown from %s:%d\n", __FILE__, __LINE__); */
			/*
			socket_shutdown(cu->cx);
			printf("!keep_connected on %p\n", cu);
			channel_del_connection(channel, cu);
			*/
		}
		cu = next;
	}
	CHANNEL_UNLOCK(channel);
}

http_action
channel_catchup_user(struct channel *channel, struct channel_user *cu, unsigned long long seq) {

	struct channel_message *msg;
	int pos, first, last, ret, sent_data = 0;
	int success = 1, found = 0;

	/* printf("catch-up: seq=%llu, keep_connected=%d\n", seq, cu->keep_connected); */
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

		ret = cu->wfun(cu->cx, msg->data, msg->data_len);
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
	if(sent_data && (!cu->keep_connected)) {
		http_streaming_end(cu->cx);
		return HTTP_DISCONNECT;
	}
	return HTTP_KEEP_CONNECTED;
}

