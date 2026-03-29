CC = gcc
CFLAGS = -std=c11 -g2 -ggdb -pedantic -W -Wall -Wextra

.SUFFIXES:
.SUFFIXES: .c .o

DEBUG   = ./build/debug
RELEASE = ./build/release
OUT_DIR = $(DEBUG)

vpath %.c src
vpath %.h src
vpath %.o build/debug

ifeq ($(MODE), release)
  CFLAGS = -std=c11 -pedantic  -W -Wall -Wextra -Wno-unused-parameter -Werror
  OUT_DIR = $(RELEASE)
  vpath %.o build/release
endif

objects_parent = $(OUT_DIR)/parent.o
objects_child = $(OUT_DIR)/child.o

parent = $(OUT_DIR)/parent
child = $(OUT_DIR)/child

all: $(parent) $(child)

$(parent): $(objects_parent)
	$(CC) $(CFLAGS) $^ -o $@

$(child): $(objects_child)
	$(CC) $(CFLAGS) $^ -o $@

$(OUT_DIR)/%.o: src/%.c
	@mkdir -p $(OUT_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY: clean

clean:
	@rm -rf $(DEBUG)/* $(RELEASE)/*
