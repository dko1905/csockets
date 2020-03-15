cc=gcc
cflags=-Wall -Werror -std=c11
# Please find good c version.
aout: main.o
	mkdir -p build
	$(cc) main.o -o build/aout $(cflags)

main.o: src/main.c
	$(cc) -c src/main.c $(cflags)
clean:
	rm -rf *.o build/*