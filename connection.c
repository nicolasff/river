#include <stdlib.h>
#include "connection.h"

struct p_connection *
connection_new(int fd, struct evhttp_request *ev) {

	struct p_connection *ret = calloc(1, sizeof(struct p_connection));

	if(NULL == ret) {
		return NULL;
	}
	
	ret->fd = fd;
	ret->ev = ev;
	return ret;
}

void
connection_free(struct p_connection *p) {

	free(p);
}

void
connection_free_r(struct p_connection *p) {

	struct p_connection *old;

	while(p) {
		old = p;
		p = p->next;
		connection_free(old);
	}
}

