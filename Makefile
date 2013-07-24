COMPILER=clang
CFLAGS=-Wall -g -pedantic


all: dirsync

dirsynctypes.o: dirsynctypes.c dirsynctypes.h
	$(COMPILER) $(CFLAGS) -c dirsynctypes.c

dirsync: dirsynctypes.o dirsync.c
	$(COMPILER) $(CFLAGS) -o dirsync dirsync.c dirsynctypes.o

clean:
	\rm *.o *~