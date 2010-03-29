#include "queue.h"

#include <stdlib.h>

struct queue_t *
queue_new() {

	struct queue_t *q = calloc(1, sizeof(struct queue_t));
	struct node_t *n = calloc(1, sizeof(struct node_t));

	pthread_spin_init(&q->h_lock, PTHREAD_PROCESS_SHARED);
	pthread_spin_init(&q->t_lock, PTHREAD_PROCESS_SHARED);

	q->head = q->tail = n;

	return q;
}


void
queue_push(struct queue_t *q, void *data) {

	struct node_t *n = calloc(1, sizeof(struct node_t));
	n->data = data;
	n->next = NULL;

	pthread_spin_lock(&q->t_lock);
	q->tail->next = n;
	q->tail = n;
	pthread_spin_unlock(&q->t_lock);
}

void *
queue_pop(struct queue_t *q) {

	struct node_t *n;
	void *ret;

	pthread_spin_lock(&q->h_lock);
	n = q->head;
	if(NULL == n->next) {
		pthread_spin_unlock(&q->h_lock);
		return NULL;
	}
	ret = n->next->data;
	q->head = n->next;
	pthread_spin_unlock(&q->h_lock);
	free(n);

	return ret;
}
