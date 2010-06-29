OUT=river
OBJS=src/server.o src/socket.o src/river.o src/channel.o src/http-parser/http_parser.o src/thread.o src/http.o src/http_dispatch.o src/dict.o src/json.o src/websocket.o src/files.o src/md5.o
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

