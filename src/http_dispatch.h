#ifndef COMETD_HTTP_DISPATCH_H
#define COMETD_HTTP_DISPATCH_H

#include "http.h"

struct http_request;
struct conf;
struct p_channel;
struct event;

struct user_timeout {

	struct p_channel_user *pcu;
	struct p_channel *channel;
	struct event ev;
	struct timeval tv;
};

void
http_init(struct conf *cfg);

http_action
http_dispatch(struct http_request *req);

http_action
http_dispatch_flash_crossdomain(struct http_request *req);

http_action
http_dispatch_iframe(struct http_request *req);

http_action
http_dispatch_subscribe(struct http_request *req);

http_action
http_dispatch_publish(struct http_request *req);

#endif /* COMETD_HTTP_DISPATCH_H */

