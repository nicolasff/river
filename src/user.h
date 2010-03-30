#ifndef COMETD_USER_H
#define COMETD_USER_H

#define USER_LOCK(u)(pthread_mutex_lock(&u->lock))
#define USER_UNLOCK(u)(pthread_mutex_unlock(&u->lock))

struct p_message;

/**
 * A user, with uid + sid + lock.
 */
struct p_user {
	int fd;

	long uid; /* user id, unique to this user */
	char * sid; /* session id */

	char *payload; 	/* custom payload for users */
	size_t payload_len;

	pthread_mutex_t lock;
};

void
user_init();

struct p_user *
user_new(long uid, const char *sid);

void
user_save(struct p_user *);

struct p_user *
user_find(long uid);

void
user_free(long uid);

#endif /* COMETD_USER_H */

