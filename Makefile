CC=gcc
CFLAGS=-Wall -O2 -g
LDFLAGS=$(shell pkg-config --libs uuid)
SRC=$(wildcard *.c)
OBJS=$(SRC:%.c=%.o)

cfvmfs: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
