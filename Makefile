# Global directories of project
SRCDIR=src
OBJDIR=obj
HDRDIR=include

# CFlags
EXEC=main
CC=gcc
CFLAGS=-Wall -Wextra -I$(HDRDIR) -fPIC
LDFLAGS=

all: $(EXEC)

_DEPS=core.h dbg.h unpack.h packed/hello.h
_SRC=$(EXEC).c dbg.c unpack.c packed/hello.c dbg_parser.c
_OBJ=$(_SRC:.c=.o)
DEPS=$(patsubst %,$(HDRDIR)/%,$(_DEPS))
OBJ=$(patsubst %,$(OBJDIR)/%,$(_OBJ))
SRC=$(patsubst %,$(SRCDIR)/%,$(_SRC))

# Test rules
test: $(EXEC) _test

test_random: gentest $(EXEC)
	(cd test; make)

_test:
	(cd test; make)

release: CFLAGS += -D RELEASE
release: $(EXEC)
		
# Generic rules
$(EXEC): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)
	python2 ./script/2pac.py $(EXEC)
	mv 2pac_$(EXEC) $(EXEC)
	chmod +x $(EXEC)

$(OBJDIR)/$(EXEC).o: $(DEPS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -o $@ -c $< $(CFLAGS) $(LDFLAGS)

.PHONY: clean mproper

# Prevent intermediate file deletion
.PRECIOUS: $(GENDIR)/%.c

clean:
	rm -rf $(OBJDIR)/*.o
	rm -rf $(OBJDIR)/handle/*.o
	rm -rf script/*.pyc script/__pycache__

mproper: clean
	rm -rf $(EXEC)

