CC=gcc
CFLAGS=-w -I.
DEPS = httpfuncs.h
OBJ = server.o httpfunc.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

server: $(OBJ)
	gcc -o $@ $^ $(CFLAGS)

clean: 
	rm *.o
