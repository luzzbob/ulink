CFLAGS=-Wall -O2

all: ulink_test

ulink_test: ulink_test.o ulink.o
	$(CC) -o $@ $^ -lpcap -lpthread -lm

%.o:%.c
	$(CC) -c -o $@ $^ $(CFLAGS)


clean:
	$(RM) -fr ulink_test *.o
