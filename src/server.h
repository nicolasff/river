#ifndef SERVER_H
#define SERVER_H

struct channel;

void
server_run(int fd, int threads, struct channel *chan);

void
cb_available_client_data(int fd, short event, void *ptr);

#endif
