#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <pthread.h>

#include "user.h"

/**
 * Static table containing all users.
 */
static GHashTable *__users = NULL;
pthread_mutex_t users_mutex;

struct p_user *
user_new(long uid, const char * sid) {

	struct p_user *user = calloc(1, sizeof(struct p_user));
	if(NULL == user) {
		return NULL;
	}

	user->uid = uid;
	user->sid = strdup(sid);
	user->seq = 1;

	return user;
}

void
user_init() {

	if(NULL == __users) {
		__users = g_hash_table_new_full(NULL, NULL, NULL, NULL);
		pthread_mutex_init(&users_mutex, NULL);
	}
}

void
user_save(struct p_user *p) {
	pthread_mutex_lock(&users_mutex);
	g_hash_table_insert(__users, GINT_TO_POINTER(p->uid), p);
	pthread_mutex_unlock(&users_mutex);
}

/**
 * Find a user from his UID.
 */
struct p_user *
user_find(long uid) {

	gpointer key = GINT_TO_POINTER(uid);
	return (struct p_user*)g_hash_table_lookup(__users, key);

}


