#include <sys/queue.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <event.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>


#include "http_dispatch.h"
#include "channel.h"
#include "server.h"
#include "message.h"
#include "http.h"
#include "dict.h"
#include "conf.h"

static struct conf *__cfg;

static char *iframe_buffer = NULL;
static size_t iframe_buffer_len = -1;

void
http_init(struct conf *cfg) {

	struct stat st;
	int ret, fp;
	size_t remain;
	const char filename[] = "iframe.js";
	/* eww. */
	__cfg = cfg;

	ret = stat(filename, &st);
	if(ret != 0) {
		return;
	}
	iframe_buffer_len = st.st_size;
	if(!(iframe_buffer = calloc(iframe_buffer_len, 1))) {
		iframe_buffer_len = -1;
		return;
	}
	remain = iframe_buffer_len;
	fp = open(filename, O_RDONLY);
	while(remain) {
		int count = read(fp, iframe_buffer + iframe_buffer_len - remain, remain);
		if(count <= 0) {
			free(iframe_buffer);
			iframe_buffer_len = -1;
			return;
		}
		remain -= count;
	}
}

static void
send_reply(struct http_request *req, int error) {

	switch(error) {
		case 200:
			http_response(req->fd, 200, "OK", "ok", 2);
			break;

		case 404:
			http_response(req->fd, 404, "Not found", "", 0);
			break;

		case 403:
			http_response(req->fd, 403, "Forbidden", "", 0);
			break;
	}
}

/**
 * Dispatch based on the path
 */
int
http_dispatch(struct http_request *req) {

	if(req->path_len == 8 && 0 == strncmp(req->path, "/publish", 8)) {
		return http_dispatch_publish(req);
	} else if(req->path_len == 10 && 0 == strncmp(req->path, "/subscribe", 10)) {
		return http_dispatch_subscribe(req);
	} else if(req->path_len == 7 && 0 == strncmp(req->path, "/iframe", 7)) {
		return http_dispatch_iframe(req);
	}

	return 0;
}


/**
 * Return generic page for iframe inclusion
 */
int
http_dispatch_iframe(struct http_request *req) {

	dictEntry *de;

	char buffer_start[] = "<html><body><script type=\"text/javascript\">\ndocument.domain=\"";
	char buffer_domain[] = "\";\n";
	char buffer_end[] = "</script></body></html>\n";

	http_streaming_start(req->fd, 200, "OK");
	http_streaming_chunk(req->fd, buffer_start, sizeof(buffer_start)-1);
	if(req->get && (de = dictFind(req->get, "domain"))) {
		http_streaming_chunk(req->fd, de->val, de->size);
	}
	http_streaming_chunk(req->fd, buffer_domain, sizeof(buffer_domain)-1);

	/* iframe.js */
	http_streaming_chunk(req->fd, iframe_buffer, iframe_buffer_len);

	http_streaming_chunk(req->fd, buffer_end, sizeof(buffer_end)-1);
	http_streaming_end(req->fd);
	
	return 0;
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
	free(ut);
}

/**
 * Connect user to a channel. This call is blocking, waiting for new data.
 * 
 * Parameters: name, [seq], [keep]
 */
int
http_dispatch_subscribe(struct http_request *req) {

	int ret = 1;
	struct p_channel *channel = NULL;

	int keep_connected = 1, has_seq = 0;
	unsigned long long seq = 0;
	char *name = NULL;
	dictEntry *de;

	if((de = dictFind(req->get, "name"))) {
		name = de->val;
	}
	if((de = dictFind(req->get, "seq"))) { /* optional */
		seq = atol(de->val);
		has_seq = 1;
	}
	if((de = dictFind(req->get, "keep"))) { /* optional */
		/* printf("has keep para: [%s]\n", de->val); */
		keep_connected = atol(de->val);
	}

	/* find channel */
	if(!(channel = channel_find(name))) {
		channel = channel_new(name);
	}

	struct p_channel_user *pcu;
	pcu = channel_new_connection(req->fd, keep_connected);
	http_streaming_start(req->fd, 200, "OK");

	/* 3 cases:
	 *
	 * 1 - catch-up followed by close
	 * 2 - catch-up followed by stay connected
	 * 3 - connect and stay connected
	 **/

	CHANNEL_LOCK(channel);
	/* chan is locked, check if we need to catch-up */
	if(has_seq && seq < channel->seq) {
		ret = channel_catchup_user(channel, pcu, seq);
		/* case 1 */
		if(ret == 0) {
			free(pcu);
			CHANNEL_UNLOCK(channel);
			return 0;
		} else {
			/* case 2*/
		}
	} else {
		/* case 3 */
	}

	/* stay connected: add pcu to channel. */
	channel_add_connection(channel, pcu);
	CHANNEL_UNLOCK(channel);

	/* add timeout to avoid keeping the user for too long. */
	if(__cfg->client_timeout > 0) {
		struct user_timeout *ut;

		ut = calloc(1, sizeof(struct user_timeout));
		ut->pcu = pcu;
		ut->channel = channel;

		/* timeout value. */
		ut->tv.tv_sec = __cfg->client_timeout;
		ut->tv.tv_usec = 0;

		/* add timeout event */
		timeout_set(&ut->ev, on_client_too_old, ut);
		event_base_set(req->base, &ut->ev);
		timeout_add(&ut->ev, &ut->tv);
	}

	return ret;
}

/**
 * Publishes a message in a channel.
 *
 * Parameters: name (channel name), data.
 */
int
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
		send_reply(req, 403);
		return 0;
	}

	/* find channel */
	if(!(channel = channel_find(name))) {
		send_reply(req, 403);
		return 0;
	}

	send_reply(req, 200);

	/* send to all channel users. */
	channel_write(channel, data, data_len);

	return 0;
}

