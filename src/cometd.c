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

	channel_init(cfg->channel_key);
	user_init();
	server_run(cfg->threads, cfg->ip, cfg->port);

	while(1) {
		sleep(60);
	}

	return EXIT_SUCCESS;
}
