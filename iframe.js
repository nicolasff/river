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

function ajax(url) {
	var xhr = new XMLHttpRequest;
	xhr.open("get",url, true);
	xhr.send(null);
}

/**
 * Cuts the first part of a message and returns it. What's after is discarded.
 * if msg is '[abc][def]', this function will return '[abc]' only.
 * if msg is '["abc]ef"]gh', this function will return '["abc]ef"]': characters between quotes are preserved.
 */
function cutMessage(msg) {

	if(msg.length < 2 || msg[0] != '[') {
		return "";
	}
	var pos;
	var depth = 0;
	var in_string = false;
	var len = msg.length;

	for(pos = 0; pos < len; pos++) {
		if(msg[pos] == '"') {
			in_string = !in_string;
			continue;
		}
		if(in_string) { // do not consider characters that are part of a string.
			continue;
		}
		// opening and closing brackets, check for depth.
		if(msg[pos] == '[') {
			depth++;
		} else if(msg[pos] == ']') {
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

	this.auth = function(uid, sid) {
		
		this.uid = uid;
		this.sid = sid;

		var url = "http://"+this.host+"/meta/authenticate?uid="+this.uid+"&sid="+this.sid;
		ajax(url);
	}

	this.reconnect = function() {
		// console.log("CLOSE");
		try {
			this.xhr.abort();
		} catch(e) {}
	}

	this.send = function(channel, msg) {

		var url = "http://" + this.host + "/meta/publish?name="+channel+"&data="+msg+"&uid="+this.uid+"&sid="+this.sid;
		ajax(url);
	}

	this.connect = function(channel, onMsg, onMeta) {
		// console.log("CONNECT");
		var comet = this;
		comet.xhr = new XMLHttpRequest;
		comet.pos = 0;
		// window.setTimeout(function() {comet.reconnect();}, 6000);

		comet.xhr.open("get", "http://"+this.host+"/meta/connect?name="+channel+"&uid="+this.uid+"&sid="+this.sid+"&seq="+this.seq, true);
		comet.xhr.onreadystatechange = function() {

			if(comet.xhr.readyState == 4 && comet.xhr.status != 200) { // error...
				window.setTimeout(function() {comet.connect(channel, onMsg, onMeta);}, 1000); // reconnect in 1 second.
				return;
			}

			var data = comet.xhr.responseText;
			if(comet.xhr.readyState === 3 || comet.xhr.readyState == 4) {

				if(data.length == 0) {
					// console.log("readyState=", comet.xhr.readyState, ", length=0");
					setTimeout(function() {
						comet.connect(channel, onMsg, onMeta);
					}, 1000); // reconnect soon.
					return;
				}

				do {
					// this might only be the first part of our current packet.
					var msg = cutMessage(data.substr(comet.pos));

					if(msg.length) try {
						var obj = JSON.parse(msg);
						comet.pos += msg.length; // add to what we've read so far.

						if(obj[1] && obj[1].seq) { // new timestamp
							comet.seq = obj[1].seq;
						}

						switch(obj[0]) {
							case 'msg': // regular message
								if(obj[1].channel == channel) {
									if(onMsg) onMsg(obj[1]);
								} // in the case of a wrong channel, raise error? (TODO)
								break;

							case 'meta': // TODO: handle service messages, meta-messages.
								if(onMeta) onMeta(obj[1]);
								break;
						}
					} catch(e) { // TODO: how do we handle errors?
						// console.log("fail line 66:", e);
						setTimeout(function() {
							comet.connect(channel, onMsg, onMeta);
						}, 1000); // reconnect soon.
					}
				// there might be more in this event: consume the whole string.
				// console.log("comet.pos=", comet.pos, ", msg.length=", msg.length, ", data.length=", data.length);
				} while(comet.pos < data.length);

			}
			if(comet.xhr.readyState == 4) { // reconnect
				// console.log("readyState=4");
				// TODO: if IE, reconnect directly? not sure yet.
				window.setTimeout(function() {
					comet.connect(channel, onMsg, onMeta);
				}, 1000);
			}
		};
		comet.xhr.send(null);
	}
};

