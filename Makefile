CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pedantic
LDFLAGS = -lSDL2 -lSDL2_ttf -lpthread

all: client server

common.o: common.c common.h
	$(CC) $(CFLAGS) -c common.c

client.o: client.c common.h
	$(CC) $(CFLAGS) -c client.c

server.o: server.c common.h
	$(CC) $(CFLAGS) -c server.c

client: client.o common.o
	$(CC) client.o common.o -o client $(LDFLAGS)

server: server.o common.o
	$(CC) server.o common.o -o server $(LDFLAGS)

clean:
	rm -f *.o client server

.PHONY: all clean
