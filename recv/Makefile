CFLAGS=-Wall -g

all: ulink_test

ulink_test: ulink_test.o ulink.o
	$(CC) -o $@ $^ -lpcap -lrt

%.o:%.c
	$(CC) -c -o $@ $^ $(CFLAGS)
