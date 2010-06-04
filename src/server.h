#ifndef COMET_SERVER_H
#define COMET_SERVER_H
#include <event.h>

struct event_base;
struct queue_t;
struct connection;

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

void
connection_free(struct connection *cx);

int
server_run(short nb_workers, const char *ip, short port);

void
socket_shutdown(struct connection *cx);

int
update_event(int flags);

#endif /* COMET_SERVER_H */

