# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g -pthread
INCLUDES = -I./include

# Target executables
TARGETS = discovery_server chat_client server_fork server_thread server_epoll benchmark_client

# Directories
SRC_DIR = src
OBJ_DIR = obj

all: $(TARGETS)

discovery_server: 
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRC_DIR)/discovery/discovery.c $(SRC_DIR)/shared/network_utils.c -o $@

chat_client: 
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRC_DIR)/client/chat_client.c $(SRC_DIR)/shared/network_utils.c -o $@

server_fork: 
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRC_DIR)/server/server_fork.c $(SRC_DIR)/shared/network_utils.c -o $@

server_thread: 
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRC_DIR)/server/server_thread.c $(SRC_DIR)/shared/network_utils.c -o $@

server_epoll: 
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRC_DIR)/server/server_epoll.c $(SRC_DIR)/shared/network_utils.c -o $@

benchmark_client: 
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRC_DIR)/client/benchmark_client.c $(SRC_DIR)/shared/network_utils.c -o $@

clean:
	rm -rf $(TARGETS) $(OBJ_DIR)

.PHONY: all clean