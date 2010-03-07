#ifndef QUEUE_LOCKS_H
#define QUEUE_LOCKS_H

#include <pthread.h>

struct node_t {
	void *data;
	struct node_t *next;
};

struct queue_t {

	struct node_t *head;
	struct node_t *tail;

	pthread_spinlock_t h_lock;
	pthread_spinlock_t t_lock;
};

struct queue_t *
queue_new();

void
queue_push(struct queue_t *q, void *data);

void *
queue_pop(struct queue_t *q);

#endif /* QUEUE_LOCKS_H */
