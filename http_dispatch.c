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

	if(req->path_len == 18 && 0 == strncmp(req->path, "/meta/authenticate", 18)) {
		return http_dispatch_meta_authenticate(req);
	} else if(req->path_len == 13 && 0 == strncmp(req->path, "/meta/publish", 13)) {
		return http_dispatch_meta_publish(req);
	} else if(req->path_len == 13 && 0 == strncmp(req->path, "/meta/connect", 13)) {
		return http_dispatch_meta_connect(req);
	} else if(req->path_len == 15 && 0 == strncmp(req->path, "/meta/subscribe", 15)) {
		return http_dispatch_meta_subscribe(req);
	} else if(req->path_len == 16 && 0 == strncmp(req->path, "/meta/newchannel", 16)) {
		return http_dispatch_meta_newchannel(req);
	}

	return 0;
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

	long uid;
	char *sid;
	dictEntry *de;

	if((de = dictFind(req->get, "uid"))) {
		uid = atol(de->val);
	}
	if((de = dictFind(req->get, "sid"))) {
		sid = de->val;
	}

	/* Look for user or create it */
	if(NULL == de || NULL == user_find(uid)) {
		user = user_new(uid, sid);
		if(user) {
			user->fd = req->fd;
			user_save(user);
			success = 1;
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

/**
 * Connect user. This call is blocking, waiting for new data.
 * 
 * Parameters: uid, sid.
 */
int
http_dispatch_meta_connect(struct http_request *req) {

	int success = 0;
	struct p_user *user;

	long uid;
	char *sid;
	dictEntry *de;

	if((de = dictFind(req->get, "uid"))) {
		uid = atol(de->val);
	}
	if((de = dictFind(req->get, "sid"))) {
		sid = de->val;
	}

	/* find user. */
	if(0 == uid || NULL == sid) {
		success = 0;
	} else {
		user = user_find(uid);
		/* if user by uid is found, check sid */
		if(user && !strcmp(sid, user->sid)) {
			success = 1;
		}
	}

	if(success) {
		http_streaming_start(req->fd, 200, "OK");
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
	struct p_channel_user *cu;

	long uid;
	char *sid, *name, *data;
	dictEntry *de;
	size_t data_len;

	if((de = dictFind(req->get, "uid"))) {
		uid = atol(de->val);
	}
	if((de = dictFind(req->get, "sid"))) {
		sid = de->val;
	}
	if((de = dictFind(req->get, "name"))) {
		name = de->val;
	}
	if((de = dictFind(req->get, "data"))) {
		data = de->val;
		data_len = de->size;
	}

	/* TODO: get (uid, sid) parameters from the sender, authenticate him */
	channel = channel_find(name);

	if(NULL == channel) {
		send_reply(req, 403);
		return 0;
	}

	/* printf("sending to channel %s\n", req->name); */

	/* send to all channel users. */
	for(cu = channel->users; cu; cu = cu->next) {
		size_t ret;
		struct p_user *user = user_find(cu->uid);

		if(NULL == user) {
			continue;
		}

		/* write message to user. TODO: use an inbox? */
		ret = http_streaming_chunk(user->fd, data, data_len); // TODO: store size.
		if(ret != data_len) { /* failed write */
			/* TODO: check that everything is cleaned. */
			close(user->fd);
			channel_del_user(channel, user->uid);
		}
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

	long uid;
	char *sid, *name;
	dictEntry *de;
	size_t sid_len;

	if((de = dictFind(req->get, "uid"))) {
		uid = atol(de->val);
	}
	if((de = dictFind(req->get, "sid"))) {
		sid = de->val;
		sid_len = de->size;
	}
	if((de = dictFind(req->get, "name"))) {
		name = de->val;
	}

	if(!(channel = channel_find(name))) {
		send_reply(req, 404);
		return 0;
	}

	if(0 != uid && NULL != sid) { /* found user */
		struct p_user *user;

		user = user_find(uid);
		/* if user by uid is found, authenticate using sid */
		if(user && !strncmp(sid, sid, sid_len)) {
	
			/* locking user here */
			pthread_mutex_lock(&(user->lock));
			if(channel_has_user(channel, user)) {
				send_reply(req, 403);
			} else {
				send_reply(req, 200);
				/*
				printf("subscribe user %ld (sid %s) to channel %s\n",
						uid, sid, name);
				*/
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

	char *name;
	dictEntry *de;

	if((de = dictFind(req->get, "name"))) {
		name = de->val;
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

