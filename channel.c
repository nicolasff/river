#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "channel.h"
#include "user.h"
#include "dict.h"

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
channel_add_user(struct p_channel *channel, struct p_user *user) {

	struct p_channel_user *pcu = calloc(1, sizeof(struct p_channel_user));
	pcu->user = user;

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

