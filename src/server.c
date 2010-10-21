#include <event.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include "server.h"
#include "channel.h"
#include "socket.h"
#include "http_dispatch.h"
#include "websocket.h"

extern char flash_xd[];
extern int flash_xd_len;

#define CHANNEL_CLEANUP_TIMER	1

/**
 * Got client data on a connection.
 */
int
on_client_data(struct connection *cx) {

	int nb_read;
	http_parser parser;
	http_parser_settings settings;
	http_action action;

	char buffer[64*1024]; 

	if(cx->state == CX_CONNECTED_WEBSOCKET) { /* already connected WS */
		if(ws_client_msg(cx) == 0) {
			return 1;
		} else {
			return -1;
		}
	}

	memset(buffer, 0, sizeof(buffer));
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

/* Called to clean empty channels. */
void
on_channel_cleanup(int fd, short event, void *ptr) {

	(void)fd;
	(void)event;
	struct cleanup_timer *ct = ptr;

	channel_clean_idle();

	/* re-add the timer */
	cleanup_reset(ct);
}

void
cleanup_reset(struct cleanup_timer *ct) {
	evtimer_set(&ct->ev, on_channel_cleanup, ct);
	event_base_set(ct->base, &ct->ev);
	event_add(&ct->ev, &ct->tv);
}

void
server_run(int fd) {

	struct event ev;
	struct event_base *base = event_base_new();

	struct cleanup_timer ct;
	ct.base = base;
	ct.tv.tv_sec = CHANNEL_CLEANUP_TIMER;
	ct.tv.tv_usec = 0;

	/* ignore sigpipe */
#ifdef SIGPIPE
	signal(SIGPIPE, SIG_IGN);
#endif

	event_set(&ev, fd, EV_READ | EV_PERSIST, on_possible_accept, base);
	event_base_set(base, &ev);
	event_add(&ev, NULL);

	cleanup_reset(&ct);

	event_base_dispatch(base);
}


void
on_possible_accept(int fd, short event, void *ptr) {
	(void)event;

	struct event_base *base = ptr;
	struct sockaddr_in addr;
	socklen_t addr_sz = sizeof(addr);
	int client_fd;

	client_fd = accept(fd, (struct sockaddr*)&addr, &addr_sz);
	struct connection *cx = cx_new(client_fd, base);

	/* wait for new data */
	event_set(cx->ev, cx->fd, EV_READ, on_available_data, cx);
	event_base_set(base, cx->ev);
	event_add(cx->ev, NULL);
}

void
on_available_data(int fd, short event, void *ptr) {
	(void)event;

	int ret;
	struct connection *cx = ptr;

	ret = on_client_data(cx);
	if(ret <= 0) {
		cx_remove(cx);
	} else {
		/* start monitoring the connection */
		event_set(cx->ev, fd, EV_READ, on_available_data, cx);
		event_base_set(cx->base, cx->ev);
		event_add(cx->ev, NULL);
	}
}
