CC = gcc
CFLAGS = -Wall -pedantic-errors -std=gnu99
LIBS = -pthread -lrt
OBJ_UNNAMED = hw3unnamed.o 
OBJ_NAMED = hw3named.o 

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) $(LIBS)

all: hw3unnamed hw3named

hw3unnamed: $(OBJ_UNNAMED)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

hw3named: $(OBJ_NAMED)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)


clean:
	rm -f *.o