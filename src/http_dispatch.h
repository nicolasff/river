#ifndef COMETD_HTTP_DISPATCH_H
#define COMETD_HTTP_DISPATCH_H

struct http_request;
struct conf;

void
http_init(struct conf *cfg);

int
http_dispatch(struct http_request *req);

int
http_dispatch_root(struct http_request *req);

int
http_dispatch_meta_read(struct http_request *req);

int
http_dispatch_meta_publish(struct http_request *req);

int
http_dispatch_meta_newchannel(struct http_request *req);

#endif /* COMETD_HTTP_DISPATCH_H */

