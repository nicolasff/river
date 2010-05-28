#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>

#include "http.h"
#include "server.h"

void
http_response(int fd, int code, const char *status, const char *data, size_t len) {
	http_response_ct(fd, code, status, data, len, "text/html");
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

void
http_response_ct(int fd, int code, const char *status, const char *data, size_t len, const char *content_type) {
	
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
	buffer = calloc(sz + 1, 1);
	ret = sprintf(buffer, template, code, status, content_type, len);
	memcpy(buffer + ret, data, len);

	ret = write(fd, buffer, sz);
	free(buffer);
	if(ret != (int)sz) {
		shutdown(fd, SHUT_RDWR);
		socket_shutdown(fd);
	}
}

void
http_streaming_start(int fd, int code, const char *status) {
	http_streaming_start_ct(fd, code, status, "text/html");
}
void
http_streaming_start_ct(int fd, int code, const char *status, const char *content_type) {

	int ret;
	size_t sz;
	char *buffer;
	const char template[] = "HTTP/1.1 %d %s\r\n"
			"Content-Type: %s\r\n"
			"Transfer-Encoding: chunked\r\n"
			"\r\n";
	sz = sizeof(template)-1 + integer_length(code) + strlen(status)
		+ strlen(content_type) - (2 + 2 + 2); /* %d %s %s */
	buffer = calloc(sz + 1, 1);
	ret = sprintf(buffer, template, code, status, content_type);

	ret = write(fd, buffer, sz);
	free(buffer);
}

int
http_streaming_chunk(int fd, const char *data, size_t len) {

	int ret;
	size_t sz = hex_length(len) + 2 + len + 2;
	char *buffer = calloc(sz + 1, 1);

	ret = sprintf(buffer, "%X\r\n", (unsigned int)len);
	memcpy(buffer + ret, data, len);
	memcpy(buffer + ret + len, "\r\n", 2);

	ret = write(fd, buffer, sz);
	free(buffer);
	if(ret == (int)sz) { /* success */
		return (int)len;
	}
	return -1; /* failure */
}

void
http_streaming_end(int fd) {

	int ret = write(fd, "0\r\n\r\n", 5);
	(void)ret;
	socket_shutdown(fd);
}

void
send_empty_reply(struct http_request *req, int error) {

	switch(error) {
		case 400:
			http_response(req->fd, 400, "Bad Request", "", 0);
			break;

		case 200:
			http_response(req->fd, 200, "OK", "", 0);
			break;

		case 404:
			http_response(req->fd, 404, "Not found", "", 0);
			break;

		case 403:
			http_response(req->fd, 403, "Forbidden", "", 0);
			break;
	}
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

	/*req->get = dictCreate(&dictTypeCopyNoneFreeAll, NULL);*/
	memset(&req->get, 0, sizeof(req->get));
	req->get.keep = 1;


	/* we have strings in the following format: at="ab=12&cd=34ø", len=11 */

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
		if(strncmp(key, "name", 4) == 0) {
			req->get.name = val;
			req->get.name_len = val_len;
		} else if(strncmp(key, "data", 4) == 0) {
			req->get.data = val;
			req->get.data_len = val_len;
		} else if(strncmp(key, "jsonp", 5) == 0) {
			req->get.jsonp = val;
			req->get.jsonp_len = val_len;
		} else if(strncmp(key, "domain", 6) == 0) {
			req->get.domain = val;
			req->get.domain_len = val_len;
		} else if(strncmp(key, "seq", 3) == 0) {
			req->get.seq = atol(val);
			req->get.has_seq = 1;
		} else if(strncmp(key, "keep", 4) == 0) {
			req->get.keep = atol(val);
		} else {
			free(key);
			free(val);
		}

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
 * Retrieve headers: called with the header name
 */
int
http_parser_on_header_field(http_parser *parser, const char *at, size_t len) {

	struct http_request *req = parser->data;

	req->header_next = calloc(len+1, 1); /* memorize the last seen */
	memcpy(req->header_next, at, len);

	return 0;
}

/**
 * Retrieve headers: called with the header value
 */
int
http_parser_on_header_value (http_parser *parser, const char *at, size_t len) {

	struct http_request *req = parser->data;
	
	if(strncmp(req->header_next, "Host", 4) == 0) { /* copy the "Host" header. */
		req->host_len = len;
		req->host = calloc(len + 1, 1);
		memcpy(req->host, at, len);
	} else if(strncmp(req->header_next, "Origin", 6) == 0) { /* copy the "Origin" header. */
		req->origin_len = len;
		req->origin = calloc(len + 1, 1);
		memcpy(req->origin, at, len);
	}
	free(req->header_next);
	req->header_next = NULL;
	return 0;
}

