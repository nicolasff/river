#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include "files.h"
#include "socket.h"
#include "mem.h"

static char* iframe_get_buffer(size_t *len);

/**
 * Return generic page for iframe inclusion
 */
static void
file_send_iframe(struct connection *cx) {

	static char *iframe_buffer = NULL;
	static size_t iframe_buffer_len;

	/* read iframe file, the first time. */
	if(iframe_buffer == NULL) {
		iframe_buffer = iframe_get_buffer(&iframe_buffer_len);
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
}


/**
 * Send Javascript library
 */
static void
file_send_libjs(struct connection *cx) {

	char buffer_start[] = "var comet_domain = '";
	char buffer_domain[] = "'; var common_domain = '";
	char buffer_js[] = "';\n\
Comet = {\n\
	init: function(cbLoaded) {\n\
		var iframe = document.createElement('iframe');\n\
		iframe.setAttribute('style', 'display: none');\n\
\n\
		var f = function() {\n\
			Comet.Client = function() { return new iframe.contentWindow.CometClient(comet_domain);};\n\
			cbLoaded();\n\
		};\n\
		if(window.addEventListener) {\n\
			window.addEventListener('load', f, false);\n\
		} else if(window.attachEvent) {\n\
			window.attachEvent('onload', f);\n\
		}\n\
		iframe.src = 'http://' + comet_domain + '/iframe?domain=' + common_domain;\n\
		document.body.appendChild(iframe);\n\
	}\n\
};";

	http_streaming_start_ct(cx, 200, "OK", "text/javascript");
	http_streaming_chunk(cx, buffer_start, sizeof(buffer_start)-1);

	/* then current host */
	if(cx->headers.host) {
		http_streaming_chunk(cx, cx->headers.host, cx->headers.host_len);
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

static char*
iframe_get_buffer(size_t *len) {

	char *out = "\n\
if(typeof XMLHttpRequest == \"undefined\") {\n\
	XMLHttpRequest = function () {\n\
		try { return new ActiveXObject(\"Msxml2.XMLHTTP.6.0\"); }\n\
		catch (e1) {}\n\
		try { return new ActiveXObject(\"Msxml2.XMLHTTP.3.0\"); }\n\
		catch (e2) {}\n\
		try { return new ActiveXObject(\"Msxml2.XMLHTTP\"); }\n\
		catch (e3) {}\n\
		try { return new ActiveXObject(\"Microsoft.XMLHTTP\"); }\n\
		catch (e4) {}\n\
		throw new Error(\"This browser does not support XMLHttpRequest.\");\n\
	};\n\
}\n\
\n\
/**\n\
 * Cuts the first part of a message and returns it. What's after is discarded.\n\
 * if msg is '[abc][def]', this function will return '[abc]' only.\n\
 * if msg is '[\"abc]ef\"]gh', this function will return '[\"abc]ef\"]': characters between quotes are preserved.\n\
 */\n\
function cutMessage(msg) {\n\
\n\
	if(msg.length < 2 || msg.charAt(0) != '[') {\n\
		return \"\";\n\
	}\n\
	var pos;\n\
	var depth = 0;\n\
	var in_string = false;\n\
	var len = msg.length;\n\
\n\
	for(pos = 0; pos < len; pos++) {\n\
		if(msg.charAt(pos) == '\"') {\n\
			in_string = !in_string;\n\
			continue;\n\
		}\n\
		if(in_string) { // do not consider characters that are part of a string.\n\
			continue;\n\
		}\n\
		// opening and closing brackets, check for depth.\n\
		if(msg.charAt(pos) == '[') {\n\
			depth++;\n\
		} else if(msg.charAt(pos) == ']') {\n\
			depth--;\n\
		}\n\
\n\
		if(depth == 0) {\n\
			return msg.substr(0, pos+1);\n\
		}\n\
	}\n\
\n\
	return \"\";\n\
\n\
}\n\
\n\
function CometClient(host){\n\
\n\
	this.host = host;\n\
	this.pos = 0;\n\
	this.seq = 0;\n\
\n\
	this.hasWebSocket = (typeof(WebSocket) != \"undefined\");\n\
\n\
	// detect if client can stream data using \"xhr.readyState=3\" or not\n\
	this.canStream = 1;\n\
	this.isIE = false;\n\
	var noStream = new Array(\"Chrome\", \"WebKit\", \"KHTML\", \"MSIE\");\n\
	for(i = 0; i < noStream.length; i++) {\n\
		if(navigator.appVersion.indexOf(noStream[i]) != -1) {\n\
			this.canStream = 0;\n\
			break;\n\
		}\n\
	}\n\
	if(navigator.appVersion.indexOf(\"MSIE\") != -1) {\n\
		this.isIE = true;\n\
	}\n\
\n\
	this.disconnect = function() {\n\
		if(this.hasWebSocket) {\n\
			return this.wsDisconnect();\n\
		}\n\
		window.clearTimeout(this.reconnectionTimeout); // cancel reconnect.\n\
\n\
		try {\n\
			this.xhr.onreadystatechange = function(){};\n\
			this.xhr.abort();\n\
		} catch(e) {}\n\
	}\n\
\n\
	this.send = function(channel, msg) {\n\
\n\
		var url = \"http://\" + this.host + \"/publish?name=\"+channel+\"&data=\"+msg;\n\
		var xhr = new XMLHttpRequest;\n\
		xhr.open(\"get\",url, true);\n\
		xhr.send(null);\n\
	}\n\
\n\
	this.connect = function(channel, onMsg, seq) {\n\
\n\
		if(typeof(seq) != \"undefined\") {\n\
			this.seq = seq;\n\
		}\n\
\n\
		if(this.hasWebSocket) {\n\
			return this.wsConnect(channel, onMsg);\n\
		} else {\n\
			return this.streamingConnect(channel, onMsg);\n\
		}\n\
	}\n\
\n\
	this.wsConnect = function(channel, onMsg) {\n\
		this.socket = new WebSocket('ws://'+this.host+'/websocket?name='+channel);\n\
		this.socket.onopen = function () {\n\
		};\n\
		this.socket.onmessage = function (messageEvent) {\n\
			try {\n\
				var obj = JSON.parse(messageEvent.data);\n\
				if(obj[0] == \"msg\") {\n\
					onMsg(obj[1]);\n\
				}\n\
			} catch(e) {\n\
			}\n\
		};\n\
		this.socket.onclose = function () {\n\
			var comet = this;\n\
			try {\n\
				window.setTimeout(function() {\n\
					comet.wsConnect(channel, onMsg);\n\
				}, 1000);\n\
			} catch(e) {}\n\
		};\n\
	}\n\
\n\
	this.wsDisconnect = function() {\n\
		this.socket.onclose = function() {}; // don't reconnect.\n\
		this.socket.close();\n\
	}\n\
\n\
	this.streamingConnect = function(channel, onMsg) {\n\
\n\
		var comet = this;\n\
\n\
		if(comet.reconnectionTimer) {\n\
			window.clearTimeout(comet.reconnectionTimer);\n\
		}\n\
\n\
		// prepare XHR\n\
		var url = \"http://\"+this.host+\"/subscribe\";\n\
		var params = \"name=\"+channel+\"&keep=\"+this.canStream;\n\
		if(this.seq >= 0) {\n\
			params += \"&seq=\"+this.seq;\n\
		}\n\
\n\
		comet.xhr = new XMLHttpRequest;\n\
		comet.pos = 0; /* counts how much we've parsed so far */\n\
		comet.xhr.open(\"POST\", url, true);\n\
\n\
		comet.xhr.onreadystatechange = function() { /* callback for the reception of messages */\n\
\n\
			if(comet.xhr.readyState != 4 && comet.canStream == 0) { /* not a full message yet. probably going through states 1 & 2 */\n\
				return; // wait for state 4.\n\
			}\n\
\n\
			if(comet.xhr.readyState == 4 && comet.xhr.status != 200) { // error, reconnect\n\
				try {\n\
				comet.reconnectTimer = window.setTimeout(function() {\n\
					comet.connect(channel, onMsg);\n\
				}, 1000);\n\
				} catch(e) {}\n\
				return;\n\
			}\n\
\n\
			if(comet.xhr.readyState === 3 || comet.xhr.readyState == 4) { // got data!\n\
\n\
				var data = comet.xhr.responseText;\n\
\n\
				if(data.length == 0) { // empty response, reconnect.\n\
					try {\n\
						comet.reconnectTimer = window.setTimeout(function() {\n\
							comet.connect(channel, onMsg);\n\
						}, 1000); // reconnect soon.\n\
					} catch(e) {}\n\
					return;\n\
				}\n\
\n\
				// try parsing message\n\
				do {\n\
					// this might only be the first part of our current packet.\n\
					var msg = cutMessage(data.substr(comet.pos));\n\
					if(msg.length) try {\n\
						var obj;\n\
						if(comet.isIE) {\n\
							obj = eval(\"(\"+msg+\")\");\n\
						} else {\n\
							obj = JSON.parse(msg);\n\
						}\n\
						comet.pos += msg.length; // add to what we've read so far.\n\
\n\
						if(obj[1] && obj[1].seq) { // new timestamp\n\
							comet.seq = obj[1].seq;\n\
							if(comet.canStream == 0) {\n\
								// console.log(\"new seq:\", comet.seq);\n\
							}\n\
						}\n\
\n\
						switch(obj[0]) {\n\
							case 'msg': // regular message\n\
								if(obj[1].channel == channel) {\n\
									if(onMsg) {\n\
										try {\n\
											onMsg(obj[1]);\n\
										} catch(e) {} // ignore client errors\n\
									}\n\
								}\n\
								break;\n\
						}\n\
					} catch(e) {\n\
						comet.xhr.abort(); // avoid duplicates upon reconnection\n\
						try{\n\
							comet.reconnectTimer = window.setTimeout(function() {\n\
								comet.connect(channel, onMsg);\n\
							}, 1000); // reconnect soon.\n\
						} catch(e) {}\n\
						return;\n\
					}\n\
\n\
					if(comet.pos < data.length) {\n\
						break;\n\
					}\n\
				} while(comet.pos < data.length);\n\
					// there might be more in this event: consume the whole string.\n\
			}\n\
			if(comet.xhr.readyState == 4) { // reconnect\n\
				// if no streaming capability, reconnect directly.\n\
				if(this.canStream) {\n\
					try {\n\
						comet.reconnectTimer = window.setTimeout(function() {\n\
							comet.connect(channel, onMsg);\n\
						}, 1000);\n\
					} catch(e) {}\n\
					return;\n\
				} else {\n\
					comet.connect(channel, onMsg);\n\
				}\n\
			}\n\
\n\
		}\n\
		comet.xhr.send(params);\n\
	}\n\
};\n\
		";
	*len = strlen(out);
	return out;

}
