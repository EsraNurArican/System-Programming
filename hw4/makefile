CC = gcc
CFLAGS = -Wall -pedantic-errors -std=gnu99
LIBS = -pthread -lrt
OBJ = hw4.o 
 
%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) $(LIBS)

all: hw4

hw4: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)


clean:
	rm -f *.o