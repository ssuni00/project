CC = gcc
CFLAGS = -I/home/s22200809/local/include -I/home/s22200809/local/include/ncurses -Wall -g
LDFLAGS = -L/home/s22200809/local/lib -lncurses

all: server

server: server.o 
	$(CC) $(CFLAGS) server.o -o server $(LDFLAGS)

server.o: server.c server.h
	$(CC) $(CFLAGS) -c server.c

clean:
	rm -f *.o server
