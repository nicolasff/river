#ifndef THREAD_H
#define THREAD_H

#include <pthread.h>

struct event;
struct event_base;
struct connection;

struct tq_worker {
	struct threaded_queue *tq;

	int recv_fd;
	int send_fd;

	struct event ev_pipe;
	struct event_base *base;
	pthread_t thread;
};

struct tq_conn_queue {
	int fd;
	struct tq_conn_queue *next;
};

struct threaded_queue {
	int fd;

	int (*cb_available_data)(struct connection *, void*);
	void *data;

	int thread_count;
	struct tq_worker *threads;
	int thread_next;

	struct event_base *base;
	struct event *ev;

	struct tq_conn_queue *cq;
	pthread_mutex_t lock;
};

struct threaded_queue *
tq_new(int fd, int count, int (*fun)(struct connection*, void*), void *ptr);

void
tq_loop(struct threaded_queue *tq);

void
tq_free(struct threaded_queue *tq);

void *
tqw_main(void *ptr);

void
tqw_pipe_event(int fd, short event, void *ptr);

void
tq_on_accept(int fd, short event, void *ptr);

void
tq_on_available_data(int fd, short event, void *ptr);

#endif
