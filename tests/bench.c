#include <stdlib.h>
#include <stdio.h>

#include <event.h>
#include <evhttp.h>

struct message_chunk {
	char *data;
	size_t sz;
	struct message_chunk *next;
};
struct comet_message {
	struct message_chunk *first;
	struct message_chunk *last;
};

void on_http_response(struct evhttp_request *req, void *ptr){

	struct comet_message *cm = ptr;
	printf("on_http_response finished.\n");

	if(req->response_code != HTTP_OK) {
		printf("FAIL\n");
	} else {
		size_t total = 0;
		struct message_chunk *mc;
		for(mc = cm->first; mc; mc = mc->next) {
			total += mc->sz;
		}
		printf("SUCCESS: got %d bytes\n", (int)total);
	}
}

void
on_http_chunk(struct evhttp_request *req, void *ptr) {

	struct comet_message *cm = ptr;

	if(req->response_code != HTTP_OK) {
		printf("CHUNK FAIL\n");
	} else {
		struct message_chunk *mc = calloc(1, sizeof(struct message_chunk));
		if(cm->last == NULL) {
			cm->first = cm->last = mc;
		} else {
			cm->last->next = mc;
			cm->last = mc;
		}
		mc->sz = EVBUFFER_LENGTH(req->input_buffer);
		printf("CHUNK SUCCESS: %d\n", (int)mc->sz);
	}
}

int
main(int argc, char *argv[]) {
	(void)argc;
	(void)argv;

	int ret, i;

	struct event_base *base = event_base_new();

	for(i = 0; i < 1; ++i) {

		struct comet_message *cm = calloc(1, sizeof(struct comet_message));

		struct evhttp_connection *evcon = evhttp_connection_new("127.0.0.1", 1234);
		struct evhttp_request *evreq = evhttp_request_new(on_http_response, cm);
		evhttp_connection_set_base(evcon, base);

		ret = evhttp_make_request(evcon, evreq, EVHTTP_REQ_GET, "/iframe");
		evhttp_request_set_chunked_cb(evreq, on_http_chunk);
	}

	event_base_dispatch(base);

	return EXIT_SUCCESS;
}

