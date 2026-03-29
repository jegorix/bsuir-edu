CC = gcc
CFLAGS = -std=c11 -g2 -ggdb -pedantic -W -Wall -Wextra -pthread

.SUFFIXES:   
.SUFFIXES: .c .o

DEBUG   = ./build/debug
RELEASE = ./build/release
OUT_DIR = $(DEBUG)
vpath %.c src
vpath %.h src
vpath %.o build/debug

ifeq ($(MODE), release)
  CFLAGS = -std=c11 -pedantic -W -Wall -Wextra -Wno-unused-parameter -Werror -pthread
  OUT_DIR = $(RELEASE)
  vpath %.o build/release
endif

objects = $(OUT_DIR)/consumer.o $(OUT_DIR)/main.o $(OUT_DIR)/message.o $(OUT_DIR)/producer.o $(OUT_DIR)/queue_cond.o $(OUT_DIR)/queue_sem.o $(OUT_DIR)/stack.o

prog = $(OUT_DIR)/semaphores

all: $(prog)

$(prog) : $(objects)
	$(CC) $(CFLAGS) $(objects) -o $@

$(OUT_DIR)/%.o : %.c
	@mkdir -p $(OUT_DIR)
	$(CC) -c $(CFLAGS) $^ -o $@

.PHONY: clean 
clean:
	@rm -rf $(DEBUG)/* $(RELEASE)/*
