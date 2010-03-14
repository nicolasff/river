#ifndef COMET_TEST_SERVER_H
#define COMET_TEST_SERVER_H
#include <event.h>

struct event_base;
struct queue_t;

struct event_callback_data {
	struct dispatcher_info *di;
	struct event ev;
};

struct event_timeout_data {
	struct event ev;
	struct timeval tv;
};

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

int
server_run(short nb_workers, short port);

#endif /* COMET_TEST_SERVER_H */

