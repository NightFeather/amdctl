CC=gcc
CFLAGS=-Wall -pedantic -Wextra -std=c99 -O2 -lm
all: amdctl
%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) 
