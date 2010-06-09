#include "socket.h"
#include "server.h"
#include "channel.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <event.h>
#include <unistd.h>
#include <pthread.h>

extern struct dispatcher_info di;

static int cx_total = 0;
pthread_mutex_t cx_lock;

/**
 * Sets up a non-blocking socket
 */
int
socket_setup(const char *ip, short port) {

	int reuse = 1;
	struct sockaddr_in addr;
	int fd, ret;

	pthread_mutex_init(&cx_lock, NULL);

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	memset(&(addr.sin_addr), 0, sizeof(addr.sin_addr));
	addr.sin_addr.s_addr = inet_addr(ip);

	/* this sad list of tests could use a Maybe monad... */

	/* create socket */
	fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (-1 == fd) {
		syslog(LOG_ERR, "Socket error: %m\n");
		return -1;
	}

	/* reuse address if we've bound to it before. */
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse,
				sizeof(reuse)) < 0) {
		syslog(LOG_ERR, "setsockopt error: %m\n");
		return -1;
	}

	/* set socket as non-blocking. */
	ret = fcntl(fd, F_SETFD, O_NONBLOCK);
	if (0 != ret) {
		syslog(LOG_ERR, "fcntl error: %m\n");
		return -1;
	}

	/* bind */
	ret = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
	if (0 != ret) {
		syslog(LOG_ERR, "Bind error: %m\n");
		return -1;
	}

	/* listen */
	ret = listen(fd, SOMAXCONN);
	if (0 != ret) {
		syslog(LOG_DEBUG, "Listen error: %m\n");
		return -1;
	}

	/* there you go, ready to accept! */
	return fd;
}

void
cx_monitor(struct connection *cx) {

	cx->ev = malloc(sizeof(struct event));
	event_set(cx->ev, cx->fd, EV_READ, cx_is_broken, cx);
	event_base_set(di.base, cx->ev);
	event_add(cx->ev, NULL);
}

void
cx_is_broken(int fd, short event, void *ptr) {

	(void)event;
	(void)fd;

	struct connection *cx = ptr;

	struct channel *chan = cx->channel;
	if(chan) {
		CHANNEL_LOCK(chan);
	}
	cx_remove(cx);
	if(chan) {
		CHANNEL_UNLOCK(chan);
	}
}

void
cx_remove(struct connection *cx) {

	close(cx->fd);
	if(cx->ev) {
		event_del(cx->ev);
		free(cx->ev);
	}
	if(cx->cu) {
		channel_del_connection(cx->channel, cx->cu);
	}
	free(cx);
	update_event(EV_READ | EV_PERSIST);
}

