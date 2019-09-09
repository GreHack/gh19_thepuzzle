CC=gcc
CFLAGS=-Wall -Wextra -fPIC -I../include -DTEST -DTEST_OBFUSCATION
LDFLAGS=

DBGSCRIPT:=2pac.debugging_script
DBG:=>/dev/null

all: test

test: test_obfuscation
# test: test_parser test_obfuscation

test_obfuscation: test_obfuscation_bin.2pac test_obfuscation.py
	python test_obfuscation.py

test_parser: test_parser_bin test_parser.py
	python test_parser.py

test_parser_bin: ../src/dbg_parser.c test_parser.c ../src/dbg.c
	@make -C .. clean $(DBG)
	@make -C .. test_obfuscation $(DBG)
	@$(CC) -o $@ test_parser.c ../obj/dbg_parser.o ../obj/dbg.o $(CFLAGS) $(LDFLAGS) $(DBG)

test_obfuscation_bin: test_obfuscation.c
	@make -C .. clean $(DBG)
	@make -C .. test_obfuscation $(DBG)
	@$(CC) -o $@ test_obfuscation.c ../obj/unpack.o ../obj/dbg.o ../obj/main.o ../obj/rc4.o ../obj/dbg_parser.o $(CFLAGS) $(LDFLAGS) $(DBG)

test_obfuscation_bin.2pac: test_obfuscation_bin ../script/2pac.py
	@python3 ../script/2pac.py $< $(DBGSCRIPT) ../include/gen/rc4_keys.txt 2>&1 >/dev/null
	@chmod +x $@

clean:
	rm -f test_parser_bin test_obfuscation_bin* $(DBGSCRIPT)

