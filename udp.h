#ifndef COMETD_UDP_H
#define COMETD_UDP_H

struct udp_server {

	int id;
	int fd;
	
};

int
udp_create_socket(const char *host, const short port, int *fd);

int
udp_start_servers(int count, const char *host, const short port);

#endif /* COMETD_UDP_H */

