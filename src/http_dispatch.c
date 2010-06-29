#include <string.h>
#include <event.h>
#include <stdio.h>

#include "http_dispatch.h"
#include "socket.h"
#include "http.h"
#include "channel.h"
#include "websocket.h"
#include "files.h"

static int
start_fun_http(struct connection *cx) {
	http_streaming_start(cx, 200, "OK");
	return 0;
}

/**
 * Dispatch based on the path
 */
http_action
http_dispatch(struct connection *cx) {

	if(cx->path_len == 8 && 0 == strncmp(cx->path, "/publish", 8)) {
		cx->state = CX_PUBLISHING;
		return http_dispatch_publish(cx);
	} else if(cx->path_len == 10 && 0 == strncmp(cx->path, "/subscribe", 10)) {
		cx->state = CX_CONNECTED_COMET;
		return http_dispatch_read(cx, start_fun_http, http_streaming_chunk);
	} else if(cx->path_len == 10 && 0 == strncmp(cx->path, "/websocket", 10)) {
		cx->state = CX_CONNECTED_WEBSOCKET;
		if(HTTP_KEEP_CONNECTED == http_dispatch_read(cx, ws_start, ws_write)) {
			return HTTP_WEBSOCKET_MONITOR;
		}
		return HTTP_DISCONNECT;
	} else if(file_send(cx) == 0) { /* check if we're sending a file. */
		cx->state = CX_SENDING_FILE;
		return HTTP_DISCONNECT;
	}

	cx->state = CX_BROKEN;
	send_empty_reply(cx, 404);
	return HTTP_DISCONNECT;
}

/**
 * Perform a read on the channel, with two callback functions.
 *
 * @param start_fun is called when the client is allowed to connect.
 * @param write_fun is called to write data to the client.
 */
http_action
http_dispatch_read(struct connection *cx, start_function start_fun, write_function write_fun) {

	http_action ret = HTTP_KEEP_CONNECTED;

	if(!cx->get.name) {
		send_empty_reply(cx, 400);
		return HTTP_DISCONNECT;
	}

	/* find channel */
	if(!(cx->channel = channel_find(cx->get.name))) {
		cx->channel = channel_new(cx->get.name);
	}

	CHANNEL_LOCK(cx->channel);
	cx->cu = channel_new_connection(cx, cx->get.keep, cx->get.jsonp, write_fun);
	if(-1 == start_fun(cx)) {
		CHANNEL_UNLOCK(cx->channel);
		return HTTP_DISCONNECT;
	}

	/* 3 cases:
	 *
	 * 1 - catch-up followed by close
	 * 2 - catch-up followed by stay connected
	 * 3 - connect and stay connected
	 **/

	/* chan is locked, check if we need to catch-up */
	if(cx->get.has_seq && cx->get.seq < cx->channel->seq) {
		ret = channel_catchup_user(cx->channel, cx->cu, cx->get.seq);
		/* case 1 */
		if(ret == HTTP_DISCONNECT) {
			CHANNEL_UNLOCK(cx->channel);
			return HTTP_DISCONNECT;
		} else {
			/* case 2*/
		}
	} else {
		/* case 3 */
	}

	/* stay connected: add cu to channel. */
	channel_add_connection(cx->channel, cx->cu);

	/* add timeout to avoid keeping the user for too long. */
#if 0
	if(__cfg->client_timeout > 0) {
		struct user_timeout *ut;

		cx->cu->free_on_remove = 0;
		CHANNEL_UNLOCK(cx->channel);

		ut = calloc(1, sizeof(struct user_timeout));
		ut->cu = cx->cu;
		ut->channel = cx->channel;

		/* timeout value. */
		ut->tv.tv_sec = __cfg->client_timeout;
		ut->tv.tv_usec = 0;

		/* add timeout event */
		timeout_set(&ut->ev, on_client_too_old, ut);
		event_base_set(cx->base, &ut->ev);
		timeout_add(&ut->ev, &ut->tv);
	} else {
#endif
		CHANNEL_UNLOCK(cx->channel);
#if 0
	}
#endif

	return ret;
}

/**
 * Publishes a message in a channel.
 *
 * Parameters: name (channel name), data.
 */
http_action
http_dispatch_publish(struct connection *cx) {
	struct channel *channel;

	if(!cx->get.name || !cx->get.data) {
		send_empty_reply(cx, 403);
		return HTTP_DISCONNECT;
	}

	/* find channel */
	if(!(channel = channel_find(cx->get.name))) {
		send_empty_reply(cx, 200); /* pretend we just did. */
		return HTTP_DISCONNECT;
	}

	send_empty_reply(cx, 200);

	/* send to all channel users. */
	channel_write(channel, cx->get.data, cx->get.data_len);

	return HTTP_DISCONNECT;
}



