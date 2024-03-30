CC=gcc
CFLAGS=-Wall -Wextra -pedantic
DEPS = main.h tcp.h udp.h help.h
OBJ = main.o tcp.o udp.o help.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

ipk24chat-client: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f *.o ipk24chat-client