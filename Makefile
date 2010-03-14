OUT=cometd
OBJS=server.o socket.o user.o http_dispatch.o cometd.o channel.o message.o queue.o http-parser/http_parser.o http.o dict.o
CFLAGS=-g -ggdb -Wall -Wextra -Ihttp-parser
LDFLAGS=-levent -lpthread

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

