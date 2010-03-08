#include <stdlib.h>
#include <stdio.h>
#include <sys/queue.h>
#include <event.h>
#include <evhttp.h>
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

#include "socket.h"
#include "server.h"
#include "queue.h"
#include "http-parser/http_parser.h"
#include "http_dispatch.h"

/**
 * Dispatch based on the path
 */
int
http_dispatch(struct http_request *req) {

	if(req->path_len == 18 && 0 == strncmp(req->path, "/meta/authenticate", 18)) {
		return http_dispatch_meta_authenticate(req);
	} else if(req->path_len == 13 && 0 == strncmp(req->path, "/meta/publish", 13)) {
		return http_dispatch_meta_publish(req);
	} else if(req->path_len == 13 && 0 == strncmp(req->path, "/meta/connect", 13)) {
		return http_dispatch_meta_connect(req);
	} else if(req->path_len == 15 && 0 == strncmp(req->path, "/meta/subscribe", 15)) {
		return http_dispatch_meta_subscribe(req);
	} else if(req->path_len == 16 && 0 == strncmp(req->path, "/meta/newchannel", 16)) {
		return http_dispatch_meta_newchannel(req);
	} 

	return 0;
}

/**
 * Copy the url
 */
int
http_parser_onurl(http_parser *parser, const char *at, size_t len) {
	
	struct http_request *req = parser->data;
	req->qs = calloc(1+len, 1);
	memcpy(req->qs, at, len);
	req->qs_len = len;

	const char *p = strchr(at, '?');

	if(!p) return 0;
	p++;

	while(1) {
		char *eq, *amp, *key, *val;
		size_t key_len, val_len;

		eq = memchr(p, '=', p - at + len);
		if(!eq) break;

		key_len = eq - p;
		key = calloc(1 + key_len, 1);
		memcpy(key, p, key_len);

		p = eq + 1;
		if(!*p) {
			free(key);
			break;
		}

		amp = memchr(p, '&', p - at + len);
		if(amp) {
			val_len = amp - p;
		} else {
			val_len = at + len - p;
		}

		val = calloc(1 + val_len, 1);
		memcpy(val, p, val_len);

		/* retrieve uid, sid. */
		/* TODO: do this in a callback function. */
		if(key_len == 3 && strncmp(key, "uid", 3) == 0) {
			req->uid = atol(val);
		} else if(key_len == 3 && strncmp(key, "sid", 3) == 0) {
			req->sid_len = val_len;
			req->sid = calloc(req->sid_len+1, 1);
			memcpy(req->sid, val, req->sid_len);
		} else if(key_len == 4 && strncmp(key, "name", 4) == 0) {
			req->name_len = val_len;
			req->name = calloc(req->name_len+1, 1);
			memcpy(req->name, val, req->name_len);
		} else if(key_len == 4 && strncmp(key, "data", 4) == 0) {
			req->data_len = val_len;
			req->data = calloc(req->data_len+1, 1);
			memcpy(req->data, val, req->data_len);
		}
		free(key);
		free(val);

		if(amp) {
			p = amp + 1;
		} else {
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

void *
worker_main(void *ptr) {

	struct worker_info *wi = ptr;
	void *raw;
	http_parser parser;
	http_parser_settings settings;

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
			close(req.fd); /* byyyee */
			continue;
		}

		/* dispatch the client depending on the URL path */
		int action = http_dispatch(&req);

		free(req.path);
		req.path = NULL;

		if(0 == action) {
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
	struct dispatcher_info *dispatcher = ptr;

	if(event != EV_READ) {
		return;
	}

	/* push fd into queue so that it'll be handled by a worker. */
	queue_push(dispatcher->q, (void*)(long)fd);
	pthread_cond_signal(&dispatcher->cond);
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
	struct event *ev_client_data;

	if(event != EV_READ) {
		return;
	}

	/* accept connection */
	addrlen = sizeof(addr);
	client_fd = accept(fd, &addr, &addrlen);

	/* add read event */
	ev_client_data = calloc(1, sizeof(struct event));
	event_set(ev_client_data, client_fd, EV_READ, on_client_data, di);
	event_base_set(di->base, ev_client_data);
	event_add(ev_client_data, NULL);
}

/**
 * Server main function. Doesn't return as long as we're up.
 */
int
server_run(short nb_workers, short port) {

	int fd, i;
	struct event_base *base;
	struct event ev;
	struct queue_t *q;
	struct dispatcher_info di;
	
	/* setup queue */
	q = queue_new();

	/* setup socket + libevent */
	fd = socket_setup(port);
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
		pthread_create(&wi->thread, NULL, worker_main, wi);
	}

	return event_base_dispatch(base);
}

