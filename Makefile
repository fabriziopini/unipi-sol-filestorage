CC		=  gcc
CFLAGS  = -Wall -pedantic

server: server.o queue.o common_funcs.o storageQueue.o config.o
	$(CC) server.o queue.o common_funcs.o storageQueue.o config.o -o server

server.o: server.c queue.h common_def.h
	$(CC) $(CFLAGS) -c server.c

queue.o: queue.c queue.h util.h
	$(CC) $(CFLAGS) -c queue.c

storageQueue.o: storageQueue.c storageQueue.h common_def.h util.h queue.h config.h
	$(CC) $(CFLAGS) -c storageQueue.c

common_funcs.o: common_funcs.c common_funcs.h
	$(CC) $(CFLAGS) -c common_funcs.c

config.o: config.c config.h common_def.h
	$(CC) $(CFLAGS) -c config.c
