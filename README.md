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
curl "http://127.0.0.1:1234/meta/newchannel?name=public-channel&key=c4rp2n3H5KzX"
curl "http://127.0.0.1:1234/meta/connect?name=public-channel"
</pre>
The last HTTP call (`/meta/connect`) is blocking.


4 - And then, in another terminal:
<pre>
curl "http://127.0.0.1:1234/meta/publish?name=public-channel&data=hello-world-of-comet&payload=xxx"
</pre>

5 - The `/meta/connect` call returns, with the data published in the channel.

### Notes
* /meta/connect takes 2 more (optional) parameters:
    * `keep`: Use HTTP streaming or close connection after every push (value=`0` or `1`, defaults to `1`)
    * `seq`: Stream messages from a the sequence number up. Example: If 1000 messages have been sent, `seq=990` will push 10 messages. This parameter still observes `keep`.

→ That's it for the proof of concept!

#### TODO
* Complete iframe page.
* Remove channel creation, making it automatic upon subscribing?
* Add JSONP callback parameter.
* Add support for HTML5 WebSockets
* Lots of cleanup code.
* Add enums for returns messages, especially in http_dispatch.{c,h}
* Prepare a better demo.
