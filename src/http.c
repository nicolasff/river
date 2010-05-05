#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "http.h"

void
http_response(int fd, int code, const char *status, const char *data, size_t len) {
	http_response_ct(fd, code, status, data, len, "text/html");
}

void
http_response_ct(int fd, int code, const char *status, const char *data, size_t len, const char *content_type) {
	
	int ret;
	/* GNU-only. TODO: replace with something more portable */
	ret = dprintf(fd, "HTTP/1.1 %d %s\r\n"
			"Content-Type: %s\r\n"
			"Content-Length: %lu\r\n"
			"\r\n",
			code, status, content_type, len);
	if(ret) {
		ret = write(fd, data, len);
		shutdown(fd, SHUT_RDWR);
		close(fd);
		(void)ret;
	}
}

void
http_streaming_start(int fd, int code, const char *status) {

	dprintf(fd, "HTTP/1.1 %d %s\r\n"
			"Content-Type: text/html\r\n"
			"Transfer-Encoding: chunked\r\n"
			"\r\n"
			, code, status);
}

int
http_streaming_chunk(int fd, const char *data, size_t len) {

	int tmp, ret;
	dprintf(fd, "%X\r\n", (unsigned int)len);
	ret = write(fd, data, len);
	tmp = write(fd, "\r\n", 2);
	(void)tmp;

	return ret;
}

void
http_streaming_end(int fd) {

	int ret = write(fd, "0\r\n\r\n", 5);
	(void)ret;
	shutdown(fd, SHUT_RDWR);
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

	/* we have strings in the following format:
	 * pos:      0123456789AB
	 * string:   ab=12&cd=34ø
	 */

	while(1) {
		char *eq, *amp, *key, *val;
		size_t key_len, val_len;

		/* find '=' */
		eq = memchr(p, '=', p - at + len);
		if(!eq) break;

		/* copy from the previous position to right before the '=' */
		key_len = eq - p;
		key = calloc(1 + key_len, 1);
		memcpy(key, p, key_len);


		/* move 1 char to the right, now on data. */
		p = eq + 1;
		if(!*p) {
			free(key);
			break;
		}

		/* find the end of data, or the end of the string */
		amp = memchr(p, '&', p - at + len);
		if(amp) {
			val_len = amp - p;
		} else {
			val_len = at + len - p;
		}

		/* copy data, from after the '=' to here. */
		val = calloc(1 + val_len, 1);
		memcpy(val, p, val_len);

		/* add to the GET dictionary */
		dictAdd(req->get, key, val, val_len);

		if(amp) { /* more to come */
			p = amp + 1;
		} else { /* end of string */
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

/**
 * Retrieve headers
 */
int
http_parser_on_header_field(http_parser *parser, const char *at, size_t len) {

	struct http_request *req = parser->data;

	if(0 == strncmp(at, "Host", len)) { /* next value is the host */
		req->host_follows = 1;
	}
	return 0;
}

int
http_parser_on_header_value (http_parser *parser, const char *at, size_t len) {

	struct http_request *req = parser->data;
	
	if(req->host_follows) { /* copy the "Host" header. */
		req->host_follows = 0;
		req->host_len = len;
		req->host = calloc(len + 1, 1);
		memcpy(req->host, at, len);
	}
	return 0;
}

