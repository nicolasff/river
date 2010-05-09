#ifndef HTTP_H
#define HTTP_H

#include "http-parser/http_parser.h"
#include "dict.h"

typedef enum {HTTP_DISCONNECT, HTTP_KEEP_CONNECTED, HTTP_MONITOR} http_action;

struct event_base;

/* Send an HTTP response */
void
http_response(int fd, int code, const char *status, const char *data, size_t len);

void
http_response_ct(int fd, int code, const char *status, const char *data, size_t len, const char *content_type);


/* Start streaming with chunked encoding */
void
http_streaming_start(int fd, int code, const char *status);
void
http_streaming_start_ct(int fd, int code, const char *status, const char *content_type);

/* Send data chunk */
int
http_streaming_chunk(int fd, const char *data, size_t len);

/* Stop streaming, close connection. */
void
http_streaming_end(int fd);



struct http_request {

	int fd;

	char *path;
	size_t path_len;

	char *header_next; /* used in header parsing */

	char *host;
	size_t host_len;

	char *origin;
	size_t origin_len;

	struct p_channel *channel;

	dict *get;
	struct event_base *base;
};

int
http_parser_onurl(http_parser *parser, const char *at, size_t len);

int
http_parser_onpath(http_parser *parser, const char *at, size_t len);

int
http_parser_on_header_field(http_parser *parser, const char *at, size_t len);

int
http_parser_on_header_value(http_parser *parser, const char *at, size_t len);

#endif /* HTTP_H */
