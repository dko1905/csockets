

OBJ = main.o

all: udpmsg

.c.o:
	$(CC) -c $< $(CFLAGS)

udpmsg: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o udpmsg

clean:
	rm -f $(OBJ) udpmsg
