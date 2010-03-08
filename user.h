#ifndef COMETD_USER_H
#define COMETD_USER_H

struct p_message;

/**
 * A user, with uid + sid + lock.
 */
struct p_user {
	int fd;

	long uid; /* user id, unique to this user */
	char * sid; /* session id */
	struct p_message *inbox;

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

#endif /* COMETD_USER_H */

