OUT=river
OBJS=src/server.o src/socket.o src/river.o src/channel.o src/http-parser/http_parser.o src/http.o src/http_dispatch.o src/dict.o src/json.o src/websocket.o src/files.o src/md5.o src/conf.o src/mem.o
CFLAGS=-O3 -Wall -Wextra -Isrc/http-parser
LDFLAGS=-levent
prefix=/usr

all: $(OUT) Makefile

install:
	mkdir -p $(prefix)/bin
	cp river $(prefix)/bin/river

$(OUT): $(OBJS) Makefile
	$(CC) $(LDFLAGS) -o $(OUT) $(OBJS)

%.o: %.c %.h Makefile
	$(CC) -c $(CFLAGS) -o $@ $<

%.o: %.c Makefile
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f $(OBJS) $(OUT)

