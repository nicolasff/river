#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>

#include "user.h"

static GHashTable *__users = NULL;

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
	}
}

void
user_save(struct p_user *p) {
	g_hash_table_insert(__users, GINT_TO_POINTER(p->uid), p);
}

struct p_user *
user_find(long uid) {

	gpointer key = GINT_TO_POINTER(uid);
	return (struct p_user*)g_hash_table_lookup(__users, key);

}


