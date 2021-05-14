CC		=  gcc
CFLAGS  = -Wall -pedantic

server: server.o queue.o common_funcs.o
	$(CC) server.o queue.o common_funcs.o -o server

server.o: server.c queue.h common_def.h
	$(CC) $(CFLAGS) -c server.c

queue.o: queue.c queue.h util.h
	$(CC) $(CFLAGS) -c queue.c

common_funcs.o: common_funcs.c common_funcs.h
	$(CC) $(CFLAGS) -c common_funcs.c
