#include <sys/queue.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

#include "http_dispatch.h"
#include "user.h"
#include "channel.h"
#include "server.h"
#include "message.h"
#include "http.h"
#include "dict.h"
#include "conf.h"

static struct conf *__cfg;

void
http_init(struct conf *cfg) {

	__cfg = cfg;
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

	/*if(req->path_len == 18 && 0 == strncmp(req->path, "/meta/authenticate", 18)) {
		return http_dispatch_meta_authenticate(req);
	} else*/ if(req->path_len == 13 && 0 == strncmp(req->path, "/meta/publish", 13)) {
		return http_dispatch_meta_publish(req);
	} else if(req->path_len == 13 && 0 == strncmp(req->path, "/meta/connect", 13)) {
		return http_dispatch_meta_read(req);
	} else if(req->path_len == 16 && 0 == strncmp(req->path, "/meta/newchannel", 16)) {
		return http_dispatch_meta_newchannel(req);
	} else if(req->path_len == 1 && 0 == strncmp(req->path, "/", 1)) {
		return http_dispatch_root(req);
	}

	return 0;
}


/**
 * Return generic page for iframe inclusion
 */
int
http_dispatch_root(struct http_request *req) {

	char buffer_start[] = "<html><body><script>\ndocument.domain=\"";
	char buffer_domain[] = "\";\n";
	char buffer_end[] = "</script></body></html>\n";

	FILE *f  = fopen("iframe.js", "r");

	http_streaming_start(req->fd, 200, "OK");
	http_streaming_chunk(req->fd, buffer_start, sizeof(buffer_start)-1);
	if(__cfg->common_domain) {
		http_streaming_chunk(req->fd, __cfg->common_domain, __cfg->common_domain_len);
	}
	http_streaming_chunk(req->fd, buffer_domain, sizeof(buffer_domain)-1);

	while(!feof(f)) {
		char line[1024], *p;
		p = fgets(line, sizeof(line), f);
		if(!p) {
			break;
		}
		http_streaming_chunk(req->fd, p, strlen(p));
	}

	http_streaming_chunk(req->fd, buffer_end, sizeof(buffer_end)-1);
	http_streaming_end(req->fd);
	
	return 0;
}

/**
 * Connect user to a channel. This call is blocking, waiting for new data.
 * 
 * Parameters: name, [seq], [keep]
 */
int
http_dispatch_meta_read(struct http_request *req) {

	int success = 1;
	struct p_channel *channel = NULL;

	int keep_connected = 1;
	unsigned long long seq = 0;
	char *name = NULL;
	dictEntry *de;

	if((de = dictFind(req->get, "name"))) {
		name = de->val;
	}
	if((de = dictFind(req->get, "seq"))) { /* optional */
		seq = atol(de->val);
	}
	if((de = dictFind(req->get, "keep"))) { /* optional */
		keep_connected = atol(de->val);
	}

	/* find channel */
	if(!(channel = channel_find(name))) {
		success = 0;
	}

	if(success) {
		struct p_channel_user *pcu;
		pcu = channel_add_connection(channel, req->fd, keep_connected);
		http_streaming_start(req->fd, 200, "OK");
		if(seq) {
			return channel_catchup_user(channel, pcu, seq);
		}
		return 1; /* this means: do not close the connection. */
	} else {
		send_reply(req, 403);
		return 0;
	}
}

/**
 * Publishes a message in a channel.
 *
 * Parameters: name (channel name), data.
 */
int
http_dispatch_meta_publish(struct http_request *req) {

	struct p_channel *channel;

	char *name = NULL, *data = NULL;
	dictEntry *de;
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

extern char *channel_creation_key;
/**
 * Creates a new channel.
 * Parameters: name, key
 */
int
http_dispatch_meta_newchannel(struct http_request *req) {

	/* get (name, key) parameters */
	char *name = NULL, *key = NULL;
	dictEntry *de;

	if((de = dictFind(req->get, "name"))) {
		name = de->val;
	}
	if((de = dictFind(req->get, "key"))) {
		key = de->val;
	}
	if(!key || strcmp(key, channel_creation_key) != 0) {
		send_reply(req, 403);
		return 0;
	}


	/* create channel */
	if(NULL == channel_find(name)) {
		channel_new(name);
		send_reply(req, 200);
	} else {
		send_reply(req, 403);
	}
	return 0;
}

