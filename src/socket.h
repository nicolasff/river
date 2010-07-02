#ifndef SOCKET_H
#define SOCKET_H

#include <stdlib.h>

struct event;
struct event_base;
struct channel_user;
struct ws_client;

int
socket_setup(const char *ip, short port);

typedef enum {
	CX_STARTING = 0,
	CX_BROKEN,
	CX_PUBLISHING,
	CX_CONNECTED_COMET,
	CX_CONNECTED_WEBSOCKET,
	CX_SENDING_FILE} cx_state;

struct connection {

	int fd;
	struct event_base *base;
	struct event *ev;

	cx_state state;

	/* http stuff */
	struct {
		char *name; int name_len;
		char *data; int data_len;
		char *jsonp; int jsonp_len;
		char *domain; int domain_len;

		unsigned long long seq; int has_seq;
		long keep;
	} get;

	/* URL */
	char *path;
	size_t path_len;

	char *header_next; /* used in header parsing */
	struct {
		char *host; size_t host_len;
		char *origin; size_t origin_len;

		char *ws1; int ws1_len;
		char *ws2; int ws2_len;

	} headers;

	/* body */
	char *post; int post_len;

	struct channel *channel;
	struct channel_user *cu;
	struct ws_client *wsc;
};

struct connection *
cx_new(int fd, struct event_base *base);

void
cx_remove(struct connection *cx);

#endif
