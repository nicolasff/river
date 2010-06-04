#### COMPILE

You need to fetch the http-parser submodule, by typing:
<pre>
git submodule init
git submodule update
</pre>


#### Run demo
1 - run `make clean all`  
2 - run `./cometd`

3 - In a terminal, run:
<pre>
curl "http://127.0.0.1:1234/subscribe?name=public-channel"
</pre>
The call to `/subscribe` is blocking.


4 - And then, in another terminal:
<pre>
curl "http://127.0.0.1:1234/publish?name=public-channel&data=hello-world-of-comet"
</pre>

5 - The call to `/subscribe` returns, with the data published in the channel.


### Embed in an HTML page
The following example considers the page to be at `example.com`, and the comet server at `cometd.example.com`.

1 - Add a reference to the `lib.js` script in your page:
<pre>
	&lt;script type="text/javascript" src="http://cometd.example.com/lib.js?domain=example.com"&gt;&lt;/script&gt;
</pre>
2 - Initialize the server:
<pre>
	document.domain = "example.com";

	function onMsg(msg) {
		alert("received message: ", msg);
	}

	Comet.init(function() {
		var c = new Comet.Client();
		c.connect("public-channel", onMsg); // call onMsg upon reception of a message.
	});
</pre>
3 - The `onMsg` function will now receive all messages published on `public-channel`.


### Notes
* /subscribe takes 3 more (optional) parameters:
    * `keep`: Use HTTP streaming or close connection after every push (value=`0` or `1`, defaults to `1`)
    * `seq`: Stream messages from a the sequence number up. Example: If 1000 messages have been sent, `seq=990` will push 10 messages. This parameter still observes `keep`.
    * `callback`: function name for a JSONP callback.

→ That’s it for the proof of concept!

#### TODO
* Support Flash’s `<policy-file-request>\0` in the WebSocket implementation.
* Find a better name, “cometd” is already taken.
* Add automatic dead channel deletion.
* Cleanup code.
* Remove dependency on dict.c in channels.
* Fix possible bug for the following scenario:
** Client A connects, is added to chan.
** Client A disconnects
** Client B writes to client A
** There is a read() for client A in ws_client_msg
** B's write() fails, client A is removed from the chan.
** A's read() fails, A is removed from the chan (again).
