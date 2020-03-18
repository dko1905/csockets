CC=gcc
CFLAGS=-Wall -Werror -std=c11

all: socketloop.o main.o clienthandle.o
	mkdir -p build/
	$(CC) $(shell find . -name "*.o") -o build/aout $(CFLAGS)
socketloop.o: src/socketloop.c
	$(CC) -c src/socketloop.c $(CFLAGS)
main.o: src/main.c
	$(CC) -c src/main.c $(CFLAGS)
clienthandle.o: src/clienthandle.c 
	$(CC) -c src/clienthandle.c $(CFLAGS)

clean:
	rm -rf *.o build/*