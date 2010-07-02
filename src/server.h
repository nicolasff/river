#ifndef SERVER_H
#define SERVER_H

struct channel;

void
server_run(int fd);

void
cb_available_client_data(int fd, short event, void *ptr);

void
on_possible_accept(int fd, short event, void *ptr);

void
on_available_data(int fd, short event, void *ptr);

#endif
