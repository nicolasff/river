#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <event.h>
#include <string.h>
#include <fcntl.h>
#include <syslog.h>

#include "websocket.h"
#include "channel.h"

/**
 * Called when a client connects using websocket, does the handshake.
 */
int
ws_start(struct http_request *req) {

	char *name = NULL;
	dictEntry *de;
	if((de = dictFind(req->get, "name"))) {
		name = de->val;
	}
	if(!name || !req->origin_len || !req->host_len) {
		return -1;
	}

	return dprintf(req->fd, 
		"HTTP/1.1 101 Web Socket Protocol Handshake\r\n"
		"Upgrade: WebSocket\r\n"
		"Connection: Upgrade\r\n"
		"WebSocket-Origin: %s\r\n"
		"WebSocket-Location: ws://%s/websocket?name=%s\r\n"
		"Origin: http://%s\r\n"
		"\r\n",

		req->origin, req->host, name, req->host);
}

/**
 * Sends a message to a websocket client, by a write on a channel
 */
int
ws_write(int fd, const char *buf, size_t len) {

	if(write(fd, "\x00", 1) != 1) { /* frame starts with \x00 */
		return -1;
	}
	if(write(fd, buf, len) != (int)len) {
		return -1;
	}
	if(write(fd, "\xff", 1) != 1) { /* frame ends with \xff */
		return -1;
	}
	/* we're expected to write only `len' bytes. the rest is protocol-specific. */
	return len;
}

/**
 * Called when we received a message from a websocket client.
 */
static void
ws_client_msg(int fd, short event, void *ptr) {
	char buffer[1024];
	int ret;
	struct p_channel *chan = ptr;

	if(event != EV_READ) {
		return;
	}

	/* read message */
	ret = read(fd, buffer, sizeof(buffer));
	if(*buffer == 0 && ret >= 2) { /* frame starts with \x00 */
		char *last;
		if((last = strchr(buffer + 1, 0xff))) { /* frame ends with \xff */
			/* publish to chan. */
			channel_write(chan, buffer + 1, last - buffer - 1);
		}
	}
}

/**
 * Start monitoring a fd connected using HTML5 websocket.
 * Creates an event on possible read(2), and add it.
 */
void
websocket_monitor(struct event_base *base, int fd, struct p_channel *chan) {

	struct event *ev = calloc(1, sizeof(struct event));

	/* set socket as non-blocking. */
	if (0 != fcntl(fd, F_SETFD, O_NONBLOCK)) {
		syslog(LOG_WARNING, "fcntl error: %m\n");
	}

	event_set(ev, fd, EV_READ | EV_PERSIST, ws_client_msg, chan);
	event_base_set(base, ev);
	event_add(ev, NULL);
}

