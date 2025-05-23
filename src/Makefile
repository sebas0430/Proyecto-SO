
CC = gcc
CFLAGS = -Wall -g

all: solicitud receptor

solicitud: solicitud.c
	$(CC) $(CFLAGS) -o solicitud solicitud.c

receptor: receptor.c
	$(CC) $(CFLAGS) -o receptor receptor.c -lpthread

clean:
	rm -f solicitud receptor *.o
