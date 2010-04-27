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

### Notes
* /meta/connect takes 3 more (optional) parameters:
    * `keep`: Use HTTP streaming or close connection after every push (value=`0` or `1`, defaults to `1`)
    * `seq`: Stream messages from a the sequence number up. Example: If 1000 messages have been sent, `seq=990` will push 10 messages. This parameter still observes `keep`.
    * `callback`: function name for a JSONP callback.

→ That’s it for the proof of concept!

#### TODO
* Complete iframe page.
* Add URL that can be loaded using a <script> tag.
* Add syslog support.
* Add support for HTML5 WebSockets
* Lots of cleanup code.
