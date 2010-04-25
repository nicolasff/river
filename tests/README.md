This directory contains C programs that test the comet server.

##`bench`: A simple benchmark##
`bench` spawns 2 threads, each with its event loop:

1. The reader thread starts 100 http requests to read on a single channel.
2. The writer thread maintains 100 http requests writing on that channel, replacing them as they terminate.
3. The program reads 1 million occurences of the message sent on the channel, and exits.

**Notes**

* Less than 1 million messages are sent on the channel; each message is sent to 100 readers, so about 10,000 messages are written to the channel, triggering 1 million http pushes.
* The message is “hello-world”, which is sent to the readers in the following format:
`[["msg", {"channel": "chan", "seq": 1729, "data": "hello-world"}]`.  
The sequence number is incremented with each write.
<pre>
$ ./bench
Running 100 readers
Running 100 writers
 1000000 messages left (got        71 bytes so far).
  990000 messages left (got    719273 bytes so far).

		[...] (many more lines)

   20000 messages left (got  72409374 bytes so far).
   10000 messages left (got  73149374 bytes so far).
       0 messages left (got  73889475 bytes so far).
Read 1000000 messages in 11.70 sec: 85506.62/sec
</pre>


-------------------
**Results:** The channel sent out 85506.62 messages per second, 1 million in 11.7 seconds.


##`catchup`, testing the reconnection feature##
Upon connection to a channel, it is possible to provide the last sequence number read on that channel, in order to *catch-up* on missed messages.
**Example**

1. Client A connects to the channel, providing `seq=0`. He stays connected.
2. Client B sends message “X” on the channel.
3. Client A receives `[["msg", {"channel": "test", "seq": 1, "data": "X"}]`, and disconnects by mistake.
4. Client B sends message “Y” on the channel.
5. Client B sends message “Z” on the channel.
6. Client A connects to the channel, providing `seq=1`. He receives the two missing messages:
`[["msg", {"channel": "test", "seq": 2, "data": "Y"}]` and `[["msg", {"channel": "test", "seq": 3, "data": "Z"}]`

In `catchup`, two threads send messages to each other. The writing thread publishes 10 messages per second, and the reading thread catches-up every second. It has expectations on what the next sequence number should be.
The test runs forever.

**Sample run**
<pre>
$ ./catchup 
sent write query
	[more of them...]
sent write query
reading [/subscribe?name=chan&keep=0&seq=0]
got read reply. [
["msg", {"channel": "chan", "seq": 16, "data": "hello-world"}]
["msg", {"channel": "chan", "seq": 17, "data": "hello-world"}]
	[more of them...]
["msg", {"channel": "chan", "seq": 32, "data": "hello-world"}]
["msg", {"channel": "chan", "seq": 33, "data": "hello-world"}]
]

sent write query
	[more of them...]
sent write query
reading [/subscribe?name=chan&keep=0&seq=33]
got read reply. I expect the first message to have sequence number 34. [
["msg", {"channel": "chan", "seq": 34, "data": "hello-world"}]
["msg", {"channel": "chan", "seq": 35, "data": "hello-world"}]
	[more of them...]
["msg", {"channel": "chan", "seq": 42, "data": "hello-world"}]
["msg", {"channel": "chan", "seq": 43, "data": "hello-world"}]
]

sent write query
sent write query
	[etc...]
</pre>
This program starts with a pre-existing channel.  
The reader starts without a known last sequence number (`seq=0`). It gets messages with sequence numbers from 16 to 33.
10 more writes follow, and the reader tries to catch up, by providing `seq=33`, the last sequence number seen on the channel. It expects the next one to be 34. The reply arrives, with messages 34 to 43, corresponding to the previous 10 writes.
