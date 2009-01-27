#ifndef COMETD_USER_H
#define COMETD_USER_H

struct evhttp_request;

struct p_user {
	long uid;
	char * sid;
	long seq;
	struct p_connection *connections;
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

