#ifndef COMET_SERVER_H
#define COMET_SERVER_H
#include <event.h>

struct event_base;
struct queue_t;

struct event_callback_data {
	struct event ev;
};

struct worker_info {
	pthread_t 	thread;
	struct event_base	*base; /* the only event base. */

	pthread_cond_t	*cond;
	struct queue_t 	*q;
};

struct dispatcher_info {

	int fd;
	struct event ev;
	int ev_flags;

	struct event_base	*base;
	pthread_cond_t 		cond;
	struct queue_t 		*q;
};

int
server_run(short nb_workers, const char *ip, short port);

void
socket_shutdown(int fd);

#endif /* COMET_SERVER_H */

