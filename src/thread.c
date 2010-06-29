#include <event.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "thread.h"
#include "socket.h"
#include "channel.h"

struct threaded_queue *
tq_new(int fd, int count, int (*fun)(struct connection*, void*), void *ptr) {

	struct threaded_queue *tq = calloc(sizeof(struct threaded_queue), 1);
	tq->fd = fd;
	tq->data = ptr;
	tq->thread_count = count;
	tq->threads = calloc(sizeof(struct tq_worker), count);

	tq->cb_available_data = fun;

	tq->base = event_base_new();
	tq->ev = calloc(sizeof(struct event), 1);

	event_set(tq->ev, fd, EV_READ | EV_PERSIST, tq_on_accept, tq);
	event_base_set(tq->base, tq->ev);
	event_add(tq->ev, NULL);

	pthread_mutex_init(&tq->lock, NULL);

	return tq;
}

void
tq_loop(struct threaded_queue *tq) {

	int i;
	for(i = 0; i < tq->thread_count; ++i) {
		
		struct tq_worker *tqw = &tq->threads[i];
		int com[2], ret;

		tqw->tq = tq;
		tqw->base = event_base_new();

		/* setup pipe */
		ret = pipe(com);
		(void)ret;
		tqw->recv_fd = com[0];
		tqw->send_fd = com[1];
		event_set(&tqw->ev_pipe, tqw->recv_fd, EV_READ | EV_PERSIST, tqw_pipe_event, tqw);
		event_base_set(tqw->base, &tqw->ev_pipe);
		event_add(&tqw->ev_pipe, NULL);

		/* start thread */
		pthread_create(&tq->threads[i].thread, NULL, tqw_main, &tq->threads[i]);
	}

	event_base_dispatch(tq->base);
}

void *
tqw_main(void *ptr) {
	struct tq_worker *tqw = ptr;

	/* ignore sigpipe */
#ifdef SIGPIPE
	signal(SIGPIPE, SIG_IGN);
#endif

	/* Each thread has its own event base. */
	event_base_dispatch(tqw->base);

	return NULL;
}

void
tqw_pipe_event(int fd, short event, void *ptr) {
	(void)event;

	struct tq_worker *tqw = ptr;
	struct threaded_queue *tq = tqw->tq;
	struct tq_conn_queue *cq;
	struct connection *cx;
	char c = 0;
	int client_fd, ret;

	ret = read(fd, &c, 1);
	if(ret != 1 || c != 1) {
		return;
	}

	/* pop the FD from the connection queue */
	pthread_mutex_lock(&tq->lock);
	cq = tq->cq;
	tq->cq = cq->next;
	pthread_mutex_unlock(&tq->lock);
	client_fd = cq->fd;
	free(cq);

	//printf("thread %d: got the client with fd=%d\n", (int)pthread_self(), client_fd);

	/* start monitoring that fd for READ events. */
	cx = calloc(sizeof(struct connection), 1);

	/* cb data */
	cx->data = tq->data;
	cx->cb_available_data = tq->cb_available_data;
	cx->fd = client_fd;

	/* read events on the client */
	cx->ev = calloc(sizeof(struct event), 1);
	cx->base = tqw->base;
	event_set(cx->ev, client_fd, EV_READ, tq_on_available_data, cx);
	event_base_set(cx->base, cx->ev);
	if(event_add(cx->ev, NULL) != 0) {
		free(cx->ev);
		free(cx);
	}
}

void
tq_on_accept(int fd, short event, void *ptr) {
	(void)event;

	struct threaded_queue *tq = ptr;
	struct sockaddr_in addr;
	socklen_t addr_sz = sizeof(addr);
	int client_fd, ret;
	struct tq_worker *tqw;

	client_fd = accept(fd, (struct sockaddr*)&addr, &addr_sz);
	//printf("accepted client_fd=%d\n", client_fd);

	/* add fd to connection queue */
	struct tq_conn_queue *cq = calloc(sizeof(struct tq_conn_queue), 1);
	cq->fd = client_fd;
	pthread_mutex_lock(&tq->lock);
	cq->next = tq->cq;
	tq->cq = cq;

	/* pick the next thread and have it handle that client */
	tqw = &tq->threads[(tq->thread_next++) % tq->thread_count];
	pthread_mutex_unlock(&tq->lock);
	ret = write(tqw->send_fd, "\1", 1); /* wake up thread using its pipe */
	(void)ret;
}

void
tq_on_available_data(int fd, short event, void *ptr) {
	(void)event;
	(void)fd;

	int ret;
	struct connection *cx = ptr;

	// printf("tq_on_available_data(cx=%p)\n", cx);

	ret = cx->cb_available_data(cx, cx->data);
	if(ret <= 0) {
		struct channel *chan = cx->channel;
		if(chan) {
			CHANNEL_LOCK(chan);
		}
		/* printf("calling cx_remove(%p) from %s:%d\n", cx, __FILE__, __LINE__); */
		cx_remove(cx);
		if(chan) {
			CHANNEL_UNLOCK(chan);
		}
	} else {
		/* start monitoring the connection */
		event_set(cx->ev, cx->fd, EV_READ, tq_on_available_data, cx);
		event_base_set(cx->base, cx->ev);
		event_add(cx->ev, NULL);
	}
}

void
tq_free(struct threaded_queue *tq) {

	int i;

	event_base_loopexit(tq->base, NULL);
	event_del(tq->ev);
	free(tq->base);
	for(i = 0; i < tq->thread_count; ++i) {
		pthread_detach(tq->threads[i].thread);
	}
	free(tq->threads);
	free(tq);
}

