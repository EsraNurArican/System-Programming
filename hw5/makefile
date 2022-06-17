CC = gcc
CFLAGS = -Wall -pedantic-errors -std=gnu99
LIBS = -pthread -lrt -lm
OBJ = hw5.o 
 
%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) $(LIBS)

all: hw5

hw5: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)


clean:
	rm -f *.o