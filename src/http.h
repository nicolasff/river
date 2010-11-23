#ifndef HTTP_H
#define HTTP_H

#include "http-parser/http_parser.h"
struct connection;

typedef enum {HTTP_DISCONNECT, HTTP_KEEP_CONNECTED, HTTP_WEBSOCKET_MONITOR} http_action;
typedef enum {ON_URL, ON_BODY} http_step;
typedef int (*write_function)(struct connection *cx, const char *data, size_t len);
typedef int (*start_function)(struct connection *cx);


/* Send an HTTP response */
int
http_response(struct connection *cx, int code, const char *status, const char *data, size_t len);

int
http_response_ct(struct connection *cx, int code, const char *status, const char *data, size_t len, const char *content_type);

/* Send an empty reply. */
void
send_empty_reply(struct connection *cx, int error);

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


int
http_parser_onurl(http_parser *parser, const char *at, size_t len);
int
http_parser_onbody(http_parser *parser, const char *at, size_t len);

int
http_parser_onpath(http_parser *parser, const char *at, size_t len);

int
http_parser_on_header_field(http_parser *parser, const char *at, size_t len);

int
http_parser_on_header_value(http_parser *parser, const char *at, size_t len);

#endif
