#include <stdlib.h>
#include <stdio.h>

#include "server.h"
#include "user.h"
#include "channel.h"
#define NB_WORKERS	1

int
main(int argc, char *argv[]) {

	(void)argc;
	(void)argv;

#if 0
	memset(preload, '\n', sizeof(preload)-1);
	preload[sizeof(preload)-1] = 0;
#endif

	channel_init();
	user_init();
	server_start(NB_WORKERS, 1234, 7777);



	while(1);

	return EXIT_SUCCESS;
}
