.POSIX:
.SUFFIXES:
CC      = cc
CFLAGS  = -ansi -Wall -O3
LDFLAGS =
LDLIBS  =
PREFIX  = ${HOME}/.local

sources = src/udtc.c
objects = $(sources:.c=.o)

udtc: $(objects)
	$(CC) $(LDFLAGS) -o $@ $(objects) $(LDLIBS)

src/udtc.o: src/udtc.c config.h src/docs.h src/optparse.h src/utf8.h

clean:
	rm -f udtc $(objects)

install: udtc udtc.1
	mkdir -p $(PREFIX)/bin
	mkdir -p $(PREFIX)/share/man/man1
	install -m 755 udtc $(PREFIX)/bin
	gzip < udtc.1 > $(PREFIX)/share/man/man1/udtc.1.gz

uninstall:
	rm -f $(PREFIX)/bin/udtc
	rm -f $(PREFIX)/share/man/man1/udtc.1.gz

.SUFFIXES: .c .o
.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<
