OUT=cometd
OBJS=src/server.o src/socket.o src/http_dispatch.o src/cometd.o src/channel.o src/message.o src/queue.o src/http-parser/http_parser.o src/http.o src/dict.o src/json.o src/conf.o src/websocket.o
CFLAGS=-O3 -ggdb -Wall -Wextra -Isrc/http-parser
LDFLAGS=-levent -lpthread

all: $(OUT) Makefile

$(OUT): $(OBJS) Makefile
	$(CC) $(LDFLAGS) -o $(OUT) $(OBJS)

%.o: %.c %.h Makefile
	$(CC) -c $(CFLAGS) -o $@ $<

%.o: %.c Makefile
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f $(OBJS) $(OUT)

