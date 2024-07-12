# Makefile, versao 1
# Sistemas Operativos, DEI/IST/ULisboa 2019-20

CC   = gcc
LD   = gcc
CFLAGS =-Wall -g -std=gnu99 -I../
LDFLAGS=-lm

# A phony target is one that is not really the name of a file
# https://www.gnu.org/software/make/manual/html_node/Phony-Targets.html
.PHONY: all clean run

all: tecnicofs-nosync tecnicofs-mutex tecnicofs-rwlock

tecnicofs-nosync: lib/bst.o fs.o main3.o
	$(LD) $(CFLAGS) $(LDFLAGS) -pthread -o tecnicofs-nosync lib/bst.o fs.o main3.o

tecnicofs-mutex: lib/bst.o fs.o main1.o
	$(LD) $(CFLAGS) $(LDFLAGS) -pthread -o tecnicofs-mutex lib/bst.o fs.o main1.o

tecnicofs-rwlock: lib/bst.o fs.o main2.o
	$(LD) $(CFLAGS) $(LDFLAGS) -pthread -o tecnicofs-rwlock lib/bst.o fs.o main2.o

lib/bst.o: lib/bst.c lib/bst.h
	$(CC) $(CFLAGS) -o lib/bst.o -c lib/bst.c

fs.o: fs.c fs.h lib/bst.h
	$(CC) $(CFLAGS) -o fs.o -c fs.c

main1.o: main.c fs.h lib/bst.h
	$(CC) $(CFLAGS) -DMUTEX -o main1.o -c main.c

main2.o: main.c fs.h lib/bst.h
	$(CC) $(CFLAGS) -DRWLOCK -o main2.o -c main.c

main3.o: main.c fs.h lib/bst.h
	$(CC) $(CFLAGS) -o main3.o -c main.c

clean:
	@echo Cleaning...
	rm -f lib/*.o *.o tecnicofs-nosync tecnicofs-mutex tecnicofs-rwlock

run: tecnicofs
	./tecnicofs
