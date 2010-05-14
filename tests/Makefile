OUT=bench catchup websocket
CFLAGS=-O0 -ggdb -Wall -Wextra
LDFLAGS=-levent -lpthread

all: $(OUT) Makefile

%: %.o Makefile
	$(CC) $(LDFLAGS) -o $@ $<

%.o: %.c Makefile
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f *.o $(OUT)

