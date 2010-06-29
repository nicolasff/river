#include <stdlib.h>
#include <stdio.h>

#include "server.h"
#include "socket.h"
#include "channel.h"

int
main() {

	int fd = socket_setup("127.0.0.1", 1234);
	channel_init();


	server_run(fd, 4, NULL);
	printf("exit\n");

	return EXIT_SUCCESS;
}
