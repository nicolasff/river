#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include "http.h"

struct event_base;
struct p_channel;
struct p_channel_user;
struct evbuffer;

struct ws_client {
	struct event ev;

	struct p_channel *chan;
	struct p_channel_user *pcu;

	struct evbuffer *buffer;
};

int
ws_start(struct http_request *req);

int
ws_write(int fd, const char *buf, size_t len);

void
websocket_monitor(struct event_base *base, int fd, struct p_channel *chan,
		struct p_channel_user *pcu);

#endif /* WEBSOCKET_H */
