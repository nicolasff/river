#ifndef COMETD_SOCKET_H
#define COMETD_SOCKET_H

int
socket_setup(const char *ip, short port);

struct connection {
	int fd;
	struct event *ev;
};

void
cx_monitor(struct connection *cx);

void
cx_is_broken(int fd, short event, void *ptr);

#endif /* COMETD_SOCKET_H */

