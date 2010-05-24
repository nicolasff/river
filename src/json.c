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
		if(data[i] == '"' || data[i] == '\\') {
			j++;
		}
		j++;
	}
	/* allocate output buffer */
	ret = calloc(1 + j, 1);

	j = 0;
	/* copy into output buffer */
	for(i = 0; i < len; ++i) {
		if(data[i] == '"' || data[i] == '\\') {
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
json_wrap(const char *data, size_t data_len, const char *jsonp, size_t jsonp_len, size_t *out) {

	size_t sz = jsonp_len + 1 + data_len + 4;
	char *buffer = calloc(sz + 1, 1);

	memcpy(buffer, jsonp, jsonp_len);
	memcpy(buffer + jsonp_len, "(", 1);
	memcpy(buffer + jsonp_len + 1, data, data_len);
	memcpy(buffer + jsonp_len + 1 + data_len, ");\r\n", 4);

	if(out) {
		*out = sz;
	}
	return buffer;
}

char *
json_msg(const char *channel, size_t channel_len,
		const unsigned long long seq,
		const char *data, size_t data_len,
		size_t *out_len) {

	size_t needed = 0;
	char *buffer = NULL, *pos;

	char fmt0[] = "[\"msg\", {\"channel\": \"";
	char fmt1[] = "\", \"seq\": ";
	char fmt2[] = ", \"data\": \"";
	char fmt3[] = "\"}]";

	char seq_str[40];
	int seq_len = sprintf(seq_str, "%lld", seq);

	/* escape data */
	char *esc_data, *esc_channel;
	esc_data = json_escape(data, data_len, &data_len);
	esc_channel = json_escape(channel, channel_len, &channel_len);

	needed = sizeof(fmt0)-1
		+ channel_len
		+ sizeof(fmt1)-1
		+ seq_len
		+ sizeof(fmt2)-1
		+ data_len
		+ sizeof(fmt3)-1;

	pos = buffer = calloc(needed + 1, 1);
	buffer[needed] = 0;

	memcpy(pos, fmt0, sizeof(fmt0)-1);
	pos += sizeof(fmt0)-1;

	memcpy(pos, esc_channel, channel_len);
	pos += channel_len;

	memcpy(pos, fmt1, sizeof(fmt1)-1);
	pos += sizeof(fmt1)-1;

	memcpy(pos, seq_str, seq_len);
	pos += seq_len;

	memcpy(pos, fmt2, sizeof(fmt2)-1);
	pos += sizeof(fmt2)-1;

	memcpy(pos, esc_data, data_len);
	pos += data_len;

	memcpy(pos, fmt3, sizeof(fmt3)-1);

	if(out_len) {
		*out_len = needed;
	}

	free(esc_data);
	free(esc_channel);

	return buffer;
}

