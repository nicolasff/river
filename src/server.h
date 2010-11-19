#ifndef SERVER_H
#define SERVER_H

#include <event.h>

struct channel;

struct cleanup_timer {
	struct event ev;
	struct event_base *base;
	struct timeval tv;
};

void
server_run(int fd, int max_connections);

void
cb_available_client_data(int fd, short event, void *ptr);

void
on_possible_accept(int fd, short event, void *ptr);

void
on_available_data(int fd, short event, void *ptr);

void
on_channel_cleanup(int fd, short event, void *ptr);

void
cleanup_reset(struct cleanup_timer *ct);

#endif
