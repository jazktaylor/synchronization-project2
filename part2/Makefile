CC=gcc
CFLAGS=-I. -pthread -std=c99
DEPS = BENSCHILLIBOWL.h
OBJ = BENSCHILLIBOWL.o main.o 

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

main: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)