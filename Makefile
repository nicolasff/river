OUT=cometd
OBJS=server.o socket.o user.o http_dispatch.o cometd.o connection.o channel.o message.o
CFLAGS=-O3 -I/opt/libevent/include/ -Wall -Wextra `pkg-config glib-2.0 --cflags` -Ihttp-parser
LDFLAGS=-levent -L/opt/libevent/lib -lpthread `pkg-config glib-2.0 --libs`

all: $(OUT)

queue-demo: queue-demo.o queue.o http-parser/http_parser.o
	$(CC) $(LDFLAGS) -o queue-demo queue-demo.o queue.o http-parser/http_parser.o

$(OUT): $(OBJS)
	$(CC) $(LDFLAGS) -o $(OUT) $(OBJS)

%.o: %.c %.h
	$(CC) -c $(CFLAGS) -o $@ $<

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f $(OBJS) $(OUT)

