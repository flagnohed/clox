CC = gcc
CFLAGS = -g -Wall -O2 
OBJS = objs/chunk.o objs/compiler.o objs/debug.o objs/main.o \
	objs/memory.o objs/objects.o objs/scanner.o objs/value.o \
	objs/vm.o 

clox: $(OBJS)
	$(CC) -o clox $(OBJS)

objs/%.o: %.c
	$(CC) -c -o $@ $^ $(CFLAGS)

