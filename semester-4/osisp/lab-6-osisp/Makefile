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

genObjects = $(OUT_DIR)/gen.o $(OUT_DIR)/index.o
viewObjects = $(OUT_DIR)/view.o $(OUT_DIR)/index.o
sortObjects = $(OUT_DIR)/sort_index.o $(OUT_DIR)/sort.o $(OUT_DIR)/file.o $(OUT_DIR)/heap.o $(OUT_DIR)/index.o
menuObjects = $(OUT_DIR)/lab6_menu.o

genProg = $(OUT_DIR)/gen
viewProg = $(OUT_DIR)/view
sortProg = $(OUT_DIR)/sort_index
menuProg = $(OUT_DIR)/lab6_menu

all: $(genProg) $(viewProg) $(sortProg) $(menuProg)

$(genProg) : $(genObjects)
	$(CC) $(CFLAGS) $(genObjects) -o $@

$(viewProg) : $(viewObjects)
	$(CC) $(CFLAGS) $(viewObjects) -o $@

$(sortProg) : $(sortObjects)
	$(CC) $(CFLAGS) $(sortObjects) -o $@

$(menuProg) : $(menuObjects)
	$(CC) $(CFLAGS) $(menuObjects) -o $@

$(OUT_DIR)/%.o : %.c
	@mkdir -p $(OUT_DIR)
	$(CC) -c $(CFLAGS) $^ -o $@

.PHONY: clean 
clean:
	@rm -rf $(DEBUG)/* $(RELEASE)/*
