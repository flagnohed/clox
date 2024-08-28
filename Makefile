CC = gcc
CFLAGS = -g -Wall -O2
OBJS = objs/chunk.o objs/debug.o objs/memory.o objs/main.o objs/value.o

clox: $(OBJS)
	$(CC) -o clox $(OBJS)

objs/%.o: %.c
	$(CC) -c -o $@ $^ $(CFLAGS)

clean:
	rm -f objs/*