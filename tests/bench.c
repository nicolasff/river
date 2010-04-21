#include <stdlib.h>
#include <stdio.h>

#include <event.h>
#include <evhttp.h>
#include <unistd.h>

int remaining_requests = 10000;

struct message_chunk {
	char *data;
	size_t sz;
	struct message_chunk *next;
};
struct comet_message {
	struct event_base        *base;
	struct evhttp_connection *evcon;
	struct message_chunk     *first;
	struct message_chunk     *last;
};

void
queue_request(struct event_base *base);


void
on_http_response(struct evhttp_request *req, void *ptr){

	struct comet_message *cm = ptr;
	printf("on_http_response finished (%d remain).\n", remaining_requests);
	if(!req || !cm || !cm->first) {
		return;
	}

	if(req->response_code != HTTP_OK) {
		printf("FAIL (response_code=%d)\n", req->response_code);
	} else {
		size_t total = 0;
		struct event_base *base = cm->base;
		struct message_chunk *mc, *mc_next;
		for(mc = cm->first; mc; mc = mc_next) {
			total += mc->sz;
			mc_next = mc->next;
			free(mc->data);
			free(mc);
		}
		free(cm);
		evhttp_connection_free(cm->evcon);
		// printf("SUCCESS: got %zd bytes\n", total);
		remaining_requests--;
		if(remaining_requests > 0) {
			queue_request(base);
		} else {
			_exit(0);
		}
	}
}

void
on_http_chunk(struct evhttp_request *req, void *ptr) {

	struct comet_message *cm = ptr;

	if(req->response_code != HTTP_OK) {
		printf("CHUNK FAIL (ret=%d)\n", req->response_code);
	} else {
		struct message_chunk *mc = calloc(1, sizeof(struct message_chunk));
		if(cm->last == NULL) {
			cm->first = cm->last = mc;
		} else {
			cm->last->next = mc;
			cm->last = mc;
		}
		mc->sz = EVBUFFER_LENGTH(req->input_buffer);
		/*
		mc->data = calloc(mc->sz, 1);
		evbuffer_remove(req->input_buffer, mc->data, mc->sz);
		printf("CHUNK SUCCESS: %zd\n", mc->sz);
		write(1, mc->data, mc->sz);
		printf("\n");
		*/
	}
}

void
queue_request(struct event_base *base) {

	struct comet_message *cm = calloc(1, sizeof(struct comet_message));

	cm->evcon = evhttp_connection_new("127.0.0.1", 1234);
	struct evhttp_request *evreq = evhttp_request_new(on_http_response, cm);

	evhttp_connection_set_base(cm->evcon, base);
	cm->base = base;
	evhttp_request_set_chunked_cb(evreq, on_http_chunk);
	evhttp_make_request(cm->evcon, evreq, EVHTTP_REQ_GET, "/iframe"); // "/subscribe?name=lol");
}

int
main(int argc, char *argv[]) {
	(void)argc;
	(void)argv;

	int i, client_count = 100;

	struct event_base *base = event_base_new();

	for(i = 0; i < client_count; ++i) {
		queue_request(base);
	}

	event_base_dispatch(base);

	return EXIT_SUCCESS;
}

