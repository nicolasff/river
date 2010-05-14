#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <event.h>
#include <string.h>
#include <fcntl.h>
#include <syslog.h>
#include <event.h>
#include <sys/socket.h>

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
	char packet[1024], *pos;
	int ret, success = 1;
	struct ws_client *wsc = ptr;

	if(event != EV_READ) {
		return;
	}

	/* read message */
	ret = read(fd, packet, sizeof(packet));
	pos = packet;
	if(ret > 0) {
		unsigned char *data, *last;
		int sz, msg_sz;
		evbuffer_add(wsc->buffer, packet, ret);
		while(1) {
			data = EVBUFFER_DATA(wsc->buffer);
			sz = EVBUFFER_LENGTH(wsc->buffer);

			if(sz == 0) {
				break;
			}
			if(*data != 0) {
				success = 0;
				break;
			}
			last = memchr(data, 0xff, sz);
			if(!last) {
				break;
			}
			msg_sz = last - data - 1;
			channel_write(wsc->chan, (const char*)data + 1, msg_sz);
			evbuffer_drain(wsc->buffer, msg_sz + 2); /* including frame delim. */
		}
	} else {
		success = 0;
	}
	if(success == 0) {
		channel_del_connection(wsc->chan, wsc->pcu);
		evbuffer_free(wsc->buffer);
		event_del(&wsc->ev);
		free(wsc);
		shutdown(fd, SHUT_RDWR);
		close(fd);
	}
}

/**
 * Start monitoring a fd connected using HTML5 websocket.
 * Creates an event on possible read(2), and add it.
 */
void
websocket_monitor(struct event_base *base, int fd, struct p_channel *chan,
		struct p_channel_user *pcu) {

	struct ws_client *wsc = calloc(1, sizeof(struct ws_client));
	wsc->buffer = evbuffer_new();
	wsc->chan = chan;
	wsc->pcu = pcu;

	/* set socket as non-blocking. */
	if (0 != fcntl(fd, F_SETFD, O_NONBLOCK)) {
		syslog(LOG_WARNING, "fcntl error: %m\n");
	}

	event_set(&wsc->ev, fd, EV_READ | EV_PERSIST, ws_client_msg, wsc);
	event_base_set(base, &wsc->ev);
	event_add(&wsc->ev, NULL);
}

