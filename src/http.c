#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "http.h"
#include "socket.h"
#include "mem.h"

int
http_response(struct connection *cx, int code, const char *status, const char *data, size_t len) {
	return http_response_ct(cx, code, status, data, len, "text/html");
}
static int
hex_length(unsigned int i) {
	int ret = 0;
	while(i) {
		i >>= 4;
		ret++;
	}
	return ret;
}

static int
integer_length(int i) {
    int sz = 0;
    int ci = abs(i);
    while (ci>0) {
            ci = (ci/10);
            sz += 1;
    }
    if(i == 0) { /* log 0 doesn't make sense. */
            sz = 1;
    } else if(i < 0) { /* allow for neg sign as well. */
            sz++;
    }
    return sz;
}

int
http_response_ct(struct connection *cx, int code, const char *status, const char *data, size_t len, const char *content_type) {

	int ret;
	size_t sz;
	char *buffer;
	const char template[] = "HTTP/1.1 %d %s\r\n"
			"Content-Type: %s\r\n"
			"Content-Length: %lu\r\n"
			"\r\n";
	sz = sizeof(template)-1 + integer_length(code) + strlen(status)
		+ strlen(content_type) + integer_length((int)len)
		- (2 + 2 + 2 + 3) /* %d %s %s %lu*/
		+ len;
	buffer = rcalloc(sz + 1, 1);
	ret = sprintf(buffer, template, code, status, content_type, len);
	memcpy(buffer + ret, data, len);

	ret = write(cx->fd, buffer, sz);
	rfree(buffer);
	return ret;
}

void
http_streaming_start(struct connection *cx, int code, const char *status) {
	http_streaming_start_ct(cx, code, status, "text/html");
}
void
http_streaming_start_ct(struct connection *cx, int code, const char *status, const char *content_type) {

	int ret;
	size_t sz;
	char *buffer;
	const char template[] = "HTTP/1.1 %d %s\r\n"
			"Content-Type: %s\r\n"
			"Transfer-Encoding: chunked\r\n"
			"\r\n";
	sz = sizeof(template)-1 + integer_length(code) + strlen(status)
		+ strlen(content_type) - (2 + 2 + 2); /* %d %s %s */
	buffer = rcalloc(sz + 1, 1);
	ret = sprintf(buffer, template, code, status, content_type);

	ret = write(cx->fd, buffer, sz);
	rfree(buffer);
}

int
http_streaming_chunk(struct connection *cx, const char *data, size_t len) {

	int ret;
	size_t sz = hex_length(len) + 2 + len + 2;
	char *buffer = rcalloc(sz + 1, 1);

	ret = sprintf(buffer, "%X\r\n", (unsigned int)len);
	memcpy(buffer + ret, data, len);
	memcpy(buffer + ret + len, "\r\n", 2);

	ret = write(cx->fd, buffer, sz);
	rfree(buffer);
	if(ret == (int)sz) { /* success */
		return (int)len;
	}
	return -1; /* failure */
}

void
http_streaming_end(struct connection *cx) {

	int ret = write(cx->fd, "0\r\n\r\n", 5);
	(void)ret;
}

void
send_empty_reply(struct connection *cx, int error) {

	switch(error) {
		case 400:
			http_response(cx, 400, "Bad Request", "", 0);
			break;

		case 200:
			http_response(cx, 200, "OK", "", 0);
			break;

		case 404:
			http_response(cx, 404, "Not found", "", 0);
			break;

		case 403:
			http_response(cx, 403, "Forbidden", "", 0);
			break;
	}
}


/**
 * Copy the url
 */
