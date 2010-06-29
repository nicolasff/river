#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include "files.h"
#include "socket.h"

/**
 * Return generic page for iframe inclusion
 */
static void
file_send_iframe(struct connection *cx) {

	static char *iframe_buffer = NULL;
	static size_t iframe_buffer_len = -1;

	struct stat st;
	int ret, fp;
	size_t remain;
	const char filename[] = "iframe.js";

	/* read iframe file, the first time. */
	if(iframe_buffer == NULL) {
		ret = stat(filename, &st);
		if(ret != 0) {
			return;
		}
		iframe_buffer_len = st.st_size;
		if(!(iframe_buffer = calloc(iframe_buffer_len, 1))) {
			iframe_buffer_len = -1;
			return;
		}
		remain = iframe_buffer_len;
		fp = open(filename, O_RDONLY);
		while(remain) {
			int count = read(fp, iframe_buffer + iframe_buffer_len - remain, remain);
			if(count <= 0) {
				free(iframe_buffer);
				iframe_buffer_len = -1;
				return;
			}
			remain -= count;
		}
	}

	char buffer_start[] = "<html><body><script type=\"text/javascript\">\ndocument.domain=\"";
	char buffer_domain[] = "\";\n";
	char buffer_end[] = "</script></body></html>\n";

	http_streaming_start(cx, 200, "OK");
	http_streaming_chunk(cx, buffer_start, sizeof(buffer_start)-1);
	if(cx->get.domain) {
		http_streaming_chunk(cx, cx->get.domain, cx->get.domain_len);
	}
	http_streaming_chunk(cx, buffer_domain, sizeof(buffer_domain)-1);

	/* iframe.js */
	http_streaming_chunk(cx, iframe_buffer, iframe_buffer_len);

	http_streaming_chunk(cx, buffer_end, sizeof(buffer_end)-1);
	http_streaming_end(cx);

	free(iframe_buffer);
	iframe_buffer = NULL;
}


/**
 * Send Javascript library
 */
static void
file_send_libjs(struct connection *cx) {

	char buffer_start[] = "var comet_domain = '";
	char buffer_domain[] = "'; var common_domain = '";
	char buffer_js[] = "';\
Comet = {\
	init: function(cbLoaded) {\
		var iframe = document.createElement('iframe');\
		iframe.src = 'http://' + comet_domain + '/iframe?domain=' + common_domain;\
		iframe.setAttribute('style', 'display: none');\
		document.body.appendChild(iframe);\
\
		iframe.onload = function() {\
			Comet.Client = function() { return new iframe.contentWindow.CometClient(comet_domain);};\
			cbLoaded();\
		};\
	}\
};";

	http_streaming_start_ct(cx, 200, "OK", "text/javascript");
	http_streaming_chunk(cx, buffer_start, sizeof(buffer_start)-1);

	/* then current host */
	if(cx->host) {
		http_streaming_chunk(cx, cx->host, cx->host_len);
	}

	/* then common domain */
	http_streaming_chunk(cx, buffer_domain, sizeof(buffer_domain)-1);
	if(cx->get.domain) {
		http_streaming_chunk(cx, cx->get.domain, cx->get.domain_len);
	}

	/* finally, the code itself. */
	http_streaming_chunk(cx, buffer_js, sizeof(buffer_js)-1);
	http_streaming_end(cx);
}

const char flash_xd[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
	"<cross-domain-policy>"
	"<allow-access-from domain=\"*\" />"
	"</cross-domain-policy>\r\n";
const int flash_xd_len = sizeof(flash_xd) - 1;

/**
 * Send crossdomain.xml file to Adobe Flash client
 */
int
file_send_flash_crossdomain(struct connection *cx) {

	return http_response_ct(cx, 200, "OK", flash_xd, sizeof(flash_xd)-1,
			"application/xml");
}

int
file_send(struct connection *cx) {

	if(cx->path_len == 7 && strncmp("/iframe", cx->path, cx->path_len) == 0) {
		file_send_iframe(cx);
	} else if(cx->path_len == 7 && strncmp("/lib.js", cx->path, cx->path_len) == 0) {
		file_send_libjs(cx);
	} else if(cx->path_len == 16 && strncmp("/crossdomain.xml", cx->path, cx->path_len) == 0) {
		file_send_flash_crossdomain(cx);
	} else {
		return -1;
	}

	return 0;
}

