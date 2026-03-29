CC = gcc
CFLAGS = -std=c11 -g2 -ggdb -pedantic -W -Wall -Wextra

DEBUG = ./build/debug
RELEASE = ./build/release
OUT_DIR = $(DEBUG)

ifeq ($(MODE),release)
  CFLAGS = -std=c11 -pedantic -W -Wall -Wextra -Werror
  OUT_DIR = $(RELEASE)
endif

SERVER_OBJ = $(OUT_DIR)/server.o
CLIENT_OBJ = $(OUT_DIR)/client.o
SERVER_BIN = $(OUT_DIR)/myserver
CLIENT_BIN = $(OUT_DIR)/myclient

all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_OBJ)
	@mkdir -p $(OUT_DIR)
	$(CC) $(CFLAGS) $(SERVER_OBJ) -o $@

$(CLIENT_BIN): $(CLIENT_OBJ)
	@mkdir -p $(OUT_DIR)
	$(CC) $(CFLAGS) $(CLIENT_OBJ) -o $@

$(OUT_DIR)/%.o: src/%.c
	@mkdir -p $(OUT_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean

clean:
	rm -rf $(DEBUG)/* $(RELEASE)/*
