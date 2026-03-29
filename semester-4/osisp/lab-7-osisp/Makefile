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
  CFLAGS = -std=c11 -pedantic -W -Wall -Wextra -Wno-unused-parameter -Werror
  OUT_DIR = $(RELEASE)
  vpath %.o build/release
endif

genObjects = $(OUT_DIR)/gen.o
mainObjects = $(OUT_DIR)/records_work.o

genProg = $(OUT_DIR)/gen
mainProg = $(OUT_DIR)/records_work

all: $(genProg) $(mainProg)

$(genProg) : $(genObjects)
	$(CC) $(CFLAGS) $(genObjects) -o $@

$(mainProg) : $(mainObjects)
	$(CC) $(CFLAGS) $(mainObjects) -o $@

$(OUT_DIR)/%.o : %.c
	@mkdir -p $(OUT_DIR)
	$(CC) -c $(CFLAGS) $^ -o $@

.PHONY: clean 
clean:
	@rm -rf $(DEBUG)/* $(RELEASE)/*
