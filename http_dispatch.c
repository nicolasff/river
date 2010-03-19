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

	/* TODO: fill this with correct code */
	char buffer[] = "\n\
<html>\n\
<body>\n\
	iframe\n\
	<script>\n\
		document.domain = \"test.com\";\n\
\n\
		if (typeof XMLHttpRequest == \"undefined\") {\n\
			XMLHttpRequest = function () {\n\
				try { return new ActiveXObject(\"Msxml2.XMLHTTP.6.0\"); }\n\
				catch (e1) {}\n\
				try { return new ActiveXObject(\"Msxml2.XMLHTTP.3.0\"); }\n\
				catch (e2) {}\n\
				try { return new ActiveXObject(\"Msxml2.XMLHTTP\"); }\n\
				catch (e3) {}\n\
				try { return new ActiveXObject(\"Microsoft.XMLHTTP\"); }\n\
				catch (e4) {}\n\
				throw new Error(\"This browser does not support XMLHttpRequest.\");\n\
			};\n\
		}\n\
\n\
		function send() {\n\
			var xhr = new XMLHttpRequest;\n\
			xhr.open(\"get\", \"http://comet.test.com/comet.php?\", true);\n\
			xhr.onreadystatechange = function(){\n\
				if(xhr.readyState === 4) {\n\
					parent.notice(xhr.responseText);\n\
				}\n\
			};\n\
			xhr.send(null);\n\
		}\n\
	</script>\n\
</body>\n\
</html>";

	http_response(req->fd, 200, "OK", buffer, sizeof(buffer)-1);
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

	long uid = 0;
	char *sid = NULL;
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
 * Connect user to a channel. This call is blocking, waiting for new data.
 * 
 * Parameters: uid, sid, name, [time]
 */
int
http_dispatch_meta_read(struct http_request *req) {

	int success = 0;
	struct p_user *user = NULL;
	struct p_channel *channel = NULL;

	long uid = 0, timestamp = 0;
	char *sid = NULL, *name = NULL;
	dictEntry *de;

	if((de = dictFind(req->get, "uid"))) {
		uid = atol(de->val);
	}
	if((de = dictFind(req->get, "sid"))) {
		sid = de->val;
	}
	if((de = dictFind(req->get, "name"))) {
		name = de->val;
	}
	if((de = dictFind(req->get, "time"))) { /* optional */
		timestamp = atol(de->val);
	}

	/* find user. */
	if(!uid || !sid || !name) {
		success = 0;
	} else {
		user = user_find(uid);
		/* if user by uid is found, check sid */
		if(user && !strcmp(sid, user->sid)) {
			success = 1;
		}
	}

	/* find channel */
	if(!(channel = channel_find(name))) {
		success = 0;
	}

	if(success) {
		struct p_channel_user *pcu;
		pcu = channel_add_connection(channel, user, req->fd);
		http_streaming_start(req->fd, 200, "OK");
		if(timestamp) {
			return channel_catchup_user(channel, pcu, (time_t)timestamp);
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

	long uid = 0;
	char *sid = NULL, *name = NULL, *data = NULL;
	dictEntry *de;
	size_t data_len = 0, sid_len = 0;
	struct p_user *user;

	if((de = dictFind(req->get, "name"))) {
		name = de->val;
	}
	if((de = dictFind(req->get, "data"))) {
		data = de->val;
		data_len = de->size;
	}
	if((de = dictFind(req->get, "uid"))) {
		uid = atol(de->val);
	}
	if((de = dictFind(req->get, "sid"))) {
		sid = de->val;
		sid_len = de->size;
	}
	if(!sid || !uid || !name || !data) {
		send_reply(req, 403);
		return 0;
	}

	/* get (uid, sid) parameters from the sender, authenticate him */
	user = user_find(uid);
	if(0 != strncmp(user->sid, sid, sid_len)) {
		send_reply(req, 403);
		return 0;
	}


	/* find channel */
	if(!(channel = channel_find(name))) {
		send_reply(req, 403);
		return 0;
	}

	/* send to all channel users. */
	channel_write(channel, uid, data, data_len);

	send_reply(req, 200);
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
	/* TODO: check that the key is right. */

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

