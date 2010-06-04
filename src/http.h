#ifndef HTTP_H
#define HTTP_H

#include "http-parser/http_parser.h"
#include "dict.h"
#include "server.h"

struct event_base;
struct http_request;

typedef enum {HTTP_DISCONNECT, HTTP_KEEP_CONNECTED, HTTP_WEBSOCKET_MONITOR} http_action;

typedef int (*write_function)(struct connection *cx, const char *data, size_t len);
typedef int (*start_function)(struct http_request *req);


/* Send an HTTP response */
int
http_response(struct connection *cx, int code, const char *status, const char *data, size_t len);

int
http_response_ct(struct connection *cx, int code, const char *status, const char *data, size_t len, const char *content_type);

/* Send an empty reply. */
void
send_empty_reply(struct http_request *req, int error);

/* Start streaming with chunked encoding */
void
http_streaming_start(struct connection *cx, int code, const char *status);
void
http_streaming_start_ct(struct connection *cx, int code, const char *status, const char *content_type);

/* Send data chunk */
int
http_streaming_chunk(struct connection *cx, const char *data, size_t len);

/* Stop streaming, close connection. */
void
http_streaming_end(struct connection *cx);



struct http_request {

	int fd;
	struct connection *cx;

	char *path;
	size_t path_len;

	char *header_next; /* used in header parsing */

	char *host;
	size_t host_len;

	char *origin;
	size_t origin_len;

	struct channel *channel;
	struct channel_user *cu;

	struct {
		char *name; int name_len;
		char *data; int data_len;
		char *jsonp; int jsonp_len;
		char *domain; int domain_len;

		unsigned long long seq; int has_seq;
		long keep;
	} get;


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
