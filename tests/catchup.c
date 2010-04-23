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

struct timer {

	struct event_base *base;
	struct event *ev;
	void (*fun)();

	struct timeval tv;

};

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
		t->fun();
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
void on_writer_tick() {
	printf("WRITER TICK\n");
}
void*
writer_main(void *ptr) {
	const char *chan = ptr;
	(void)chan;

	struct timer *t = timer_new(1000000, on_writer_tick);

	add_timer(t);
	event_base_dispatch(t->base);

	return NULL;
}

/* reader */

void on_reader_tick() {
	printf("READER TICK\n");
}
void*
reader_main(void *ptr) {
	const char *chan = ptr;
	(void)chan;

	struct timer *t = timer_new(5000000, on_reader_tick);

	add_timer(t);
	event_base_dispatch(t->base);

	return NULL;
}


int
main(int argc, char *argv[]) {
	(void)argc;
	(void)argv;
	struct timespec t0, t1;

	clock_gettime(CLOCK_MONOTONIC, &t0);
	srand(time(NULL));

	char *chan = channel_make_name();

	pthread_t reader_thread;
	pthread_create(&reader_thread, NULL, reader_main, chan);

	pthread_t writer_thread;
	pthread_create(&writer_thread, NULL, writer_main, chan);

	/* timing */
	clock_gettime(CLOCK_MONOTONIC, &t1);
	float mili0 = t0.tv_sec * 1000 + t0.tv_nsec / 1000000;
	float mili1 = t1.tv_sec * 1000 + t1.tv_nsec / 1000000;
	(void)mili0;
	(void)mili1;
	/*printf("Read %d messages in %0.2f sec: %0.2f/sec\n", request_count, (mili1-mili0)/1000.0, 1000*(float)request_count/(mili1-mili0));*/

	pthread_join(writer_thread, NULL);
	pthread_join(reader_thread, NULL);

	return EXIT_SUCCESS;
}

