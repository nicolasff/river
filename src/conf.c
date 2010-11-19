#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "conf.h"
#include "mem.h"


struct conf *
conf_read(const char *filename) {

	struct conf *conf;

	FILE *f = fopen(filename, "r");

	if(!f) {
		return NULL;
	}

	conf = rcalloc(1, sizeof(struct conf));
	conf->client_timeout = 30;

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
			conf->ip = rstrdup(ret + 3);
		} else if(strncmp(ret, "port ", 5) == 0) {
			conf->port = (short)atoi(ret + 5);
		} else if(strncmp(ret, "log ", 4) == 0) {
			conf->log_file = rstrdup(ret + 4);
		} else if(strncmp(ret, "client_timeout", 14) == 0) {
			conf->client_timeout = (int)atoi(ret + 14);
		} else if(strncmp(ret, "max_connections", 15) == 0) {
			conf->max_connections = (int)atoi(ret + 15);
		}
	}
	fclose(f);

	/* default values */
	if(!conf->ip) {
		conf->ip = rstrdup("127.0.0.1");
	}
	if(!conf->log_file) {
		conf->log_file = rstrdup("river.conf");
	}

	return conf;
}

void
conf_free(struct conf *conf) {

	rfree(conf->ip);
	rfree(conf->log_file);

	rfree(conf);
}
