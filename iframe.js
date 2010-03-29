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

	this.auth = function(uid, sid) {
		
		this.uid = uid;
		this.sid = sid;

		url = "http://"+this.host+"/meta/authenticate?uid="+this.uid+"&sid="+this.sid;
		ajax(url);
	}

	this.connect = function(channel, onMsg, onMeta) {
		
		var xhr = new XMLHttpRequest;
		xhr.open("get", "http://"+this.host+"/meta/connect?name="+channel+"&uid="+this.uid+"&sid="+this.sid, true);
		var comet = this;
		xhr.onreadystatechange = function() {
			var data = xhr.responseText;
			if(xhr.readyState === 3 || xhr.readyState == 4) {

				// check if we've read anything before.
				var curLen = data.length;
				do {
					// this might only be the first part of our current packet.

					var msg = cutMessage(data.substr(comet.pos));

					if(msg.length) try {
						var obj = JSON.parse(msg);
						comet.pos += msg.length; // add to what we've read so far.

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
						xhr = null; // close
						console.log("fail line 66:", e);
						// comet.connect(channel, onMsg);
					}
				// there might be more in this event: consume the whole string.
				} while(comet.pos + msg.length != data.length);

			}
			if(xhr.readyState == 4) { // reconnect
				console.log("fail line 74:", e);
				// comet.connect(channel, onMsg);
			}
		};
		xhr.send(null);
	}
};

