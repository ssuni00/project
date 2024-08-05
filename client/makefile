CC = gcc
CFLAGS = -I/home/s22200809/local/include -I/home/s22200809/local/include/ncurses
LDFLAGS = -L/home/s22200809/local/lib -lncurses


all: client

client: client.o
	$(CC) $(CFLAGS) -o client client.o $(LDFLAGS)

client.o: client.c client.h
	$(CC) $(CFLAGS) -c client.c

clean:
	rm -f *.o client
