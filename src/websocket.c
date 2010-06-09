#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <event.h>
#include <string.h>
#include <fcntl.h>
#include <syslog.h>
#include <event.h>
#include <sys/socket.h>
#include <errno.h>
#include <pthread.h>

#include "websocket.h"
#include "channel.h"
#include "server.h"
#include "socket.h"

/**
 * Called when a client connects using websocket, does the handshake.
 */
int
ws_start(struct http_request *req) {

	int ret;
	char *name = NULL;
	char *buffer;
	char template[] = "HTTP/1.1 101 Web Socket Protocol Handshake\r\n"
		"Upgrade: WebSocket\r\n"
		"Connection: Upgrade\r\n"
		"WebSocket-Origin: %s\r\n"
		"WebSocket-Location: ws://%s/websocket?name=%s\r\n"
		"Origin: http://%s\r\n"
		"\r\n";
	size_t sz;
	if(!req->get.name || !req->origin_len || !req->host_len) {
		return -1;
	}

	/* WARNING: this implementation is OLD, and does not use the new handshake.
	 * In future versions, "Web Socket Protocol" will become "WebSocket Protocol"
	 */

	sz = sizeof(template) + req->origin_len + req->host_len * 2 + req->get.name_len
		- (2 + 2 + 2 + 2); /* %s must be removed from the template size */
	buffer = calloc(sz + 1, 1);
	sprintf(buffer, template, req->origin, req->host, name, req->host);
	ret = write(req->cx->fd, buffer, sz);
	free(buffer);

	return ret;
}

/**
 * Sends a message to a websocket client, by a write on a channel
 */
int
ws_write(struct connection *cx, const char *buf, size_t len) {

	int ret;

	char *tmp = malloc(2+len);

	tmp[0] = 0;
	tmp[len+1] = 0xff;
	memcpy(tmp+1, buf, len);

	ret = write(cx->fd, tmp, len+2);

	if(ret != (int)len+2) {
		free(tmp);
		return -1;
	}
	free(tmp);
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
		ws_close(wsc, wsc->cu->cx);
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

			if(sz == 0) { /* no data */
				break;
			}
			if(*data != 0) { /* missing frame start */
				success = 0;
				break;
			}
			last = memchr(data, 0xff, sz);
			if(!last) { /* no end of frame in sight, keep what we have for now. */
				break;
			}
			msg_sz = last - data - 1;
			channel_write(wsc->chan, (const char*)data + 1, msg_sz);

			/* drain including frame delimiters (+2 bytes) */
			evbuffer_drain(wsc->buffer, msg_sz + 2);
		}
	} else {
		success = 0;
	}
	if(success == 0) {
		ws_close(wsc, wsc->cu->cx);
	} else { /* re-add the event only in case of success */
		event_set(&wsc->ev, fd, EV_READ, ws_client_msg, wsc);
		event_base_set(wsc->base, &wsc->ev);
		event_add(&wsc->ev, NULL);
	}
}

/**
 * Shuts down a client
 */
void
ws_close(struct ws_client *wsc, struct connection *cx) {

	CHANNEL_LOCK(wsc->chan);
	cx_remove(cx);
	CHANNEL_UNLOCK(wsc->chan);
	evbuffer_free(wsc->buffer);
	event_del(&wsc->ev);
	free(wsc);
}

/**
 * Start monitoring a fd connected using HTML5 websocket.
 * Creates an event on possible read(2), and add it.
 */
void
websocket_monitor(struct event_base *base, struct connection *cx, struct channel *chan,
		struct channel_user *cu) {

	struct ws_client *wsc = calloc(1, sizeof(struct ws_client));
	wsc->buffer = evbuffer_new();
	wsc->base = base;
	wsc->chan = chan;
	wsc->cu = cu;

	/* set socket as non-blocking. */
	if (0 != fcntl(cx->fd, F_SETFD, O_NONBLOCK)) {
		syslog(LOG_WARNING, "fcntl error: %m\n");
	}

	event_set(&wsc->ev, cx->fd, EV_READ, ws_client_msg, wsc);
	event_base_set(wsc->base, &wsc->ev);
	event_add(&wsc->ev, NULL);
}

