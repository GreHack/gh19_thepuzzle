CC=gcc
CFLAGS=-Wall -Wextra -fPIC -I../include -DTEST -DTEST_OBFUSCATION
LDFLAGS=

DBGSCRIPT:=2pac.debugging_script

all: test

test: test_parser test_obfuscation

test_obfuscation: test_obfuscation_bin test_obfuscation.py
	python test_obfuscation.py

test_parser: test_parser_bin test_parser.py
	python test_parser.py

test_parser_bin: ../src/dbg_parser.c test_parser.c ../src/dbg.c
	@$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) 2>/dev/null

test_obfuscation_bin: test_obfuscation.c ../src/main.c ../src/dbg_parser.c ../src/dbg.c ../src/unpack.c
	@$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) 2>/dev/null
	@python2 ../script/2pac.py $@ $(DBGSCRIPT) ../include/gen/rc4_keys.txt 2>&1 >/dev/null
	@mv 2pac_$@ $@
	@chmod +x $@

clean:
	rm -f test_parser_bin test_obfuscation_bin* $(DBGSCRIPT)

