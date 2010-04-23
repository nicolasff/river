This directory contains C programs that test the comet server.

##Benchmark##
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

