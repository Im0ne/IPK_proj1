CC=gcc
CFLAGS=-I.
DEPS = main.h tcp.h udp.h
OBJ = main.o tcp.o udp.o 

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

ipk24: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
