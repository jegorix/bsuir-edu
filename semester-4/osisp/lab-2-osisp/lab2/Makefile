CC = gcc
CFLAGS = -W -Wall -Wextra -std=c11 -pedantic

all: parent child

parent: parent.c
	$(CC) $(CFLAGS) -o parent parent.c

child: child.c
	$(CC) $(CFLAGS) -o child child.c

clean:
	rm -f parent child

.PHONY: all clean