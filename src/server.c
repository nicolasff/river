#include <event.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>

#include "server.h"
#include "channel.h"
#include "socket.h"
#include "thread.h"
#include "http-parser/http_parser.h"
#include "http.h"
#include "http_dispatch.h"
#include "websocket.h"

struct event_base *base;
struct queue_t *q;

extern char flash_xd[];
extern int flash_xd_len;

int
on_client_data(struct connection *cx, void *nil) {
	(void)nil;
	int nb_read;
	http_parser parser;
	http_parser_settings settings;
	http_action action;

	char buffer[64*1024]; 
	memset(buffer, 0, sizeof(buffer));

	if(cx->state == CX_CONNECTED_WEBSOCKET) {
		if(ws_client_msg(cx) == 0) {
			return 1;
		} else {
			return -1;
		}
	}
	nb_read = read(cx->fd, buffer, sizeof(buffer));
	if(nb_read <= 0) {
		return nb_read;
	}

	/* got data, setup http parser */
	memset(&settings, 0, sizeof(http_parser_settings));
	settings.on_path = http_parser_onpath;
	settings.on_url = http_parser_onurl;
	settings.on_header_field = http_parser_on_header_field;
	settings.on_header_value = http_parser_on_header_value;

	if(nb_read == 23 &&
		memcmp(buffer, "<policy-file-request/>", 23) == 0) {
		int ret = write(cx->fd, flash_xd, flash_xd_len-1);
		(void)ret;
		action = HTTP_DISCONNECT;
	} else {
		/* parse data using @ry’s http-parser library.
		 * → http://github.com/ry/http-parser/
		 */
		http_parser_init(&parser, HTTP_REQUEST);
		parser.data = cx;
		int nb_parsed = http_parser_execute(&parser, settings, buffer, nb_read);

		if(nb_parsed  < nb_read) {
			size_t post_len = nb_read - nb_parsed - 1;

			cx->post_len = (int)post_len;
			cx->post = calloc(post_len, 1);
			memcpy(cx->post, buffer + nb_parsed + 1, post_len);
		}

		if(!nb_parsed) {
			return -1;
		} else {
			/* dispatch the client depending on the URL path */
			action = http_dispatch(cx);
		}
	}

	switch(action) {
		case HTTP_DISCONNECT:
			return -1;

		case HTTP_KEEP_CONNECTED:
			return 1;

		case HTTP_WEBSOCKET_MONITOR:
			return 1;

		default:
			return -1;
	}
}

void
server_run(int fd, int threads, struct channel *chan) {

	/* printf("running with fd=%d\n", fd); */

	struct threaded_queue *tq = tq_new(fd, threads, on_client_data, chan);
	tq_loop(tq);
}

