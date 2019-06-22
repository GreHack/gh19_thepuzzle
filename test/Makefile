CC=gcc
CFLAGS=-Wall -Wextra -fPIC -I../include -DTEST
LDFLAGS=

all: test

test: test_parser

test_parser: test_parser_bin
	python test_parser.py

test_parser_bin: ../src/dbg_parser.c test_parser.c
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean:
	rm -f test_parser

