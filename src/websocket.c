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
#include <stdio.h>
#include <arpa/inet.h>

#include "websocket.h"
#include "channel.h"
#include "server.h"
#include "socket.h"
#include "md5.h"

/**
 * Called when a client connects using websocket, does the handshake.
 */
int
ws_start(struct connection *cx) {

	int ret;
	char *buffer;
	
	unsigned char handshake[16];

	char template[] = "HTTP/1.1 101 Websocket Protocol Handshake\r\n"
		"Upgrade: WebSocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Origin: %s\r\n"
		"Sec-WebSocket-Location: ws://%s/websocket?name=%s\r\n"
		"Origin: http://%s\r\n"
		"\r\n";
	size_t sz;
	if(!cx->get.name || !cx->headers.origin_len || !cx->headers.host_len
		|| !cx->headers.ws1_len || !cx->headers.ws2_len) {
		return -1;
	}

	if(ws_handshake(cx, &handshake[0]) != 0) {
		return -1;
	}

	/* This code uses the WebSocket specification from May 23, 2010.
	 * The latest copy is available at http://www.whatwg.org/specs/web-socket-protocol/
	 */
	sz = sizeof(template) + cx->headers.origin_len + cx->headers.host_len * 2
		+ cx->get.name_len
		+ sizeof(handshake) - 1
		- (2 + 2 + 2 + 2); /* %s must be removed from the template size */

	buffer = calloc(sz, 1);
	sprintf(buffer, template, cx->headers.origin, cx->headers.host,
			cx->get.name, cx->headers.host);
	memcpy(buffer + sz - sizeof(handshake), handshake, sizeof(handshake));
	ret = write(cx->fd, buffer, sz);
	free(buffer);

	struct ws_client *wsc = calloc(1, sizeof(struct ws_client));
	wsc->buffer = evbuffer_new();
	cx->wsc = wsc;

	return ret;
}

static uint32_t
ws_read_key(char *p) {
	uint32_t ret = 0, spaces = 0;
	for(; *p; ++p) {
		if(*p >= '0' && *p <= '9') {
			ret *= 10;
			ret += (*p)  - '0';
		} else if (*p == ' ') {
			spaces++;
		}
	}
	return htonl(ret / spaces);
}

int
ws_handshake(struct connection *cx, unsigned char *out) {
	char buffer[16];
	md5_state_t ctx;

	// websocket handshake
	uint32_t number_1 = ws_read_key(cx->headers.ws1);
	uint32_t number_2 = ws_read_key(cx->headers.ws2);

	if(cx->post_len != 8) {
		return -1;
	}

	memcpy(buffer, &number_1, sizeof(uint32_t));
	memcpy(buffer + sizeof(uint32_t), &number_2, sizeof(uint32_t));
	memcpy(buffer + 2 * sizeof(uint32_t), cx->post, cx->post_len);
	
	md5_init(&ctx);
	md5_append(&ctx, (const md5_byte_t *)buffer, sizeof(buffer));
	md5_finish(&ctx, out);

	return 0;
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
int
ws_client_msg(struct connection *cx) {
	char packet[1024], *pos;
	int ret, success = 1;

	/* read message */
	ret = read(cx->fd, packet, sizeof(packet));
	if(ret <= 0) {
		/* ws_close(cx); */
		return -1;
	}
	pos = packet;
	unsigned char *data, *last;
	int sz, msg_sz;
	evbuffer_add(cx->wsc->buffer, packet, ret);

	while(1) {
		data = EVBUFFER_DATA(cx->wsc->buffer);
		sz = EVBUFFER_LENGTH(cx->wsc->buffer);

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
		channel_write(cx->channel, (const char*)data + 1, msg_sz);

		/* drain including frame delimiters (+2 bytes) */
		evbuffer_drain(cx->wsc->buffer, msg_sz + 2);
	}
	return 0;
}

