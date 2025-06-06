CC=gcc
CFLAGS=-Wall -g

DNS_SRC=dns_server.c
SERVER_SRC=file_server.c
CLIENT_SRC=client.c
COMMON=common.h

DNS_BIN=dns_server
SERVER_BIN=file_server
CLIENT_BIN=client

DNS_TARGET=../dfs/dns_server/$(DNS_BIN)
SERVER_TARGET=../dfs/file_server/$(SERVER_BIN)
CLIENT_TARGET=../dfs/client/$(CLIENT_BIN)

all: $(DNS_TARGET) $(SERVER_TARGET) $(CLIENT_TARGET)

$(DNS_TARGET): $(DNS_SRC) $(COMMON)
	$(CC) $(CFLAGS) $(DNS_SRC) -o $(DNS_BIN)
	mkdir -p ../dfs/dns_server
	mv -f $(DNS_BIN) ../dfs/dns_server/

$(SERVER_TARGET): $(SERVER_SRC) $(COMMON)
	$(CC) $(CFLAGS) $(SERVER_SRC) -o $(SERVER_BIN)
	mkdir -p ../dfs/file_server
	mv -f $(SERVER_BIN) ../dfs/file_server/

$(CLIENT_TARGET): $(CLIENT_SRC) $(COMMON)
	$(CC) $(CFLAGS) $(CLIENT_SRC) -o $(CLIENT_BIN)
	mkdir -p ../dfs/client
	mv -f $(CLIENT_BIN) ../dfs/client/

clean:
	rm -f $(DNS_BIN) $(SERVER_BIN) $(CLIENT_BIN)
	rm -f ../dfs/dns_server/$(DNS_BIN) ../dfs/file_server/$(SERVER_BIN) ../dfs/client/$(CLIENT_BIN)