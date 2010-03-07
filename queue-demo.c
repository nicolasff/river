#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <event.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>


#include "queue.h"

#include <pthread.h>
#include "http-parser/http_parser.h"

#define THREAD_COUNT	8

struct dispatcher_data {
	struct event_base *base;
	struct queue_t *q;
	pthread_cond_t cond;
};


int
socket_create(short port) {

	struct sockaddr_in addr;
	int fd, reuse = 1;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	memset(&(addr.sin_addr), 0, sizeof(addr.sin_addr));
	addr.sin_addr.s_addr = htonl(0);

	fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	/* no error checking under there, beware. */
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	fcntl(fd, F_SETFD, O_NONBLOCK);
	bind(fd, (struct sockaddr*)&addr, sizeof(addr));
	listen(fd, SOMAXCONN);

	return fd;
}

/**
 * Called when an HTTP request is available for reading on the client socket
 */
void on_http_request(int fd, short event_type, void *ptr) {
	(void)event_type;
	struct dispatcher_data *dispatcher = ptr;

	/* push fd into queue so that it'll be handled by a worker. */
	queue_push(dispatcher->q, (void*)(long)fd);
	pthread_cond_signal(&dispatcher->cond);
}

/**
 * Called when we can accept the connection
 */
void on_accept(int fd, short event_type, void *ptr) {
	(void)event_type;

	int client_fd;
	struct sockaddr addr;
	socklen_t addrlen;

	struct event *ev_client_data;
	struct dispatcher_data *dispatcher = ptr;

	/* accept connection */
	addrlen = sizeof(addr);
	client_fd = accept(fd, &addr, &addrlen);

	/* prepare event: when client writes data, call on_http_request */
	ev_client_data = calloc(1, sizeof(struct event));
	event_set(ev_client_data, client_fd, EV_READ, on_http_request, dispatcher);
	event_base_set(dispatcher->base, ev_client_data);
	event_add(ev_client_data, NULL);
}


void *
worker_main(void *ptr) {

	int fd, ret;
	struct dispatcher_data *dispatcher = ptr;
	void *raw;
	http_parser parser;
	http_parser_settings settings;

	pthread_mutex_t mutex;
	pthread_mutex_init(&mutex, NULL);

	/* setup http parser */
	memset(&settings, 0, sizeof(http_parser_settings));
	http_parser_init(&parser, HTTP_REQUEST);

	while(1) {
		/* get client fd */
		raw = queue_pop(dispatcher->q);
		if(!raw) {
			pthread_cond_wait(&dispatcher->cond, &mutex);
			continue;
		}
		/* we can read data from the client, now. */
		fd = (int)(long)raw;

		char buffer[64*1024]; 
		size_t len = sizeof(buffer), nb_parsed;
		int nb_read;
		nb_read = recv(fd, buffer, len, 0);

		if(nb_read < 0) {
			close(fd); /* byyyee */
			continue;
		}

		nb_parsed = http_parser_execute(&parser, settings,
				buffer, 1+nb_read);

		if(nb_read != (int)nb_parsed) {
			close(fd); /* byyyee */
			continue;
		}

		ret = write(fd, "Hello, world.\r\n", 15);
		close(fd);
	}

	return NULL;
}


int
main() {

	int server_fd, i;
	pthread_t threads[THREAD_COUNT];
	struct dispatcher_data dispatcher;

	struct event ev;

	/* create queue */
	dispatcher.q = queue_new();
	
	pthread_cond_init(&dispatcher.cond, NULL);

	/* create worker threads and start them */
	for(i = 0; i < THREAD_COUNT; ++i) {
		pthread_create(&threads[i], NULL, worker_main, &dispatcher);
	}

	/* create server */
	server_fd = socket_create(1111);

	/* setup libevent */
	dispatcher.base = event_base_new();
	event_set(&ev, server_fd, EV_READ|EV_PERSIST, on_accept, &dispatcher);
	event_base_set(dispatcher.base, &ev);
	event_add(&ev, NULL);

	/* GO! */
	event_base_dispatch(dispatcher.base);

	return EXIT_SUCCESS;
}

