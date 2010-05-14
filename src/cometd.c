#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <syslog.h>

#include "server.h"
#include "channel.h"
#include "http_dispatch.h"
#include "conf.h"
#define NB_WORKERS	8

int
main(int argc, char *argv[]) {

	int ret;

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

	/* initialize syslog */
	openlog(cfg->log_file, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_USER);

	http_init(cfg);
	channel_init();
	ret = server_run(cfg->threads, cfg->ip, cfg->port);
	if(ret != 0) {
		fprintf(stderr, "Server start failed. Check syslog for more info.\n");
		return EXIT_FAILURE;
	}

	while(1) {
		sleep(60);
	}

	return EXIT_SUCCESS;
}
