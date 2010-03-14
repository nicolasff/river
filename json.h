#ifndef JSON_H
#define JSON_H

#include "time.h"

char *
json_msg(const char *channel, const long uid, const time_t ts, const char *data, size_t *len);

#endif /* JSON_H */

