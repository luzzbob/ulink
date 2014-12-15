CFLAGS=-Wall -g

all: ulink_test

ulink_test: ulink_test.o ulink.o
	$(CC) -o $@ $^ -lpcap -lpthread

%.o:%.c
	$(CC) -c -o $@ $^ $(CFLAGS)


clean:
	$(RM) -fr ulink_test *.o
