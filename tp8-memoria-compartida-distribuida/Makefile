CC = gcc
CFLAGS = -Wall -pthread -g
SOURCES = communication.c
OBJECTS = communication.o

# Targets
all: client server

client: client.c $(OBJECTS)
	$(CC) $(CFLAGS) -o client client.c $(OBJECTS)

server: newServer.c $(OBJECTS)
	$(CC) $(CFLAGS) -o server newServer.c $(OBJECTS)

communication.o: communication.c communication.h
	$(CC) $(CFLAGS) -c communication.c

clean:
	rm -f client server *.o

.PHONY: all clean