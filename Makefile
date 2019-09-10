# Global directories of project
SRCDIR=src
OBJDIR=obj
HDRDIR=include

# CFlags
EXEC=main
CC=gcc
CFLAGS=-Wall -Wextra -I$(HDRDIR) -fPIC -g
LDFLAGS=-lX11 -lm

all: $(EXEC)

_DEPS=core.h dbg.h unpack.h screen.h packed/ocr.h gen/rc4_consts.txt gen/rc4_keys.txt kdtree.h img.h packed/check.h sha256.h rc4.h
_SRC=$(EXEC).c dbg.c unpack.c packed/ocr.c dbg_parser.c screen.c kdtree.c img.c packed/check.c sha256.c rc4.c
_OBJ=$(_SRC:.c=.o)
DEPS=$(patsubst %,$(HDRDIR)/%,$(_DEPS))
OBJ=$(patsubst %,$(OBJDIR)/%,$(_OBJ))
SRC=$(patsubst %,$(SRCDIR)/%,$(_SRC))

test_obfuscation: CFLAGS += -D TEST_OBFUSCATION # -D DEBUG -D DEBUG_MAIN -D DEBUG_LOAD -D DEBUG_2PAC
test_obfuscation: $(OBJ)

debug: CFLAGS += -D DEBUG_OCR -D DEBUG -D DEBUG_MAIN -D DEBUG_LOAD -D DEBUG_CHECK # -D DEBUG_IMG
debug: $(EXEC)

release: CFLAGS += -D RELEASE -D KD_LOAD
release: flag kd_load

flag:
	python2 ./script/gen_flag.py >> data/flag.txt

kd_load: CFLAGS += -D KD_LOAD
kd_load: $(EXEC)
	python3 script/rc4.py "This program cannot be run in DOS mode" data/kd.bin data/kd.enc
	cat data/kd.enc >> $(EXEC).2pac
	python2 -c "import struct; print(struct.pack('<I', $$(wc -c < data/kd.bin)))" | head -c -1 >> $(EXEC).2pac
		
# Generic rules
$(EXEC): $(OBJ)
	$(CC) -o $@ $(OBJ) $(CFLAGS) $(LDFLAGS)
	python3 ./script/2pac.py $(EXEC) ./dbg/2pac.debugging_script ./include/gen/rc4_keys.txt
	chmod u+x $(EXEC).2pac

$(OBJDIR)/$(EXEC).o: $(DEPS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -o $@ -c $< $(CFLAGS) $(LDFLAGS)

$(DEPS):
	python2 ./script/gen_keys.py

.PHONY: clean mproper

clean:
	rm -rf $(OBJDIR)/*.o
	rm -rf $(OBJDIR)/packed/*.o
	rm -rf script/*.pyc script/__pycache__
	rm -rf include/gen/*
	rm -rf $(EXEC)
	rm -rf $(EXEC).*

mproper: clean
	rm -rf $(EXEC)

