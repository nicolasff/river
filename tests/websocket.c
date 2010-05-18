#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <errno.h>


#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>


#include <event.h>

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

/* worker_thread, with counter of remaining messages */
struct worker_thread {
	struct host_info *hi;
	int reader_per_chan;
	struct event_base *base;

	char **channels;
	int channel_count;

	int request_count;
	int byte_count;
	pthread_t thread;

	struct evbuffer *buffer;
	int got_header;

	struct event ev_w;
};

void
process_message(struct worker_thread *wt, size_t sz) {

	// printf("process_message\n");
	if(wt->request_count % 10000 == 0) {
		printf("%8d messages left (got %9d bytes so far).\n",
			wt->request_count, wt->byte_count);
	}
	wt->byte_count += sz;

	/* decrement read count, and stop receiving when we reach zero. */
	wt->request_count--;
	if(wt->request_count == 0) {
		event_base_loopexit(wt->base, NULL);
	}
}

void
websocket_write(int fd, short event, void *ptr) {
	int ret;
	struct worker_thread *wt = ptr;

	/* printf("websocket_write (event=%s)\n", (event == EV_WRITE ? "EV_WRITE": "???")); */
	if(event != EV_WRITE) {
		return;
	}

	char message[] = "\x00hello, world!\xff";
	ret = write(fd, message, sizeof(message)-1);
	if(ret != sizeof(message)-1) {
		fprintf(stderr, "write(2) on fd=%d failed: %s\n", fd, strerror(errno));
		close(fd);
	}

	event_set(&wt->ev_w, fd, EV_WRITE, websocket_write, wt);
	event_base_set(wt->base, &wt->ev_w);
	ret = event_add(&wt->ev_w, NULL);
}

static void
websocket_read(int fd, short event, void *ptr) {
	char packet[2048], *pos;
	int ret, success = 1;

	struct worker_thread *wt = ptr;

	/* printf("websocket_read (event=%s)\n", (event == EV_READ ? "EV_READ": "???")); */
	if(event != EV_READ) {
		return;
	}

	/* read message */
	ret = read(fd, packet, sizeof(packet));
	pos = packet;
	if(ret > 0) {
		char *data, *last;
		int sz, msg_sz;
		
		if(wt->got_header == 0) { /* first response */
			char *frame_start = strstr(packet, "\r\n\r\n");
			if(frame_start == NULL) {
				return;
			} else {

				wt->got_header = 1;
				event_set(&wt->ev_w, fd, EV_WRITE, websocket_write, wt);
				event_base_set(wt->base, &wt->ev_w);
				ret = event_add(&wt->ev_w, NULL);
			}
		} else {
			evbuffer_add(wt->buffer, packet, ret);
		}

		while(1) {
			data = (char*)EVBUFFER_DATA(wt->buffer);
			sz = EVBUFFER_LENGTH(wt->buffer);

			if(sz == 0) { /* no data */
				/* printf("no data.\n"); */
				break;
			}
			if(*data != 0) { /* missing frame start */
				printf("missing frame start. (ret=%d) data=[%s]\n", ret, data);
				success = 0;
				break;
			}
			last = memchr(data, 0xff, sz);
			if(!last) { /* no end of frame in sight, keep what we have for now. */
				break;
			}
			msg_sz = last - data - 1;
			process_message(ptr, msg_sz);

			/* drain including frame delimiters (+2 bytes) */
			evbuffer_drain(wt->buffer, msg_sz + 2); 
		}
	} else {
		printf("ret=%d\n", ret);
		success = 0;
	}
	if(success == 0) {
		shutdown(fd, SHUT_RDWR);
		close(fd);
		event_base_loopexit(wt->base, NULL);
	}
}

void*
worker_main(void *ptr) {

	char ws_handshake[] = "GET /websocket?name=x HTTP/1.1\r\n"
				"Host: 127.0.0.1:1234\r\n"
				"Connection: Upgrade\r\n"
				"Upgrade: WebSocket\r\n"
				"Origin: http://127.0.0.1:1234\r\n"
				"\r\n";

	struct worker_thread *wt = ptr;

	int ret;
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(1234);
	memset(&(addr.sin_addr), 0, sizeof(addr.sin_addr));
	addr.sin_addr.s_addr = 0;

	ret = connect(fd, (struct sockaddr*)&addr, sizeof(struct sockaddr));
	if(ret != 0) {
		fprintf(stderr, "connect(2): ret=%d: %s\n", ret, strerror(errno));
		return NULL;
	}

	ret = write(fd, ws_handshake, sizeof(ws_handshake)-1);

	struct event ev_r;
	event_set(&ev_r, fd, EV_READ | EV_PERSIST, websocket_read, wt);
	event_base_set(wt->base, &ev_r);
	event_add(&ev_r, NULL);

	event_base_dispatch(wt->base);
	return NULL;
}

	struct worker_thread wt;

int
main(int argc, char *argv[]) {

	(void)argc;
	(void)argv;
	//int connections_per_thread = 10;
	int request_count = 100*1000;

	struct timespec t0, t1;
	clock_gettime(CLOCK_MONOTONIC, &t0);

	wt.base = event_base_new();
	wt.request_count = request_count;
	wt.buffer = evbuffer_new();
	wt.byte_count = 0;
	wt.got_header = 0;

	worker_main(&wt);
//	pthread_create(&wt.thread, NULL, worker_main, &wt);
//
//	pthread_join(wt.thread, NULL);

	/* timing */
	clock_gettime(CLOCK_MONOTONIC, &t1);
	float mili0 = t0.tv_sec * 1000 + t0.tv_nsec / 1000000;
	float mili1 = t1.tv_sec * 1000 + t1.tv_nsec / 1000000;
	printf("Read %d messages in %0.2f sec: %0.2f/sec\n", request_count, (mili1-mili0)/1000.0, 1000*(float)request_count/(mili1-mili0));

	return EXIT_SUCCESS;
}

