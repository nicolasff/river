if(typeof XMLHttpRequest == "undefined") {
	XMLHttpRequest = function () {
		try { return new ActiveXObject("Msxml2.XMLHTTP.6.0"); }
		catch (e1) {}
		try { return new ActiveXObject("Msxml2.XMLHTTP.3.0"); }
		catch (e2) {}
		try { return new ActiveXObject("Msxml2.XMLHTTP"); }
		catch (e3) {}
		try { return new ActiveXObject("Microsoft.XMLHTTP"); }
		catch (e4) {}
		throw new Error("This browser does not support XMLHttpRequest.");
	};
}

/**
 * Cuts the first part of a message and returns it. What's after is discarded.
 * if msg is '[abc][def]', this function will return '[abc]' only.
 * if msg is '["abc]ef"]gh', this function will return '["abc]ef"]': characters between quotes are preserved.
 */
function cutMessage(msg) {

	if(msg.length < 2 || msg.charAt(0) != '[') {
		return "";
	}
	var pos;
	var depth = 0;
	var in_string = false;
	var len = msg.length;

	for(pos = 0; pos < len; pos++) {
		if(msg.charAt(pos) == '"') {
			in_string = !in_string;
			continue;
		}
		if(in_string) { // do not consider characters that are part of a string.
			continue;
		}
		// opening and closing brackets, check for depth.
		if(msg.charAt(pos) == '[') {
			depth++;
		} else if(msg.charAt(pos) == ']') {
			depth--;
		}

		if(depth == 0) {
			return msg.substr(0, pos+1);
		}
	}

	return "";

}

function CometClient(host){

	this.host = host;
	this.pos = 0;
	this.seq = 0;

	// detect if client can stream data using "xhr.readyState=3" or not
	this.canStream = 1;
	this.isIE = false;
	var noStream = new Array("Chrome", "WebKit", "KHTML", "MSIE");
	for(i = 0; i < noStream.length; i++) {
		if(navigator.appVersion.indexOf(noStream[i]) != -1) {
			this.canStream = 0;
			break;
		}
	}
	if(navigator.appVersion.indexOf("MSIE") != -1) {
		this.isIE = true;
	}

	this.disconnect = function() {
		window.clearTimeout(this.reconnectionTimeout); // cancel reconnect.
		// console.log("CLOSE");
		try {
			this.xhr.onreadystatechange = function(){};
			this.xhr.abort();
		} catch(e) {}
	}

	this.send = function(channel, msg) {

		var url = "http://" + this.host + "/publish?name="+channel+"&data="+msg;
		var xhr = new XMLHttpRequest;
		xhr.open("get",url, true);
		xhr.send(null);
	}

	this.connect = function(channel, onMsg) {
		// console.log("CONNECT");
		var comet = this;
		comet.xhr = new XMLHttpRequest;
		comet.pos = 0;
		if(comet.canStream) {
			comet.reconnectionTimeout = window.setTimeout(function() {comet.disconnect(); comet.connect(channel, onMsg);}, 5000);
		}

		var url = "http://"+this.host+"/subscribe?name="+channel+"&keep="+this.canStream+"&seq="+this.seq;
		// console.log(url);
		comet.xhr.open("get", url, true);
		comet.xhr.onreadystatechange = function() {
			// console.log("entered onreadystatechange ("+comet.xhr.readyState+")");
			if(comet.xhr.readyState != 4 && comet.canStream == 0) {
				return; // wait for state 4.
			}

			if(comet.xhr.readyState == 4 && comet.xhr.status != 200) { // error...
				window.setTimeout(function() {comet.connect(channel, onMsg);}, 1000); // reconnect soon.
				return;
			}

			var data = comet.xhr.responseText;
			if(comet.xhr.readyState === 3 || comet.xhr.readyState == 4) {
				// console.log("data.length=", data.length);
				if(data.length == 0) {
					// console.log("readyState=", comet.xhr.readyState, ", length=0");
					setTimeout(function() {
						comet.connect(channel, onMsg);
					}, 1000); // reconnect soon.
					return;
				}

				// console.log("got a message:", data);
				do {
					// this might only be the first part of our current packet.
					var msg = cutMessage(data.substr(comet.pos));
					if(msg.length) try {
						var obj;
						if(comet.isIE) {
							obj = eval("("+msg+")");
						} else {
							obj = JSON.parse(msg);
						}
						comet.pos += msg.length; // add to what we've read so far.

						if(obj[1] && obj[1].seq) { // new timestamp
							comet.seq = obj[1].seq;
							if(comet.canStream == 0) {
								// console.log("new seq:", comet.seq);
							}
						}

						switch(obj[0]) {
							case 'msg': // regular message
								if(obj[1].channel == channel) {
									if(onMsg) {
										try {
											onMsg(obj[1]);
										} catch(e) {} // ignore client errors
									}
								}
								break;
						}
					} catch(e) { // TODO: how do we handle errors?
						// console.log("fail line 66:", e);
						comet.xhr.abort(); // avoid duplicates upon reconnection
						setTimeout(function() {
							comet.connect(channel, onMsg);
						}, 1000); // reconnect soon.
					}
					

					if(comet.pos < data.length) {
						// console.log("comet.pos=", comet.pos, ", msg.length=", msg.length, ", data.length=", data.length);
						break;
					}
					// there might be more in this event: consume the whole string.
				} while(comet.pos < data.length);

			}
			if(comet.xhr.readyState == 4) { // reconnect
				// console.log("END, readyState=4, length=", data.length);
				// TODO: if IE, reconnect directly? not sure yet.
				if(this.canStream) {
					window.setTimeout(function() {
						comet.connect(channel, onMsg);
					}, 1000);
				} else {
					comet.connect(channel, onMsg);
				}
			}
		};
		comet.xhr.send(null);
	}
};

