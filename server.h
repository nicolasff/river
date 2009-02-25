#ifndef COMET_TEST_SERVER_H
#define COMET_TEST_SERVER_H

struct evhttp_request;
struct evhttp;
struct event_base;

struct thread_info {
	int id;
	int fd;
	
	int pipe[2];

	int clients;
	struct evhttp 		*ev_http;
	struct event_base	*ev_base;
	pthread_t 		thread;
};

struct thread_info	*worker_threads;

int
server_start(short nb_workers, short port, short udp_port);

#endif /* COMET_TEST_SERVER_H */

