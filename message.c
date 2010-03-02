#include <stdlib.h>
#include <event.h>
#include <pthread.h>

#include "message.h"

struct p_message *
message_new(const char *data, size_t length) {

	struct p_message *ret = calloc(1, sizeof(struct p_message));
	if(NULL == ret) {
		return NULL;
	}

	ret->data = evbuffer_new();
	evbuffer_add(ret->data, data, length);
	evbuffer_add(ret->data, "\r\n", 2);

	return ret;
}

void
message_free(struct p_message *m) {

	evbuffer_free(m->data);
	free(m);
}


