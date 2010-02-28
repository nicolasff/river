#include "socket.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>

/**
 * Sets up a non-blocking socket
 */
int
socket_setup(int fd) {
	int ret, reuse = 1;

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse,
				sizeof(reuse)) < 0) {
		syslog(LOG_ERR, "setsockopt error: %s\n", strerror(errno));
		return -1;
	}
	ret = fcntl(fd, F_SETFD, O_NONBLOCK);
	if (0 != ret) {
		syslog(LOG_ERR, "fcntl error: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

