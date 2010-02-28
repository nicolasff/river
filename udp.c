#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>

#include "udp.h"
#include "socket.h"

/**
 * Creates a UDP server for back-end com
 */
int
udp_create_socket(const char *host, const short port, int *fd) {

	int ret;
	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	memset(&(addr.sin_addr), 0, sizeof(addr.sin_addr));
	addr.sin_addr.s_addr = inet_addr(host);

	*fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	ret = socket_setup(*fd);
	if(0 != ret) {
		return -1;
	}

	return 0;
}

