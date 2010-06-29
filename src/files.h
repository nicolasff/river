#ifndef FILES_H
#define FILES_H

#include "http.h"

int
file_send_flash_crossdomain(struct connection *cx);

int
file_send(struct connection *cx);


#endif /* FILES_H */

