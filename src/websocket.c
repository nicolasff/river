#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "websocket.h"

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
 * Called to send a message to a websocket client.
 */
int
ws_write(int fd, const char *buf, size_t len) {

	if(write(fd, "\x00", 1) != 1) {
		return -1;
	}
	if(write(fd, buf, len) != (int)len) {
		return -1;
	}
	if(write(fd, "\xff", 1) != 1) {
		return -1;
	}
	return len; /* we're expected to write only `len' bytes. the rest is protocol-specific. */
}
