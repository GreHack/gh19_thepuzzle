# Global directories of project
SRCDIR=src
OBJDIR=obj
HDRDIR=include

# CFlags
EXEC=main
CC=gcc
CFLAGS=-Wall -Wextra -I$(HDRDIR) -fPIC -g
LDFLAGS=-lX11

all: $(EXEC)

_DEPS=core.h dbg.h unpack.h screen.h packed/hello.h gen/rc4_consts.txt gen/rc4_keys.txt
_SRC=$(EXEC).c dbg.c unpack.c packed/hello.c dbg_parser.c screen.c
_OBJ=$(_SRC:.c=.o)
DEPS=$(patsubst %,$(HDRDIR)/%,$(_DEPS))
OBJ=$(patsubst %,$(OBJDIR)/%,$(_OBJ))
SRC=$(patsubst %,$(SRCDIR)/%,$(_SRC))

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

$(DEPS): # HDIR)/gen/rc4_consts.txt:
	python2 ./script/gen_keys.py

.PHONY: clean mproper

clean:
	rm -rf $(OBJDIR)/*.o
	rm -rf script/*.pyc script/__pycache__
	rm -rf include/gen/rc4_keys.txt

mproper: clean
	rm -rf $(EXEC)

