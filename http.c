#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "http.h"

void
http_response(int fd, int code, const char *status, const char *data, size_t len) {
	
	/* GNU-only. TODO: replace with something more portable */
	dprintf(fd, "HTTP/1.1 %d %s\r\n"
			"Content-Type: text/html\r\n"
			"Content-Length: %lu\r\n"
			"\r\n",
			code, status, len);
	write(fd, data, len);
}

void
http_streaming_start(int fd, int code, const char *status) {

	dprintf(fd, "HTTP/1.1 %d %s\r\r"
			"Content-Type: text/html\r\n"
			"Content-Encoding: chunked\r\n"
			"\r\n",
			code, status);
}

int
http_streaming_chunk(int fd, const char *data, size_t len) {

	dprintf(fd, "%X\r\n", (unsigned int)len);
	return write(fd, data, len);
}

void
http_streaming_end(int fd) {

	write(fd, "0\r\n\r\n", 5);
	close(fd);
}


/**
 * Copy the url
 */
int
http_parser_onurl(http_parser *parser, const char *at, size_t len) {
	
	struct http_request *req = parser->data;

	const char *p = strchr(at, '?');

	if(!p) return 0;
	p++;

	req->get = dictCreate(&dictTypeCopyNoneFreeAll, NULL);

	while(1) {
		char *eq, *amp, *key, *val;
		size_t key_len, val_len;

		eq = memchr(p, '=', p - at + len);
		if(!eq) break;

		key_len = eq - p;
		key = calloc(1 + key_len, 1);
		memcpy(key, p, key_len);

		p = eq + 1;
		if(!*p) {
			free(key);
			break;
		}

		amp = memchr(p, '&', p - at + len);
		if(amp) {
			val_len = amp - p;
		} else {
			val_len = at + len - p;
		}

		val = calloc(1 + val_len, 1);
		memcpy(val, p, val_len);

		dictAdd(req->get, key, val, val_len);

		if(amp) {
			p = amp + 1;
		} else {
			break;
		}

	}

	return 0;
}

/**
 * Copy the path
 */
int
http_parser_onpath(http_parser *parser, const char *at, size_t len) {

	struct http_request *req = parser->data;
	req->path = calloc(1+len, 1);
	memcpy(req->path, at, len);
	req->path_len = len;
	return 0;
}

