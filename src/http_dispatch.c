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
	http_streaming_start(req->fd, 200, "OK");
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
	http_streaming_end(ut->pcu->fd);

	/* remove from channel */
	CHANNEL_LOCK(ut->channel);
	channel_del_connection(ut->channel, ut->pcu);
	CHANNEL_UNLOCK(ut->channel);
	free(ut->pcu);
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

	int keep_connected = 1, has_seq = 0;
	unsigned long long seq = 0;
	char *name = NULL;
	char *jsonp = NULL;
	dictEntry *de;

	if(!req->get) {
		send_empty_reply(req, 400);
		return HTTP_DISCONNECT;
	}

	if((de = dictFind(req->get, "name"))) {
		name = de->val;
	}
	if((de = dictFind(req->get, "seq"))) { /* optional */
		seq = atol(de->val);
		has_seq = 1;
	}
	if((de = dictFind(req->get, "keep"))) { /* optional */
		keep_connected = atol(de->val);
	}
	if((de = dictFind(req->get, "callback"))) { /* optional */
		jsonp = de->val;
	}

	if(!name) {
		send_empty_reply(req, 400);
		return HTTP_DISCONNECT;
	}

	/* find channel */
	if(!(req->channel = channel_find(name))) {
		req->channel = channel_new(name);
	}

	req->pcu = channel_new_connection(req->fd, keep_connected, jsonp, write_fun);
	start_fun(req);

	/* 3 cases:
	 *
	 * 1 - catch-up followed by close
	 * 2 - catch-up followed by stay connected
	 * 3 - connect and stay connected
	 **/

	CHANNEL_LOCK(req->channel);
	/* chan is locked, check if we need to catch-up */
	if(has_seq && seq < req->channel->seq) {
		ret = channel_catchup_user(req->channel, req->pcu, seq);
		/* case 1 */
		if(ret == HTTP_DISCONNECT) {
			free(req->pcu->jsonp);
			free(req->pcu);
			CHANNEL_UNLOCK(req->channel);
			return HTTP_DISCONNECT;
		} else {
			/* case 2*/
		}
	} else {
		/* case 3 */
	}

	/* stay connected: add pcu to channel. */
	channel_add_connection(req->channel, req->pcu);

	/* add timeout to avoid keeping the user for too long. */
	if(__cfg->client_timeout > 0) {
		struct user_timeout *ut;

		req->pcu->free_on_remove = 0;
		CHANNEL_UNLOCK(req->channel);

		ut = calloc(1, sizeof(struct user_timeout));
		ut->pcu = req->pcu;
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

	struct p_channel *channel;

	dictEntry *de;
	char *name = NULL, *data = NULL;
	size_t data_len = 0;

	if((de = dictFind(req->get, "name"))) {
		name = de->val;
	}
	if((de = dictFind(req->get, "data"))) {
		data = de->val;
		data_len = de->size;
	}
	if(!name || !data) {
		send_empty_reply(req, 403);
		return HTTP_DISCONNECT;
	}

	/* find channel */
	if(!(channel = channel_find(name))) {
		send_empty_reply(req, 200); /* pretend we just did. */
		return HTTP_DISCONNECT;
	}

	send_empty_reply(req, 200);

	/* send to all channel users. */
	channel_write(channel, data, data_len);

	return HTTP_DISCONNECT;
}

