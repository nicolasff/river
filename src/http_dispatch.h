#ifndef HTTP_DISPATCH_H
#define HTTP_DISPATCH_H

#include "http.h"

struct connection;

http_action
http_dispatch(struct connection *cx);

http_action
http_dispatch_publish(struct connection *cx);

http_action
http_dispatch_read(struct connection *cx, start_function start_fun,
		write_function write_fun);

#endif
