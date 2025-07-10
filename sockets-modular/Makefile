# Makefile

CC = gcc
CFLAGS = -Wall -O2

all: client server

client: client.c protocol.c
	$(CC) $(CFLAGS) -o client client.c protocol.c

server: server.c protocol.c
	$(CC) $(CFLAGS) -o server server.c protocol.c

clean:
	rm -f client server