static int
http_parser_split_params(http_parser *parser, const char *at, size_t len, http_step step) {

	struct connection *cx = parser->data;

	const char *p;
	switch(step) {
		case ON_URL:
			p = strchr(at, '?'); /* GET: start after page name */
			if(p && p < at + len) {
				p++;
			} else {
				return 0;
			}
			break;

		case ON_BODY:		/* POST: read data directly */
			p = at;
			break;

		default:
			return -1;
	}

	/* memset(&cx->get, 0, sizeof(cx->get)); */
	cx->get.keep = 1;


	/* we have strings in the following format: at="ab=12&cd=34ø", len=11 */

	while(1) {
		char *eq, *amp, *key, *val;
		size_t key_len, val_len;

		/* find '=' */
		eq = memchr(p, '=', p - at + len);
		if(!eq) break;

		/* copy from the previous position to right before the '=' */
		key_len = eq - p;
		key = rcalloc(1 + key_len, 1);
		memcpy(key, p, key_len);


		/* move 1 char to the right, now on data. */
		p = eq + 1;
		if(!*p) {
			rfree(key);
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
		val = rcalloc(1 + val_len, 1);
		memcpy(val, p, val_len);

		/* add to the GET dictionary */
		if(strncmp(key, "name", 4) == 0 && cx->get.name == NULL) {
			cx->get.name = val;
			cx->get.name_len = val_len;
		} else if(strncmp(key, "data", 4) == 0 && cx->get.data == NULL) {
			cx->get.data = val;
			cx->get.data_len = val_len;
		} else if(strncmp(key, "jsonp", 5) == 0 && cx->get.jsonp == NULL) {
			cx->get.jsonp = val;
			cx->get.jsonp_len = val_len;
		} else if(strncmp(key, "domain", 6) == 0 && cx->get.domain == NULL) {
			cx->get.domain = val;
			cx->get.domain_len = val_len;
		} else if(strncmp(key, "seq", 3) == 0) {
			cx->get.seq = atol(val);
			cx->get.has_seq = 1;
			rfree(val);
		} else if(strncmp(key, "keep", 4) == 0) {
			cx->get.keep = atol(val);
			rfree(val);
		} else {
			rfree(val);
		}
		rfree(key);

		if(amp) { /* more to come */
			p = amp + 1;
		} else { /* end of string */
			break;
		}
	}

	return 0;
}

int
http_parser_onurl(http_parser *parser, const char *at, size_t len) {
	return http_parser_split_params(parser, at, len, ON_URL);
}

int
http_parser_onbody(http_parser *parser, const char *at, size_t len) {
	return http_parser_split_params(parser, at, len, ON_BODY);
}


/**
 * Copy the path
 */
int
http_parser_onpath(http_parser *parser, const char *at, size_t len) {

	struct connection *cx = parser->data;

	cx->path = rcalloc(1+len, 1);
	memcpy(cx->path, at, len);
	cx->path_len = len;

	return 0;
}

/**
 * Retrieve headers: called with the header name
 */
int
http_parser_on_header_field(http_parser *parser, const char *at, size_t len) {

	struct connection *cx = parser->data;

	cx->header_next = rcalloc(len+1, 1); /* memorize the last seen */
	memcpy(cx->header_next, at, len);

	return 0;
}

/**
 * Retrieve headers: called with the header value
 */
int
http_parser_on_header_value (http_parser *parser, const char *at, size_t len) {

	struct connection *cx = parser->data;

	if(strncmp(cx->header_next, "Host", 4) == 0) { /* copy the "Host" header. */
		cx->headers.host_len = len;
		cx->headers.host = rcalloc(len + 1, 1);
		memcpy(cx->headers.host, at, len);
	} else if(strncmp(cx->header_next, "Origin", 6) == 0) { /* copy the "Origin" header. */
		cx->headers.origin_len = len;
		cx->headers.origin = rcalloc(len + 1, 1);
		memcpy(cx->headers.origin, at, len);
	} else if(strncmp(cx->header_next, "Sec-WebSocket-Key1", 18) == 0) {
		cx->headers.ws1_len = len;
		cx->headers.ws1 = rcalloc(len + 1, 1);
		memcpy(cx->headers.ws1, at, len);
	} else if(strncmp(cx->header_next, "Sec-WebSocket-Key2", 18) == 0) {
		cx->headers.ws2_len = len;
		cx->headers.ws2 = rcalloc(len + 1, 1);
		memcpy(cx->headers.ws2, at, len);
	}
	rfree(cx->header_next);
	cx->header_next = NULL;
	return 0;
}
