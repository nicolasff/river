#ifndef CONF_H
#define CONF_H


struct conf {

	char *ip;
	short port;

	char *domain;
	char *parent_domain;

	char *channel_key;

	int threads;
};

struct conf *
conf_read(const char *filename);

void
conf_free(struct conf *conf);

#endif /* CONF_H */
