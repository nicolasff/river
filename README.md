#### COMPILE

You need to fetch the http-parser submodule, by typing:
<pre>
git submodule init
git submodule update
</pre>


#### Run demo
1 - run `make clean all`  
2 - run `./river`

3 - In a terminal, run:
<pre>
curl -N -d "name=public-channel" "http://127.0.0.1:1234/subscribe"
</pre>
The call to `/subscribe` is blocking.


4 - And then, in another terminal:
<pre>
curl -d "name=public-channel&data=hello-world-of-comet" "http://127.0.0.1:1234/publish"
</pre>

5 - The call to `/subscribe` returns, with the data published in the channel.


### Embed in an HTML page
The following example considers the page to be at `example.com`, and the comet server at `river.example.com`.

1 - Add a reference to the `lib.js` script in your page:
<pre>
	&lt;script type="text/javascript" src="http://river.example.com/lib.js?domain=example.com"&gt;&lt;/script&gt;
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
* Parameters can be sent in GET or POST.
* /subscribe takes 3 more (optional) parameters:
    * `keep`: Use HTTP streaming or close connection after every push (value=`0` or `1`, defaults to `1`)
    * `seq`: Stream messages from a the sequence number up. Example: If 1000 messages have been sent, `seq=990` will push 10 messages.
    * `callback`: function name for a JSONP callback.
* The *tests* directory contains two benchmarking programs, `websocket` and `bench`. They can simulate large numbers of concurrent clients reading and writing messages. A single core can process more than 450,000 messages per second.


#### TODO
* Test support for Flash’s `<policy-file-request>\0` in the WebSocket implementation.
* More efficient channel deletion.
* Remove dependency on dict.c in channels, use own HT.
