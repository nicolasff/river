#include <stdlib.h>
#include <stdio.h>
#include <sys/queue.h>
#include <event.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include "socket.h"
#include "server.h"
#include "queue.h"
#include "http-parser/http_parser.h"
#include "http_dispatch.h"
#include "http.h"
#include "websocket.h"
#include "files.h"

extern char flash_xd[];
extern int flash_xd_len;

struct dispatcher_info di;

static void
on_accept(int fd, short event, void *ptr);

int
update_event(int flags);

void *
worker_main(void *ptr) {

	struct worker_info *wi = ptr;
	void *raw;
	http_parser parser;
	http_parser_settings settings;

	/* ignore sigpipe */
#ifdef SIGPIPE
	signal(SIGPIPE, SIG_IGN);
#endif

	/* setup http parser */
	memset(&settings, 0, sizeof(http_parser_settings));
	settings.on_path = http_parser_onpath;
	settings.on_url = http_parser_onurl;
	settings.on_header_field = http_parser_on_header_field;
	settings.on_header_value = http_parser_on_header_value;

	while(1) {
		http_action action;
		pthread_mutex_t mutex; /* mutex used in pthread_cond_wait */
		char buffer[64*1024]; 
		struct http_request req;

		/* init local variables */
		memset(&req, 0, sizeof(req));
		pthread_mutex_init(&mutex, NULL);

		/* get client fd */
		raw = queue_pop(wi->q);
		if(!raw) {
			pthread_cond_wait(wi->cond, &mutex);
			continue;
		}
		/* we can read data from the client, now. */
		req.cx = calloc(sizeof(struct connection), 1);
<<<<<<< Updated upstream
		/* printf("req.cx=%p\n", req.cx); */
=======
		printf("\n\nreq.cx = %p\n", req.cx);
>>>>>>> Stashed changes
		req.cx->fd = (int)(long)raw;
		cx_count(+1);

		size_t len = sizeof(buffer), nb_parsed;
		int nb_read;
		nb_read = recv(req.cx->fd, buffer, len, 0);

		/* fail, close. */
		if(nb_read < 0) {
<<<<<<< Updated upstream
			/* printf("calling cx_remove from %s:%d\n", __FILE__, __LINE__); */
=======
			printf("calling cx_remove from %s:%d\n", __FILE__, __LINE__);
>>>>>>> Stashed changes
			cx_remove(req.cx); /* byyyee */
			continue;
		}
		buffer[nb_read] = 0;

		/* special case! check if we've just received a request from a Flash client. */
		if(nb_read == 23 &&
			memcmp(buffer, "<policy-file-request/>", 23) == 0) {
			int ret = write(req.cx->fd, flash_xd, flash_xd_len-1);
			(void)ret;
			action = HTTP_DISCONNECT;
		} else {
			/* parse data using @ry’s http-parser library.
			 * → http://github.com/ry/http-parser/
			 */
			http_parser_init(&parser, HTTP_REQUEST);
			parser.data = &req;
			nb_parsed = http_parser_execute(&parser, settings,
					buffer, nb_read);

			if((int)nb_parsed < nb_read - 1) {
<<<<<<< Updated upstream
				/* printf("calling cx_remove from %s:%d\n", __FILE__, __LINE__); */
=======
			printf("calling cx_remove from %s:%d\n", __FILE__, __LINE__);
>>>>>>> Stashed changes
				cx_remove(req.cx);
				action = -1;
			} else {
				/* dispatch the client depending on the URL path */
				req.base = wi->base;
				/* printf("going to dispatch...\n"); */
				action = http_dispatch(&req);
			}
		}

		switch(action) {
			case HTTP_DISCONNECT:
<<<<<<< Updated upstream
				/* printf("calling cx_remove(%p) from %s:%d\n", req.cx, __FILE__, __LINE__); */
=======
				printf("action = HTTP_DISCONNECT (req.path=%s)\n", req.path);
				printf("calling cx_remove from %s:%d\n", __FILE__, __LINE__);
>>>>>>> Stashed changes
				cx_remove(req.cx);
				break;

			case HTTP_WEBSOCKET_MONITOR:
				printf("action = HTTP_WEBSOCKET_MONITOR\n");
				websocket_monitor(wi->base, req.cx, req.cx->channel, req.cx->cu);
				break;

			case HTTP_KEEP_CONNECTED:
				printf("action = HTTP_KEEP_CONNECTED\n");
				cx_monitor(req.cx);
				break;
		}

		/* cleanup */
		free(req.host); req.host = NULL; req.host_len = 0;
		free(req.origin); req.origin = NULL; req.origin_len = 0;
		free(req.path); req.path = NULL;

		free(req.get.name);
		free(req.get.data);
		free(req.get.jsonp);
		free(req.get.domain);
	}

	return NULL;
}

