#ifndef COMETD_CHANNEL_H
#define COMETD_CHANNEL_H

#define CHANNEL_LOCK(c)(pthread_mutex_lock(&c->lock))
#define CHANNEL_UNLOCK(c)(pthread_mutex_unlock(&c->lock))

struct p_user;

struct p_channel_user {
	struct p_user *user;
	int fd;
	int keep_connected;

	struct p_channel_user *prev;
	struct p_channel_user *next;
};

struct p_channel_message {

	unsigned long long seq; /* sequence number */

	char *data; /* message contents */
	size_t data_len;

	long uid; /* producer */
};

struct p_channel {
	char *name;

	unsigned long long seq;

	struct p_channel_user *user_list;

	struct p_channel_message *log_buffer;
	int log_pos;

	pthread_mutex_t lock;
};

void
channel_init(char *key) ;

struct p_channel *
channel_new(const char *name);

void
channel_free(struct p_channel *);

struct p_channel * 
channel_find(const char *name);

struct p_channel_user *
channel_add_connection(struct p_channel *, struct p_user *, int fd, int keep_connected) ;

void
channel_del_connection(struct p_channel *channel, struct p_channel_user *pcu);

void
channel_write(struct p_channel *channel, long uid, const char *data, size_t data_len);

int
channel_catchup_user(struct p_channel *channel, struct p_channel_user *pcu, unsigned long long seq);

#endif /* COMETD_CHANNEL_H */

