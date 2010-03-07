#ifndef COMET_TEST_SERVER_H
#define COMET_TEST_SERVER_H

struct event_base;
struct queue_t;

struct worker_info {
	pthread_t 	thread;
	pthread_cond_t	*cond;
	struct queue_t 	*q;
};

struct dispatcher_info {

	struct event_base	*base;
	pthread_cond_t 		cond;
	struct queue_t 		*q;
};

struct http_request {

	int fd;

	char *path;
	size_t path_len;

	char *sid;
	size_t sid_len;

	char *qs; /* query string */
	size_t qs_len;
};

int
server_run(short nb_workers, short port);

#endif /* COMET_TEST_SERVER_H */

