#include <stdlib.h>
#include "fd.h"

struct p_fd *
fd_new(int fd, struct evhttp_request *ev) {

	struct p_fd *ret = calloc(1, sizeof(struct p_fd));

	if(NULL == ret) {
		return NULL;
	}
	
	ret->fd = fd;
	ret->ev = ev;
	return ret;
}

void
fd_free(struct p_fd *p) {

	free(p);
}

void
fd_free_r(struct p_fd *p) {

	struct p_fd *old;

	while(p) {
		old = p;
		p = p->next;
		fd_free(old);
	}
}

