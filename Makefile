OUT=cometd
OBJS=server.o socket.o user.o http_dispatch.o cometd.o channel.o message.o queue.o http-parser/http_parser.o
CFLAGS=-g -ggdb -Wall -Wextra `pkg-config glib-2.0 --cflags` -Ihttp-parser
LDFLAGS=-levent -lpthread `pkg-config glib-2.0 --libs`

all: $(OUT) Makefile

queue-demo: queue-demo.o queue.o http-parser/http_parser.o Makefile
	$(CC) $(LDFLAGS) -o queue-demo queue-demo.o queue.o http-parser/http_parser.o

$(OUT): $(OBJS) Makefile
	$(CC) $(LDFLAGS) -o $(OUT) $(OBJS)

%.o: %.c %.h Makefile
	$(CC) -c $(CFLAGS) -o $@ $<

%.o: %.c Makefile
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f $(OBJS) $(OUT)

