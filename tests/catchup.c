#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <event.h>
#include <evhttp.h>

/**
 * Creates a generic channel name
 */
char *
channel_make_name() {
	char *out = calloc(21, 1);
	snprintf(out, 20, "chan-%d", rand());
	return out;
}

struct host_info {
	char *host;
	short port;
};

struct timer {

	struct event_base *base;
	struct event *ev;
	void (*fun)(struct timer *);

	struct timeval tv;
	int seq;
	struct host_info *hi;
};

/* http */
void
enqueue_http_request(const char *host, short port,
		struct event_base *base, const char *url,
		void (*cb_end)(struct evhttp_request *, void *), void *ptr) {

	struct evhttp_connection *evcon = evhttp_connection_new(host, port);
	struct evhttp_request *evreq = evhttp_request_new(cb_end, ptr);

	evhttp_connection_set_base(evcon, base);
	evhttp_make_request(evcon, evreq, EVHTTP_REQ_GET, url);
}



/* timer */
struct timer *
timer_new(int usec, void (*fun)()) {
	
	struct timer *t = calloc(1, sizeof(struct timer));
	t->tv.tv_sec = 0;
	t->tv.tv_usec = usec;

	t->base = event_base_new();
	t->ev = calloc(1, sizeof(struct event));
	t->fun = fun;

	return t;
}

void
add_timer(struct timer *t);

void
on_timer_tick(int fd, short event, void *ptr) {
	(void)fd;
	(void)event;

	struct timer *t = ptr;
	if(t->fun) {
		t->fun(t);
	}
	add_timer(t);
}

void
add_timer(struct timer *t) {

	evtimer_set(t->ev, on_timer_tick, t);
	event_base_set(t->base, t->ev);
	evtimer_add(t->ev, &t->tv);
}

/* writer */
void on_writer_sent_http(struct evhttp_request *req, void *ptr) {
	(void)req;
	(void)ptr;

	printf("sent write query\n");
}

void on_writer_tick(struct timer *t) {
	/* send write request */
	const char *url = "/publish?name=chan&data=hello-world";
	enqueue_http_request(t->hi->host, t->hi->port, t->base, url, on_writer_sent_http, NULL);
}
void*
writer_main(void *ptr) {
	struct timer *t;

	t = timer_new(100000, on_writer_tick);
	t->hi = ptr;

	add_timer(t);
	event_base_dispatch(t->base);

	return NULL;
}

/* reader */

void on_reader_sent_http(struct evhttp_request *req, void *ptr) {
	(void)req;

	struct timer *t = ptr;
	char *buf, *pos, *old_pos;
	size_t sz;

	sz = EVBUFFER_LENGTH(req->input_buffer);
	buf = calloc(sz + 1, 1);
	evbuffer_remove(req->input_buffer, buf, sz);
	printf("got read reply. ");
	fflush(stdout);
	if(t->seq != 0) {
		printf("I expect the first message to have sequence number %d. [\n%s\n]\n\n", t->seq+1, buf);
	} else {
		printf("[\n%s\n]\n\n", buf);
	}

	/* look for the sequence number in the JSON buffer. */
	old_pos = pos = buf;
	while(pos) {
		old_pos = pos;
		pos = strstr(pos + 1, "seq\": ");
	}
	t->seq = atol(old_pos + sizeof("seq\":"));
	free(buf);
}
void on_reader_tick(struct timer *t) {
	/* send read request */
	const char format[] = "/subscribe?name=chan&keep=0&seq=%d";
	char *url = calloc(10 + sizeof(format), 1);
	sprintf(url, format, t->seq);
	printf("reading [%s]\n", url);
	enqueue_http_request(t->hi->host, t->hi->port, t->base, url, on_reader_sent_http, t);
	free(url);
}
void*
reader_main(void *ptr) {

	struct timer *t = timer_new(1000000, on_reader_tick);
	t->hi = ptr;

	add_timer(t);
	event_base_dispatch(t->base);

	return NULL;
}


int
main(int argc, char *argv[]) {
	(void)argc;
	(void)argv;

	struct host_info hi;
	hi.host = "127.0.0.1";
	hi.port = 1234;

	pthread_t reader_thread;
	pthread_create(&reader_thread, NULL, reader_main, &hi);

	pthread_t writer_thread;
	pthread_create(&writer_thread, NULL, writer_main, &hi);

	pthread_join(writer_thread, NULL);
	pthread_join(reader_thread, NULL);

	return EXIT_SUCCESS;
}

