OUT=bench catchup
OBJS=bench.o catchup.o
CFLAGS=-O3 -Wall -Wextra
LDFLAGS=-levent -lpthread

all: $(OUT) Makefile

bench: bench.o Makefile
	$(CC) $(LDFLAGS) -o bench bench.o

catchup: catchup.o Makefile
	$(CC) $(LDFLAGS) -o catchup catchup.o

%.o: %.c %.h Makefile
	$(CC) -c $(CFLAGS) -o $@ $<

%.o: %.c Makefile
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f $(OBJS) $(OUT)

