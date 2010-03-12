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
	pthread_mutex_init(&channel->lock, NULL);

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

/* TODO: make this faster than O(n), by using a uid → user HT in p_channel. */
int
channel_has_user(struct p_channel *channel, struct p_user *user) {

	struct p_channel_user *cu;
	for(cu = channel->users; cu; cu = cu->next) {
		if(user->uid == cu->uid) {
			return 1;
		}
	}
	return 0;
}

int
channel_add_user(struct p_channel *channel, struct p_user *user) {

	struct p_channel_user *pu;
	
	if(NULL == channel || NULL == user) {
		return -1;
	}

	pu = calloc(1, sizeof(struct p_channel_user));
	if(NULL == pu) {
		return -1;
	}

	pu->uid = user->uid;
	pu->next = channel->users;
	channel->users = pu;
	
	return 0;
}

void
channel_del_user(struct p_channel *channel, long uid) {

	struct p_channel_user *pu, *prev = NULL;

	/* TODO: better than this. */
	pthread_mutex_lock(&channel->lock);
	for(pu = channel->users; pu; pu = pu->next) {

		if(pu && pu->uid) {
			if(prev) {
				prev->next = pu->next;
			} else {
				channel->users = channel->users->next;
			}
			user_free(uid);
		}
		prev = pu;
	}
	pthread_mutex_unlock(&channel->lock);
}

