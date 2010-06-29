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

/* reader thread, with counter of remaining messages */
struct reader_thread {
	struct host_info *hi;
	int reader_per_chan;
	struct event_base *base;

	char **channels;
	int channel_count;

	int request_count;
	int byte_count;
};

struct writer_thread {
	struct host_info *hi;
	int writer_per_chan;
	struct event_base *base;

	char **channels;
	int channel_count;

	int byte_count;
	unsigned int current_channel;

	char *url;
};

/* readers */
void
process_message(struct reader_thread *rt, size_t sz) {

	if(rt->request_count == 0) {
		return;
	}

	rt->byte_count += sz;
	if(rt->request_count % 10000 == 0) {
		printf("%8d messages left (got %9d bytes so far).\n",
			rt->request_count, rt->byte_count);
	}

	/* decrement read count, and stop receiving when we reach zero. */
	rt->request_count--;
	if(rt->request_count == 0) {
		event_base_loopexit(rt->base, NULL);
	}
}

/**
 * Called when a message is sent on the HTTP channel pipe.
 */
void
on_http_message_chunk(struct evhttp_request *req, void *ptr) {

	struct reader_thread *rt = ptr;

	if(req->response_code == HTTP_OK) {
		size_t sz = EVBUFFER_LENGTH(req->input_buffer);
		process_message(rt, sz);
	} else {
		fprintf(stderr, "CHUNK FAIL (ret=%d)\n", req->response_code);
	}
}

/**
 * Called when a channel pipe is closed.
 */
void
on_end_of_subscribe(struct evhttp_request *req, void *nil){
	(void)nil;

	if(!req || req->response_code != HTTP_OK) {
		/* fprintf(stderr, "subscribe FAIL (response_code=%d)\n", req->response_code); */
	}
	printf("end of subscribe\n");
}

/**
 * pthread entry point, running the reader event base.
 */
void *
comet_run_readers(void *ptr) {

	struct reader_thread *rt = ptr;
	int i, j;
	rt->base = event_base_new();

	for(i = 0; i < rt->channel_count; ++i) {
		char *chan = rt->channels[i];
		char *url = calloc(strlen(chan) + 17, 1);
		sprintf(url, "/subscribe?name=%s", chan);

		for(j = 0; j < rt->reader_per_chan; ++j) {

			struct evhttp_connection *evcon = evhttp_connection_new(rt->hi->host, rt->hi->port);
			struct evhttp_request *evreq = evhttp_request_new(on_end_of_subscribe, rt);

			evhttp_connection_set_base(evcon, rt->base);
			evhttp_request_set_chunked_cb(evreq, on_http_message_chunk);
			evhttp_make_request(evcon, evreq, EVHTTP_REQ_GET, url);
			/* printf("[SUBSCRIBE: http://127.0.0.1:1234%s]\n", url); */
		}
	}
	event_base_dispatch(rt->base);
	return NULL;
}

/* writers */

void
publish(struct writer_thread *wt, const char *url);


/**
 * Called after a publish query has returned.
 */
void
on_end_of_publish(struct evhttp_request *req, void *ptr){
	(void)req;

	struct writer_thread *wt = ptr;

	/* enqueue another publication */
	wt->current_channel++;

	char *channel = wt->channels[wt->current_channel % wt->channel_count];
	char template[] = "/publish?name=%s&data=hello-world";
	char *url = calloc(strlen(channel) + sizeof(template), 1);
	sprintf(url, template, channel);
	publish(wt, url);
}

/**
 * Enqueue an HTTP request to /publish in the writer thread's event base.
 */
void
publish(struct writer_thread *wt, const char *url) {
	struct evhttp_connection *evcon = evhttp_connection_new(wt->hi->host, wt->hi->port);
	struct evhttp_request *evreq = evhttp_request_new(on_end_of_publish, wt);

	evhttp_connection_set_base(evcon, wt->base);
	evhttp_make_request(evcon, evreq, EVHTTP_REQ_GET, url);

//	printf("[PUBLISH: http://127.0.0.1:1234%s] : ret=%d\n", url, ret);
}

/**
 * pthread entry point, running the writer event base.
 */
void *
comet_run_writers(void *ptr) {

	int i, j;

	struct writer_thread *wt = ptr;
	wt->base = event_base_new();

	for(i = 0; i < wt->channel_count; ++i) {
		char template[] = "/publish?name=%s&data=hello-world";
		char *url = calloc(strlen(wt->channels[i]) + sizeof(template), 1);
		sprintf(url, template, wt->channels[i]);

		for(j = 0; j < wt->writer_per_chan; ++j) {
			publish(wt, url);
		}
	}
	event_base_dispatch(wt->base);

	free(wt->url);
	return NULL;
}

int
main(int argc, char *argv[]) {
	(void)argc;
	(void)argv;
	struct timespec t0, t1;

	clock_gettime(CLOCK_MONOTONIC, &t0);
	srand(time(NULL));

	int reader_per_chan = 400;
	int writer_per_chan = 40;
	int request_count = 1000000;
	int channel_count = 2;
	int i;

	char **channels = calloc(channel_count, sizeof(char*));
	for(i = 0; i < channel_count; ++i) {
		channels[i] = channel_make_name();
	}

	struct host_info hi;
	hi.host = "127.0.0.1";
	hi.port = 1234;

	printf("Using %d channels.\n", channel_count);

	/* run readers */
	printf("Running %d readers (%d per chan).\n", reader_per_chan * channel_count, reader_per_chan);
	pthread_t reader_thread;
	struct reader_thread rt;
	rt.channels = channels;
	rt.channel_count = channel_count;
	rt.hi = &hi;
	rt.reader_per_chan = reader_per_chan;
	rt.request_count = request_count;
	rt.byte_count = 0;
	pthread_create(&reader_thread, NULL, comet_run_readers, &rt);


	/* run writers */
	printf("Running %d writers (%d per chan).\n", writer_per_chan * channel_count, writer_per_chan);
	pthread_t writer_thread;
	struct writer_thread wt;
	wt.channels = channels;
	wt.channel_count = channel_count;
	wt.hi = &hi;
	wt.writer_per_chan = writer_per_chan;
	pthread_create(&writer_thread, NULL, comet_run_writers, &wt);

	/* wait for readers to finish */
	pthread_join(reader_thread, NULL);

	/* timing */
	clock_gettime(CLOCK_MONOTONIC, &t1);
	float mili0 = t0.tv_sec * 1000 + t0.tv_nsec / 1000000;
	float mili1 = t1.tv_sec * 1000 + t1.tv_nsec / 1000000;
	printf("Read %d messages in %0.2f sec: %0.2f/sec\n", request_count, (mili1-mili0)/1000.0, 1000*(float)request_count/(mili1-mili0));

	for(i = 0; i < channel_count; ++i) {
		free(channels[i]);
	}
	free(channels);
	return EXIT_SUCCESS;
}

