#ifndef COMETD_HTTP_DISPATCH_H
#define COMETD_HTTP_DISPATCH_H

#include "http.h"

struct http_request;
struct conf;
struct channel;
struct event;

struct user_timeout {

	struct channel_user *pcu;
	struct channel *channel;
	struct event ev;
	struct timeval tv;
};

void
http_init(struct conf *cfg);

http_action
http_dispatch(struct http_request *req);

http_action
http_dispatch_read(struct http_request *req, start_function start_fun,
		write_function write_fun);

http_action
http_dispatch_publish(struct http_request *req);

#endif /* COMETD_HTTP_DISPATCH_H */

