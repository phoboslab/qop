CC = gcc
CFLAGS = -Wall -Wextra -Werror

all: qopconv

qopconv: qopconv.c qop.h
	$(CC) -std=c99 $(CFLAGS) -O3 qopconv.c -o qopconv

example: qopconv qop.h example.c
	$(CC) -std=gnu99 $(CFLAGS) -O3 example.c -o example
	./qopconv qop.h example_archive.qop
	cat example example_archive.qop > example_with_archive
	chmod a+x example_with_archive

clean:
	rm qopconv example example_archive.qop example_with_archive

# Phony targets
.PHONY: all clean