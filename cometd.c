#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "server.h"
#include "user.h"
#include "channel.h"
#include "conf.h"
#define NB_WORKERS	8

int
main(int argc, char *argv[]) {

	(void)argc;
	(void)argv;

	struct conf *cfg;
	
	if(argc == 2) {
		cfg = conf_read(argv[1]);
	} else {
		cfg = conf_read("cometd.conf");
	}
	if(!cfg) {
		fprintf(stderr, "Could not read config file.\n");
		return EXIT_FAILURE;
	}

	channel_init();
	user_init();
	server_run(NB_WORKERS, 1234);

	while(1) {
		sleep(60);
	}

	return EXIT_SUCCESS;
}
