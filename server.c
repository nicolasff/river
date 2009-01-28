#include <stdlib.h>
#include <stdio.h>
#include <sys/queue.h>
#include <event.h>
#include <evhttp.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <fcntl.h>

#include "socket.h"
#include "udp.h"
#include "http_dispatch.h"
#include "server.h"
#include "user.h"
#include "connection.h"
#include "message.h"

#if 0
char preload[2000];
#endif

void
handle_client(struct evhttp_request *ev) {

#if 0
	int webkit = 0;
#endif
	struct evbuffer *buffer;
	buffer = evbuffer_new();

	evhttp_send_reply_start(ev, HTTP_OK, "OK");
#if 0
	if(webkit) {
		evbuffer_add(buffer, preload, sizeof(preload)-1);
	}
#endif
	evbuffer_add_printf(buffer, "lol('hello');\n");
	evhttp_send_reply_chunk(ev, buffer);
	evhttp_send_reply_end(ev);

	evbuffer_free(buffer);
}

void
callback(struct evhttp_request *ev, void *arg) {

	struct thread_info *self = arg;
//	printf("thread %d handling a client\n", self->id);
	self->clients++;
	handle_client(ev);
	return ;
}

void
client_data_available(int fd, short event, void *arg) {

	struct p_user *user;
	struct p_connection *u_connection;
	struct p_message *m;

	struct thread_info *self = arg;
	if(fd != self->pipe[0] || event != EV_READ) {
		return;
	}

	/* read data from the pipe */
	read(fd, &u_connection, sizeof(u_connection));
	user = u_connection->user;
	
	
	/*
	printf("thread %d! received [%p] written in %d\n", 
		self->id, u_connection, self->pipe[1]);
	*/
	
	
	/* find corresponding user */
	if(!user) {
		return;
	}
	
	/* locking user here. */
	/*
	printf("locking user %ld\n", user->uid);
	*/
	pthread_mutex_lock(&(user->lock));

		for(m = u_connection->inbox_first; m;) {
			struct p_message *m_next = m->next;
			/* send data */
			/*
			printf("send data to %p\n", u_connection->ev);
			*/
			evhttp_send_reply_chunk(u_connection->ev, m->data);

			message_free(m);

			m = m_next;
		}
		evhttp_send_reply_end(u_connection->ev);

		/* remove connection from user. */
		if(u_connection == user->connections) {	/* del first */
			user->connections = user->connections->next;
		} else {
			struct p_connection *cx_prev;
			for(cx_prev = user->connections; cx_prev; cx_prev = cx_prev->next) {
				if(cx_prev && cx_prev->next == u_connection) {
					cx_prev->next = cx_prev->next->next;
					break;
				}
			}
		}
		connection_free(u_connection);
		/*	
		u_connection->inbox_first = u_connection->inbox_last = NULL;
		*/

	pthread_mutex_unlock(&(user->lock));
	/*
	printf("unlocked user %ld\n", user->uid);
	*/
}

void*
thread_start(void *arg) {
	struct thread_info *self = arg;
	int err;
	struct event cx_ev;

	/* initialize libevent in the thread */
	self->ev_base = event_init ();
	self->ev_http = evhttp_new(self->ev_base);

	/* create communication pipe */
	pipe(self->pipe);
	fcntl(self->pipe[1], F_SETFL, O_NONBLOCK);
	event_set(&cx_ev, self->pipe[0], EV_READ | EV_PERSIST,
			client_data_available, self);
	event_base_set(self->ev_base, &cx_ev);
	err = event_add(&cx_ev, NULL);
	/*
	printf("thread %d listening to pipe %d: err=%d\n", self->id, self->pipe[0], err);
	*/

	err = evhttp_accept_socket(self->ev_http, self->fd);

	/* set meta callbacks */
	evhttp_set_cb(self->ev_http, "/meta/authenticate", 
			http_dispatch_meta_authenticate, self);
	
	evhttp_set_cb(self->ev_http, "/meta/connect", 
			http_dispatch_meta_connect, self);

	evhttp_set_cb(self->ev_http, "/meta/publish", 
			http_dispatch_meta_publish, self);

	evhttp_set_cb(self->ev_http, "/meta/subscribe",
			http_dispatch_meta_subscribe, self);

	evhttp_set_cb(self->ev_http, "/meta/unsubscribe",
			http_dispatch_meta_unsubscribe, self);

	evhttp_set_cb(self->ev_http, "/meta/newchannel",
			http_dispatch_meta_newchannel, self);

	/*evhttp_set_gencb(self->ev_http, callback, self);*/

	event_base_dispatch(self->ev_base);

	return NULL;
}


int
server_start(short nb_workers, short port, short udp_port) {

	struct sockaddr_in addr;
	int fd, i, ret;
	int udp;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	memset(&(addr.sin_addr), 0, sizeof(addr.sin_addr));
	addr.sin_addr.s_addr = htonl(0);

	fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	/* now bind & test */
	if (-1 == fd) {
		syslog(LOG_ERR, "Socket error: %s\n", strerror(errno));
		return -1;
	}
	ret = socket_setup(fd);
	if(0 != ret) {
		return -1;
	}
	ret = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
	if (0 != ret) {
		syslog(LOG_ERR, "Bind error: %s\n", strerror(errno));
		return -1;
	}
	ret = listen(fd, SOMAXCONN);
	if (0 != ret) {
		syslog(LOG_DEBUG, "Listen error: %s\n", strerror(errno));
		return -1;
	}

	event_init();

	worker_threads = calloc(nb_workers, sizeof(struct thread_info));

	for(i = 0; i < nb_workers; ++i) {
		worker_threads[i].id = i;
		worker_threads[i].fd = fd;
		worker_threads[i].clients = 0;
		pthread_create(&worker_threads[i].thread, NULL, 
				thread_start, &worker_threads[i]);
	}

#if 0
	if(udp_create_socket("0.0.0.0", udp_port, &udp) != 0) {
		printf("UDP FAIL\n");
	} else {
		printf("UDP WIN\n");
		close(udp);
	}
#else 
	(void)udp_port;
	(void)udp;
#endif
	return 0;
}
