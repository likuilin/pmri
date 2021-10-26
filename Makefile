CC = clang
CFLAGS = -Wall -Wextra -O2 -g
LDFLAGS = -lpmemobj

test: test.o
	$(CC) -lpmemobj test.o -o test

test.o: test.c
	$(CC) -c $(CFLAGS) test.c -o test.o
