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

#if 0

/**
 * A message has been sent to a user, and a thread woke us up.
 * Let's see which client is affected.
 */
void
client_data_available(int fd, short event, void *arg) {

	struct p_user *user;
	struct p_message *m;

	struct thread_info *self = arg;
	if(fd != self->pipe[0] || event != EV_READ) {
		return;
	}

	/* read data from the pipe, a raw pointer to p_connection. */
	read(fd, &u_connection, sizeof(u_connection));

	/* find corresponding user */
	user = u_connection->user;
	if(!user) {
		return;
	}
	
	/* locking user here. */
	pthread_mutex_lock(&(user->lock));

		/* sending data. */
		for(m = u_connection->inbox_first; m;) {
			struct p_message *m_next = m->next;
			/* send data */
			evhttp_send_reply_chunk(u_connection->ev, m->data);

			message_free(m);

			m = m_next;
		}
		u_connection->inbox_first = NULL;
		u_connection->inbox_last = NULL;

	/* unlock user */
	pthread_mutex_unlock(&(user->lock));
}

#endif

/**
 * Dispatch based on the path
 */
int
http_dispatch(struct http_request *req) {

	// printf("dispatch, req->path = [%s]\n", req->path);
	if(req->path_len == 18 && 0 == strncmp(req->path, "/meta/authenticate", 18)) {
	//	return http_dispatch_meta_authenticate(&req);
	} else if(req->path_len == 13 && 0 == strncmp(req->path, "/meta/connect", 13)) {
	//	return http_dispatch_meta_connect(&req);
	} else if(req->path_len == 15 && 0 == strncmp(req->path, "/meta/subscribe", 15)) {
	//	return http_dispatch_meta_subscribe(&req);
	} else if(req->path_len == 17 && 0 == strncmp(req->path, "/meta/unsubscribe", 17)) {
	//	return http_dispatch_meta_unsubscribe(&req);
	} else if(req->path_len == 16 && 0 == strncmp(req->path, "/meta/newchannel", 16)) {
	//	return http_dispatch_meta_newchannel(&req);
	} 

	return -1;
}

/**
 * Copy the query string
 */
int
http_parser_onquerystring(http_parser *parser, const char *at, size_t len) {
	
	struct http_request *req = parser->data;
	req->qs = calloc(1+len, 1);
	memcpy(req->qs, at, len);
	req->qs_len = len;

	printf("qs=[%s]\n", req->qs);
	
	const char *p = at;

	while(1) {
		char *eq, *amp, *key, *val;
		size_t key_len, val_len;

		eq = memchr(p, '=', p - at + len);
		if(!eq) break;

		key_len = eq - p;
		key = calloc(1 + key_len, 1);
		memcpy(key, p, key_len);
		printf("key=[%s](%lu)\n", key, key_len);

		p = eq + 1;
		if(!*p) {
			free(key);
			break;
		}

		amp = memchr(p, '&', p - at + len);
		if(!amp) break;

		val_len = amp - p;
		val = calloc(1 + val_len, 1);
		memcpy(val, p, val_len);

		printf("key=[%s](%lu), val=[%s](%lu)\n",
			key, key_len, val, val_len);

		p = amp + 1;

		free(key);
		free(val);
	}

	return len;
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

	return len;
}

void *
worker_main(void *ptr) {

	int ret;
	struct worker_info *wi = ptr;
	void *raw;
	http_parser parser;
	http_parser_settings settings;

	/* setup http parser */
	memset(&settings, 0, sizeof(http_parser_settings));
	http_parser_init(&parser, HTTP_REQUEST);
	settings.on_path = http_parser_onpath;
	settings.on_query_string = http_parser_onquerystring;
	settings.on_url = http_parser_onquerystring;
	settings.on_fragment = http_parser_onquerystring;
	settings.on_header_field = http_parser_onquerystring;
	settings.on_header_value = http_parser_onquerystring;

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
		parser.data = &req;
		nb_parsed = http_parser_execute(&parser, settings,
				buffer, 1+nb_read);

		if(0 && nb_read != (int)nb_parsed) {
			write(1, buffer, nb_read+1);
			close(req.fd); /* byyyee */
			continue;
		}

		/* dispatch the client depending on the URL path */
		// http_dispatch(&req);

		free(req.path);
		req.path = NULL;

		/* reply to the client */
		ret = write(req.fd, "Hello, world.\r\n", 15);
		close(req.fd);
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

