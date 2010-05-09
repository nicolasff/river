#include <string.h>

#include "files.h"

char *iframe_buffer = NULL;
size_t iframe_buffer_len = -1;

/**
 * Return generic page for iframe inclusion
 */
static void
file_send_iframe(struct http_request *req) {

	dictEntry *de;

	char buffer_start[] = "<html><body><script type=\"text/javascript\">\ndocument.domain=\"";
	char buffer_domain[] = "\";\n";
	char buffer_end[] = "</script></body></html>\n";

	http_streaming_start(req->fd, 200, "OK");
	http_streaming_chunk(req->fd, buffer_start, sizeof(buffer_start)-1);
	if(req->get && (de = dictFind(req->get, "domain"))) {
		http_streaming_chunk(req->fd, de->val, de->size);
	}
	http_streaming_chunk(req->fd, buffer_domain, sizeof(buffer_domain)-1);

	/* iframe.js */
	http_streaming_chunk(req->fd, iframe_buffer, iframe_buffer_len);

	http_streaming_chunk(req->fd, buffer_end, sizeof(buffer_end)-1);
	http_streaming_end(req->fd);
}


/**
 * Send Javascript library
 */
void
file_send_libjs(struct http_request *req) {

	dictEntry *de;

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

	http_streaming_start_ct(req->fd, 200, "OK", "text/javascript");
	http_streaming_chunk(req->fd, buffer_start, sizeof(buffer_start)-1);
	/* then current host */
	if(req->host) {
		http_streaming_chunk(req->fd, req->host, req->host_len);
	}
	http_streaming_chunk(req->fd, buffer_domain, sizeof(buffer_domain)-1);
	/* then common domain */
	if(req->get && (de = dictFind(req->get, "domain"))) {
		http_streaming_chunk(req->fd, de->val, de->size);
	}
	http_streaming_chunk(req->fd, buffer_js, sizeof(buffer_js)-1);
	http_streaming_end(req->fd);
}

/**
 * Send crossdomain.xml file to Adobe Flash client
 */
void
file_send_flash_crossdomain(struct http_request *req) {

	const char crossdomain[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
		"<cross-domain-policy>"
		"<allow-access-from domain=\"*\" />"
		"</cross-domain-policy>\r\n";

	http_response_ct(req->fd, 200, "OK", crossdomain, sizeof(crossdomain)-1,
			"application/xml");
}

int
file_send(struct http_request *req) {


	if(req->path_len == 7 && strncmp("/iframe", req->path, req->path_len) == 0) {
		file_send_iframe(req);
	} else if(req->path_len == 7 && strncmp("/lib.js", req->path, req->path_len) == 0) {
		file_send_libjs(req);
	} else if(req->path_len == 16 && strncmp("/crossdomain.xml", req->path, req->path_len) == 0) {
		file_send_flash_crossdomain(req);
	} else {
		return -1;
	}

	return 0;
}

