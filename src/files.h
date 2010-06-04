#ifndef COMETD_FILES_H
#define COMETD_FILES_H

#include "http.h"

int
file_send_flash_crossdomain(struct connection *cx);

int
file_send(struct http_request *req);


#endif /* COMETD_FILES_H */

