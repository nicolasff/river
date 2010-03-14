#include "json.h"

#include <stdlib.h>
#include <stdio.h>

char *
json_msg(const char *channel, const long uid, const time_t ts, const char *data, size_t *len) {

	/* TODO: replace " by \" on the fly */
	int needed = 0;
	char *buffer = NULL;
	char fmt[] = "{msg, {channel: \"%s\", "
			"uid: %ld, "
			"time: %ld, "
			"data: \"%s\"}}";
	
	needed = snprintf(NULL, 0, fmt, channel, uid, ts, data);
	buffer = calloc(needed, 1);
	*len = (size_t)snprintf(buffer, 1+needed, fmt, channel, uid, ts, data);

	return buffer;
}

