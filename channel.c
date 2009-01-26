#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "channel.h"
#include "user.h"

#include <glib.h>

GHashTable *__channels = NULL;

void
channel_init() {
	if(NULL == __channels) {
		__channels = g_hash_table_new_full(NULL, NULL, NULL, NULL);
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

	g_hash_table_insert(__channels, GINT_TO_POINTER(g_str_hash(name)),
			channel);

	return channel;
}

struct p_channel *
channel_find(const char *name) {

	return g_hash_table_lookup(__channels, 
			GINT_TO_POINTER(g_str_hash(name)));
}

void
channel_free(struct p_channel * p) {

	free(p->name);
	free(p);
}

/* TODO: make this faster than O(n) */
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

