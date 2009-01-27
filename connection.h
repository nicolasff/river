#ifndef COMETD_CONNECTION_H
#define COMETD_CONNECTION_H

struct evhttp_request;
struct p_user;

struct p_connection {

	int fd;
	struct evhttp_request *ev;
	struct p_user *user;

	/* list of messages for this connection */
	struct p_message *inbox_first;
	struct p_message *inbox_last;

	struct p_connection *next;
};

struct p_connection *
connection_new(int fd, struct evhttp_request *ev);

void
connection_free(struct p_connection *);

void
connection_free_r(struct p_connection *);

#endif /* COMETD_CONNECTION_H */

