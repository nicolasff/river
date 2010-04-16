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

	while(1) {
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
		req.fd = (int)(long)raw;
		size_t len = sizeof(buffer), nb_parsed;
		int nb_read;
		nb_read = recv(req.fd, buffer, len, 0);

		/* fail, close. */
		if(nb_read < 0) {
			printf("CLOSE %d (%s:%d)\n", req.fd, __FILE__, __LINE__);
			close(req.fd); /* byyyee */
			continue;
		}
		buffer[nb_read] = 0;


		/* parse data using @ry’s http-parser library.
		 * → http://github.com/ry/http-parser/
		 */
		http_parser_init(&parser, HTTP_REQUEST);
		parser.data = &req;
		nb_parsed = http_parser_execute(&parser, settings,
				buffer, nb_read);

		if(nb_read != (int)nb_parsed) {
			printf("CLOSE %d (%s:%d)\n", req.fd, __FILE__, __LINE__);
			close(req.fd); /* byyyee */
			continue;
		}

		/* dispatch the client depending on the URL path */
		req.base = wi->base;
		int action = http_dispatch(&req);

		/* cleanup */
		free(req.path); req.path = NULL;
		dictRelease(req.get);

		if(0 == action) {
			printf("CLOSE %d (%s:%d)\n", req.fd, __FILE__, __LINE__);
			close(req.fd);
		}
	}

	return NULL;
}

/**
 * Called when we can read an HTTP request from a client.
 */
void
on_client_data(int fd, short event, void *ptr) {
	struct event_callback_data *cb_data = ptr;
	struct dispatcher_info *di = cb_data->di;

	if(event != EV_READ) {
		free(cb_data);
		return;
	}

	/* push fd into queue so that it'll be handled by a worker. */
	queue_push(di->q, (void*)(long)fd);
	pthread_cond_signal(&di->cond);
	free(cb_data);
}

/**
 * Called when we can accept(2) a connection from a client.
 */
void
on_accept(int fd, short event, void *ptr) {

	struct dispatcher_info *di = ptr;
	int client_fd;
	struct sockaddr addr;
	socklen_t addrlen;
	struct event_callback_data *cb_data;

	if(event != EV_READ) {
		return;
	}

	/* accept connection */
	addrlen = sizeof(addr);
	client_fd = accept(fd, &addr, &addrlen);

	/* add read event */
	cb_data = calloc(1, sizeof(struct event_callback_data));
	cb_data->di = di;
	event_set(&cb_data->ev, client_fd, EV_READ, on_client_data, cb_data);
	event_base_set(di->base, &cb_data->ev);
	event_add(&cb_data->ev, NULL);
}

/**
 * Server main function. Doesn't return as long as we're up.
 */
int
server_run(short nb_workers, const char *ip, short port) {

	int fd, i;
	struct event_base *base;
	struct event ev;
	struct queue_t *q;
	struct dispatcher_info di;
	
	/* setup queue */
	q = queue_new();

	/* setup socket + libevent */
	fd = socket_setup(ip, port);
	base = event_init();
	event_set(&ev, fd, EV_READ | EV_PERSIST, on_accept, &di);
	event_base_set(base, &ev);
	event_add(&ev, NULL); 

	/* fill in dispatcher info */
	di.base = base;
	di.q = q;
	pthread_cond_init(&di.cond, NULL);

	/* run workers */
	for(i = 0; i < nb_workers; ++i) {
		struct worker_info *wi = calloc(1, sizeof(struct worker_info));
		wi->q = q;
		wi->cond = &di.cond;
		wi->base = base;
		pthread_create(&wi->thread, NULL, worker_main, wi);
	}

	return event_base_dispatch(base);
}

