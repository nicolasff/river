#ifndef COMETD_FD_H
#define COMETD_FD_H

struct evhttp_request;

struct p_fd {
	int fd;
	struct evhttp_request *ev;

	struct p_fd *next;
};

struct p_fd *
fd_new(int fd, struct evhttp_request *ev);

void
fd_free(struct p_fd *);

void
fd_free_r(struct p_fd *);

#endif /* COMETD_FD_H */