/**
 * Called when we can read an HTTP request from a client.
 */
void
on_client_data(int fd, short event, void *ptr) {

	free(ptr);
	if(event != EV_READ) {
		return;
	}

	/* push fd into queue so that it'll be handled by a worker. */
	queue_push(di.q, (void*)(long)fd);
	pthread_cond_signal(&di.cond);
}

/**
 * Called when we can accept(2) a connection from a client.
 */
static void
on_accept(int fd, short event, void *ptr) {

	(void)ptr;
	int client_fd, ret;
	struct event *ev;
	struct sockaddr addr;
	socklen_t addrlen;

	if(event != EV_READ) {
		return;
	}

	/* accept connection */
	addrlen = sizeof(addr);
	/* printf("\n\naccepting...\n"); */
	client_fd = accept(fd, &addr, &addrlen);
	/* printf("accept(on fd=%d) returned %d\n", fd, client_fd); */
	if(client_fd < 1) {
		/* failed accept, we need to stop accepting connections until we close a fd */
		/* printf("accept() returned %d: %s\n", client_fd, strerror(errno)); */
		syslog(LOG_WARNING, "FAIL! accept() returned %d: %m", client_fd);
		update_event(0);
		return;
	}

	/* add read event */
	ev = calloc(sizeof(struct event), 1);
	event_set(ev, client_fd, EV_READ, on_client_data, ev);
	ret = event_base_set(di.base, ev);
	if(ret == 0) {
		ret = event_add(ev, NULL);
		if(ret != 0) {
			free(ev);
			syslog(LOG_WARNING, "event_add() failed: %m");
			/* printf("event_add() failed: %s", strerror(errno)); */
			update_event(0);
		}
	} else {
		syslog(LOG_WARNING, "event_base_set() failed: %m");
		update_event(0);
	}
	update_event(EV_READ | EV_PERSIST);
}

int
update_event(int flags) {

	struct event_base *base = di.base;

	pthread_mutex_lock(&di.lock);

	/* printf("allow_accept(fd=%d): %s\n", di.fd, (flags == 0 ? "NO":"YES")); */
	if(di.ev_flags == flags) { /* no change in flags */
		/* printf("already in that mode.\n"); */
		goto success;
	}

	if(di.ev && flags == 0) {
		/* printf("event_del: ev=%p (%s:%d)\n", di.ev, __FILE__, __LINE__); */
		if(event_del(di.ev) == -1) {
			syslog(LOG_WARNING, "event_del() failed.");
			goto failure;
		}
		free(di.ev);
		di.ev = NULL;
		goto success;
	}

	free(di.ev);
	di.ev = calloc(sizeof(struct event), 1);
	event_set(di.ev, di.fd, flags, on_accept, NULL);
	event_base_set(base, di.ev);

	if(event_add(di.ev, 0) == -1) {
		/* printf("event_add() failed: %s\n", strerror(errno)); */
		syslog(LOG_WARNING, "event_add() failed: %m");
		di.ev_flags = 0;

		free(di.ev);
		di.ev = NULL;

		goto failure;
	} else {
		/* printf("Now accepting again on fd=%d!\n", di.fd); */
	}

success:
	di.ev_flags = flags;
	pthread_mutex_unlock(&di.lock);
	return 0;
failure:
	pthread_mutex_unlock(&di.lock);
	return -1;
}

/**
 * Server main function. Doesn't return as long as we're up.
 */
int
server_run(short nb_workers, const char *ip, short port) {

	int i, ret;
	struct event_base *base;
	struct queue_t *q;

	/* setup queue */
	q = queue_new();

	/* setup socket + libevent */
	ret = di.fd = socket_setup(ip, port);
	if(!ret) {
		return -1;
	}
	base = event_init();
	di.ev = calloc(sizeof(struct event), 1);
	event_set(di.ev, di.fd, EV_READ | EV_PERSIST, on_accept, NULL);
	event_base_set(base, di.ev);
	ret = event_add(di.ev, NULL);
	if(ret != 0) {
		return -1;
	}

	/* fill in dispatcher info */
	di.ev_flags = EV_READ | EV_PERSIST;
	di.base = base;
	di.q = q;
	pthread_cond_init(&di.cond, NULL);
	pthread_mutex_init(&di.lock, NULL);

	/* run workers */
	for(i = 0; i < nb_workers; ++i) {
		struct worker_info *wi = calloc(1, sizeof(struct worker_info));
		wi->q = q;
		wi->cond = &di.cond;
		wi->base = base;
		pthread_create(&wi->thread, NULL, worker_main, wi);
	}

	syslog(LOG_INFO, "Running.");

	while(1) {
		event_base_loop(base, 0);
		/* printf("ffuuuu- %s\n", strerror(errno)); // break; */
	}
}

