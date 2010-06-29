#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include "http.h"

struct event_base;
struct channel;
struct channel_user;
struct evbuffer;

struct ws_client {
	struct event ev;
	struct event_base *base;

	struct channel *chan; /* current channel the user is connected on */
	struct channel_user *cu; /* channel user using ws:// */

	struct evbuffer *buffer; /* data read so far */
};

int
ws_start(struct connection *cx);

int
ws_write(struct connection *cx, const char *buf, size_t len);

void
ws_close(struct connection *cx);

int
ws_handshake(struct connection *cx, unsigned char *out);

int
ws_client_msg(struct connection *cx);

#endif /* WEBSOCKET_H */
