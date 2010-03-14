#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "user.h"
#include "dict.h"

/**
 * Static table containing all users.
 */
static dict *__users = NULL;
pthread_mutex_t users_mutex;

struct p_user *
user_new(long uid, const char * sid) {

	struct p_user *user = calloc(1, sizeof(struct p_user));
	if(NULL == user) {
		return NULL;
	}

	user->uid = uid;
	user->sid = strdup(sid);

	return user;
}

void
user_init() {

	if(NULL == __users) {
		__users = dictCreate(&dictTypeIntCopyNoneFreeNone, NULL);
		pthread_mutex_init(&users_mutex, NULL);
	}
}

void
user_save(struct p_user *p) {
	pthread_mutex_lock(&users_mutex);
	dictAdd(__users, (void*)p->uid, p, 0);
	pthread_mutex_unlock(&users_mutex);
}

/**
 * Find a user from his UID.
 */
struct p_user *
user_find(long uid) {

	dictEntry *de;
	if((de = dictFind(__users, (void*)uid))) {
		return (struct p_user*)de->val;
	}
	return NULL;
}


void
user_free(long uid) {
	struct p_user *p;
	p = user_find(uid);

	pthread_mutex_lock(&users_mutex);
	dictDelete(__users, (void*)uid);
	pthread_mutex_unlock(&users_mutex);

	if(p) {
		free(p->sid);
		free(p);
	}
}

