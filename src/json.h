#ifndef JSON_H
#define JSON_H

#include "time.h"

char *
json_msg(const char *channel, size_t channel_len,
		const unsigned long long seq,
		const char *data, size_t data_len,
		size_t *json_len);

char *
json_escape(const char *data, size_t len, size_t *out_len);

char *
json_wrap(const char *data, size_t data_len, const char *jsonp, size_t jsonp_len, size_t *out);

#endif /* JSON_H */

