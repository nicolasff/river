#ifndef COMETD_SOCKET_H
#define COMETD_SOCKET_H

int
socket_setup(const char *ip, short port);

struct connection {
	int fd;
	struct event *ev;
	struct channel *channel;
	struct channel_user *cu;
};

void
cx_monitor(struct connection *cx);

void
cx_is_broken(int fd, short event, void *ptr);

void
cx_remove(struct connection *cx);

#endif /* COMETD_SOCKET_H */

