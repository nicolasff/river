#ifndef HTTP_H
#define HTTP_H

#include "http-parser/http_parser.h"
#include "dict.h"

struct event_base;
struct http_request;

typedef enum {HTTP_DISCONNECT, HTTP_KEEP_CONNECTED, HTTP_WEBSOCKET_MONITOR} http_action;

typedef int (*write_function)(int fd, const char *data, size_t len);
typedef int (*start_function)(struct http_request *req);


/* Send an HTTP response */
void
http_response(int fd, int code, const char *status, const char *data, size_t len);

void
http_response_ct(int fd, int code, const char *status, const char *data, size_t len, const char *content_type);

/* Send an empty reply. */
void
send_empty_reply(struct http_request *req, int error);

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

	struct channel *channel;
	struct channel_user *pcu;

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
