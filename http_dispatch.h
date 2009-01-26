#ifndef COMETD_HTTP_DISPATCH_H
#define COMETD_HTTP_DISPATCH_H

struct evhttp_request;

void
http_dispatch_meta_authenticate(struct evhttp_request *ev, void *data);

void
http_dispatch_meta_connect(struct evhttp_request *ev, void *data);

void
http_dispatch_meta_disconnect(struct evhttp_request *ev, void *data);

void
http_dispatch_meta_publish(struct evhttp_request *ev, void *nil);

void
http_dispatch_meta_subscribe(struct evhttp_request *ev, void *data);

void
http_dispatch_meta_unsubscribe(struct evhttp_request *ev, void *data);

void
http_dispatch_meta_newchannel(struct evhttp_request *ev, void *data);

#endif /* COMETD_HTTP_DISPATCH_H */

