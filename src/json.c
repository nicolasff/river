#include "json.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char *
json_escape(const char *data, size_t len, size_t *out_len) {

	char *ret;
	size_t i = 0, j = 0;
	for(i = 0; i < len; ++i) {
		/* count quotes */
		if(data[i] == '"') {
			j++;
		}
		j++;
	}
	/* allocate output buffer */
	ret = calloc(1 + j, 1);

	j = 0;
	/* copy into output buffer */
	for(i = 0; i < len; ++i) {
		if(data[i] == '"') {
			ret[j] = '\\';
			j++;
		}
		ret[j] = data[i];
		j++;
	}
	if(out_len) {
		*out_len = j;
	}
	return ret;

}

char *
json_msg(const char *channel, const long uid, const unsigned long long seq, const char *data, size_t *len) {

	int needed = 0;
	char *buffer = NULL;

	char fmt[] = "[\"msg\", {\"channel\": \"%s\", "
			"\"uid\": %ld, "
			"\"time\": %ld, "
			"\"data\": \"%s\"}]";
	char *esc_data, *esc_channel;

	esc_data = json_escape(data, strlen(data), NULL);
	esc_channel = json_escape(channel, strlen(channel), NULL);
	
	needed = snprintf(NULL, 0, fmt, esc_channel, uid, seq, esc_data);
	buffer = calloc(needed, 1);
	*len = (size_t)snprintf(buffer, 1+needed, fmt, esc_channel, uid, seq, esc_data);

	free(esc_data);
	free(esc_channel);

	return buffer;
}

