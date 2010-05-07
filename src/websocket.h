#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include "http.h"

int
ws_start(struct http_request *req);

int
ws_write(int fd, const char *buf, size_t len);

#endif /* WEBSOCKET_H */
