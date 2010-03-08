#include <sys/queue.h>

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "http_dispatch.h"
#include "user.h"
#include "connection.h"
#include "channel.h"
#include "server.h"
#include "message.h"


void
http_response(int fd, int code, char *status, const char *data, size_t len) {
	
	/* GNU-only. TODO: replace with something more portable */
	dprintf(fd, "HTTP/1.1 %d %s\r\n"
			"Content-Length: %lu\r\n"
			"Content-Type: text/html\r\n"
			"\r\n"
			"%s",
			code, status, len, data);
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
 * Authenticate user.
 * 
 * Parameters: uid, sid.
 */
int
http_dispatch_meta_authenticate(struct http_request *req) {

	struct p_user *user;
	int success = 0;

	/* Look for user or create it */
	if(NULL == user_find(req->uid)) {
		user = user_new(req->uid, req->sid);
		if(user) {
			user->fd = req->fd;
			user_save(user);
			success = 1;
			printf("OK\n");
		} 
	}

	// reply
	if(success) {
		send_reply(req, 200);
	} else {
		send_reply(req, 403);
	}
	return 0;
}

#if 1

/**
 * Connect user. This call is blocking, waiting for new data.
 * 
 * Parameters: uid, sid.
 */
int
http_dispatch_meta_connect(struct http_request *req) {

	int success = 0;
	struct p_user *user;

	/* find user. */
	if(0 == req->uid || NULL == req->sid) {
		success = 0;
	} else {
		user = user_find(req->uid);
		/* if user by uid is found, check sid */
		if(user && !strncmp(req->sid, user->sid, req->sid_len)) { 
			success = 1;
		}
	}

	if(success) { /* register the request with the user */

#if 0
		struct evbuffer *padding;

		pthread_mutex_lock(&user->lock);

		/* send whitespace padding */
		padding = evbuffer_new();
		evhttp_send_reply_start(ev, 200, "OK");
		evbuffer_add_printf(padding, "\n");
		evhttp_send_reply_chunk(ev, padding);
		evbuffer_free(padding);
#endif


		pthread_mutex_unlock(&user->lock);
		return 1;

	} else {
		send_reply(req, 403);
		return 0;
	}
}
#endif

/**
 * Publishes a message in a channel.
 *
 * Parameters: name (channel name), data.
 */
int
http_dispatch_meta_publish(struct http_request *req) {

	struct p_channel *channel;
	struct p_channel_user *cu;

	/* TODO: get (uid, sid) parameters from the sender, authenticate him */
	channel = channel_find(req->name);

	if(NULL == channel) {
		send_reply(req, 403);
		return 0;
	}


	printf("sending to channel %s\n", req->name);

	/* send to all channel users. */
	for(cu = channel->users; cu; cu = cu->next) {

		struct p_user *user = user_find(cu->uid);
		printf("user=%p\n", (void*)user);

		if(NULL == user) {
			continue;
		}

		/* write message to user. TODO: use an inbox? */
		write(user->fd, req->data, req->data_len);
	}

	send_reply(req, 200);
	return 0;
}



/**
 * Subscribe a user to a channel.
 */
int
http_dispatch_meta_subscribe(struct http_request *req) {

	struct p_channel *channel;

	if(!(channel = channel_find(req->name))) {
		send_reply(req, 404);
		return 0;
	}

	if(0 != req->uid && NULL != req->sid) { /* found user */
		struct p_user *user;

		user = user_find(req->uid);
		/* if user by uid is found, authenticate using sid */
		if(user && !strncmp(req->sid, user->sid, req->sid_len)) { 
	
			/* locking user here */
			pthread_mutex_lock(&(user->lock));
			if(channel_has_user(channel, user)) {
				send_reply(req, 403);
			} else {
				send_reply(req, 200);
				printf("subscribe user %ld (sid %s) to channel %s\n",
						req->uid, req->sid, req->name);
				channel_add_user(channel, user);
			}
			pthread_mutex_unlock(&(user->lock));

		} else { /* wrong sid or unknown user */
			send_reply(req, 403);
		}
	} else { /* wrong parameters */
		send_reply(req, 403);
	}
	return 0;
}

/**
 * Creates a new channel.
 * Parameters: name, key
 */
int
http_dispatch_meta_newchannel(struct http_request *req) {

	/* get (name, key) parameters */
	/* TODO: check that the key is right. */

	/* create channel */
	if(NULL == channel_find(req->name)) {
		channel_new(req->name);
		send_reply(req, 200);
	} else {
		send_reply(req, 403);
	}
	return 0;
}

