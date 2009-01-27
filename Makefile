OUT=cometd
OBJS=server.o udp.o socket.o user.o http_dispatch.o cometd.o connection.o channel.o message.o
CFLAGS=-g -I/opt/libevent/include/ -Wall -Wextra `pkg-config glib-2.0 --cflags`
LDFLAGS=-levent -L/opt/libevent/lib -lpthread `pkg-config glib-2.0 --libs` -lreadline

all: $(OUT)

$(OUT): $(OBJS)
	$(CC) $(LDFLAGS) -o $(OUT) $(OBJS)

%.o: %.c %.h
	$(CC) -c $(CFLAGS) -o $@ $<

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f $(OBJS) $(OUT)

