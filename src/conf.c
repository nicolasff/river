#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "conf.h"


struct conf *
conf_read(const char *filename) {

	struct conf *conf;

	FILE *f = fopen(filename, "r");

	if(!f) {
		return NULL;
	}

	conf = calloc(1, sizeof(struct conf));
	conf->client_timeout = 30;
	conf->threads = 8;

	while(!feof(f)) {
		char buffer[100], *ret;
		memset(buffer, 0, sizeof(buffer));
		if(!(ret = fgets(buffer, sizeof(buffer)-1, f))) {
			break;
		}
		if(*ret == '#') { /* comments */
			continue;
		}

		if(*ret != 0) {
			ret[strlen(ret)-1] = 0; /* remove new line */
		}

		if(strncmp(ret, "ip ", 3) == 0) {
			conf->ip = strdup(ret + 3);
		} else if(strncmp(ret, "port ", 5) == 0) {
			conf->port = (short)atoi(ret + 5);
		} else if(strncmp(ret, "log ", 4) == 0) {
			conf->log_file = strdup(ret + 4);
		} else if(strncmp(ret, "threads ", 8) == 0) {
			conf->threads = (int)atoi(ret + 8);
		} else if(strncmp(ret, "client_timeout", 14) == 0) {
			conf->client_timeout = (int)atoi(ret + 14);
		}
	}
	fclose(f);

	/* default values */
	if(!conf->ip) {
		conf->ip = strdup("127.0.0.1");
	}
	if(!conf->log_file) {
		conf->log_file = strdup("cometd.conf");
	}

	return conf;
}

void
conf_free(struct conf *conf) {

	free(conf->ip);
	free(conf->log_file);

	free(conf);
}
