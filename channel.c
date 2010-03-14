#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "channel.h"
#include "user.h"
#include "dict.h"
#include "http.h"
#include "json.h"

#include <unistd.h>
#include <pthread.h>

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

	/* users in the channel */
	channel->users = dictCreate(&dictTypeIntCopyNoneFreeNone, NULL);

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

int
channel_has_user(struct p_channel *channel, struct p_user *user) {

	dictEntry *de;
	if((de = dictFind(channel->users, (void*)user->uid))) {
		return 1;
	}
	return 0;
}

int
channel_add_user(struct p_channel *channel, struct p_user *user, int fd) {

	struct p_channel_user *pcu = calloc(1, sizeof(struct p_channel_user));
	pcu->user = user;
	pcu->fd = fd;

	CHANNEL_LOCK(channel);
	if(DICT_OK == dictAdd(channel->users,
				(void*)user->uid, pcu, 0)) {

		/* add user to the front of the list */
		if(channel->user_list) {
			channel->user_list->prev = pcu;
		}
		pcu->next = channel->user_list;
		channel->user_list = pcu;

		CHANNEL_UNLOCK(channel);
		return 0;
	}
	free(pcu);
	CHANNEL_UNLOCK(channel);
	return -1;
}

void
channel_del_user(struct p_channel *channel, long uid) {

	struct p_channel_user *pcu = NULL;
	dictEntry *de;
	/* lock */
	CHANNEL_LOCK(channel);
	if((de = dictFind(channel->users, (void*)uid))) {

		pcu = (struct p_channel_user *)de->val;

		if(pcu->prev) {
			pcu->prev->next = pcu->next;
		} else {
			channel->user_list = channel->user_list->next;
			channel->user_list->prev = NULL;
		}
		dictDelete(channel->users, (void*)uid);
	}

	/* unlock chan & free */
	CHANNEL_UNLOCK(channel);
	user_free(uid);
	free(pcu);
}

void
channel_write(struct p_channel *channel, long uid, const char *data, size_t data_len) {

	char *json;
	size_t sz;
	struct p_channel_user *pcu;
	struct p_channel_message *msg;
	msg = calloc(1, sizeof(struct p_channel_message));

	/* timestamp */
	msg->ts = time(NULL);

	/* copy data */
	msg->data = calloc(data_len, 1);
	memcpy(msg->data, data, data_len);
	msg->data_len = data_len;
	json = json_msg(channel->name, uid, msg->ts, msg->data, &sz);

	CHANNEL_LOCK(channel);
	/* save log */
	msg->next = channel->log;
	channel->log = msg;
	msg->uid = uid;

	/* push message to connected users */
	for(pcu = channel->user_list; pcu; pcu = pcu->next) {

		/* write message to connected user */
		int ret = http_streaming_chunk(pcu->fd, json, sz);
		if(ret != (int)sz) { /* failed write */
			/* TODO: check that everything is cleaned. */
			close(pcu->fd);
			// channel_del_user(channel, pcu->user->uid);
		}
	}
	CHANNEL_UNLOCK(channel);
	free(json);
}

/* TODO: this is the wrong order. fix it. */
void
channel_catchup_user(struct p_channel *channel, int fd, time_t timestamp) {

	struct p_channel_message *msg;

	http_streaming_chunk(fd, "[", 1);
	for(msg = channel->log; msg && msg->ts > timestamp; msg = msg->next) {
		size_t sz;
		char *json = json_msg(channel->name, msg->uid, msg->ts, msg->data, &sz);
		http_streaming_chunk(fd, json, sz);
		free(json);
		if(msg->next) {
			http_streaming_chunk(fd, ", ", 2);
		}
		/* TODO: check return value for each call. */
	}
	http_streaming_chunk(fd, "]", 1);
}

