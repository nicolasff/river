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
		} else if(strncmp(ret, "threads ", 8) == 0) {
			conf->threads = (short)atoi(ret + 8);
		} else if(strncmp(ret, "domain ", 7) == 0) {
			conf->domain = strdup(ret + 7);
			conf->domain_len = strlen(conf->domain);
		} else if(strncmp(ret, "commondomain ", 13) == 0) {
			conf->common_domain = strdup(ret + 13);
			conf->common_domain_len = strlen(conf->common_domain);
		} else if(strncmp(ret, "channelkey ", 11) == 0) {
			conf->channel_key = strdup(ret + 11);
		}
	}
	fclose(f);

	/* default values */
	if(!conf->ip) {
		conf->ip = strdup("127.0.0.1");
	}
	if(!conf->threads) {
		conf->threads = 8;
	}
	if(!conf->domain) {
		conf->domain = strdup("127.0.0.1");
	}
	if(!conf->common_domain) {
		conf->common_domain = strdup("127.0.0.1");
	}

	return conf;
}

void
conf_free(struct conf *conf) {

	free(conf->ip);
	free(conf->domain);
	free(conf->common_domain);
	free(conf->channel_key);

	free(conf);
}
