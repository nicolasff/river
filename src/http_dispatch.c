#include <sys/queue.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <event.h>
#include <fcntl.h>
#include <sys/types.h>


#include "http_dispatch.h"
#include "websocket.h"
#include "files.h"
#include "channel.h"
#include "server.h"
#include "message.h"
#include "http.h"
#include "dict.h"
#include "conf.h"

static struct conf *__cfg;

void
http_init(struct conf *cfg) {

	/* eww. */
	__cfg = cfg;
}

static int
start_fun_http(struct http_request *req) {
	http_streaming_start(req->cx, 200, "OK");
	return 0;
}

/**
 * Dispatch based on the path
 */
http_action
http_dispatch(struct http_request *req) {

	if(req->path_len == 8 && 0 == strncmp(req->path, "/publish", 8)) {
		return http_dispatch_publish(req);
	} else if(req->path_len == 10 && 0 == strncmp(req->path, "/subscribe", 10)) {
		return http_dispatch_read(req, start_fun_http, http_streaming_chunk);
	} else if(req->path_len == 10 && 0 == strncmp(req->path, "/websocket", 10)) {
		if(HTTP_KEEP_CONNECTED == http_dispatch_read(req, ws_start, ws_write)) {
			return HTTP_WEBSOCKET_MONITOR;
		}
		return HTTP_DISCONNECT;

	} else if(file_send(req) == 0) { /* check if we're sending a file. */
		return HTTP_DISCONNECT;
	}

	send_empty_reply(req, 404);
	return HTTP_DISCONNECT;
}

void
on_client_too_old(int fd, short event, void *arg) {

	(void)fd;
	struct user_timeout *ut = arg;
	if(event != EV_TIMEOUT) {
		return;
	}
	/* disconnect user */
	http_streaming_end(ut->cu->cx);

	/* remove from channel */
	CHANNEL_LOCK(ut->channel);
	channel_del_connection(ut->channel, ut->cu);
	CHANNEL_UNLOCK(ut->channel);
	free(ut->cu);
	free(ut);
}

/**
 * Perform a read on the channel, with two callback functions.
 *
 * @param start_fun is called when the client is allowed to connect.
 * @param write_fun is called to write data to the client.
 */
http_action
http_dispatch_read(struct http_request *req, start_function start_fun, write_function write_fun) {

	http_action ret = HTTP_KEEP_CONNECTED;
	/* printf("read!\n"); */

	if(!req->get.name) {
		send_empty_reply(req, 400);
		return HTTP_DISCONNECT;
	}

	/* find channel */
	if(!(req->channel = channel_find(req->get.name))) {
		req->channel = channel_new(req->get.name);
	}

	/* printf("locking channel...\n"); */
	CHANNEL_LOCK(req->channel);
	/* printf("done.\n"); */
	req->cu = channel_new_connection(req->cx, req->get.keep, req->get.jsonp, write_fun);
	if(-1 == start_fun(req)) {
		CHANNEL_UNLOCK(req->channel);
		return HTTP_DISCONNECT;
	}

	/* 3 cases:
	 *
	 * 1 - catch-up followed by close
	 * 2 - catch-up followed by stay connected
	 * 3 - connect and stay connected
	 **/

	/* chan is locked, check if we need to catch-up */
	/* printf("req->has_seq = %d, req->seq=%llu, channel->seq=%llu\n", req->get.has_seq, req->get.seq, req->channel->seq); */
	if(req->get.has_seq && req->get.seq < req->channel->seq) {
		/* printf("catch-up\n"); */
		ret = channel_catchup_user(req->channel, req->cu, req->get.seq);
		/* case 1 */
		if(ret == HTTP_DISCONNECT) {
			/* printf("disconnect\n"); */
			free(req->cu->jsonp);
			free(req->cu);
			CHANNEL_UNLOCK(req->channel);
			return HTTP_DISCONNECT;
		} else {
			/* printf("stay connected\n"); */
			/* case 2*/
		}
	} else {
		/* case 3 */
	}

	/* stay connected: add cu to channel. */
	channel_add_connection(req->channel, req->cu);

	/* add timeout to avoid keeping the user for too long. */
	if(__cfg->client_timeout > 0) {
		struct user_timeout *ut;

		req->cu->free_on_remove = 0;
		CHANNEL_UNLOCK(req->channel);

		ut = calloc(1, sizeof(struct user_timeout));
		ut->cu = req->cu;
		ut->channel = req->channel;

		/* timeout value. */
		ut->tv.tv_sec = __cfg->client_timeout;
		ut->tv.tv_usec = 0;

		/* add timeout event */
		timeout_set(&ut->ev, on_client_too_old, ut);
		event_base_set(req->base, &ut->ev);
		timeout_add(&ut->ev, &ut->tv);
	} else {
		CHANNEL_UNLOCK(req->channel);
	}

	return ret;
}

/**
 * Publishes a message in a channel.
 *
 * Parameters: name (channel name), data.
 */
http_action
http_dispatch_publish(struct http_request *req) {

	struct channel *channel;

	if(!req->get.name || !req->get.data) {
		send_empty_reply(req, 403);
		return HTTP_DISCONNECT;
	}

	/* find channel */
	if(!(channel = channel_find(req->get.name))) {
		send_empty_reply(req, 200); /* pretend we just did. */
		return HTTP_DISCONNECT;
	}
	/* printf("here\n"); */

	send_empty_reply(req, 200);

	/* send to all channel users. */
	channel_write(channel, req->get.data, req->get.data_len);

	return HTTP_DISCONNECT;
}

