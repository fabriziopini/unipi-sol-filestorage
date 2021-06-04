CC		=  gcc
CFLAGS  = -Wall -pedantic

server: server.o queue.o common_funcs.o storageQueue.o config.o signal_handler.o intWithLock.o
	$(CC) server.o queue.o common_funcs.o storageQueue.o config.o signal_handler.o intWithLock.o -o server

server.o: server.c common_def.h
	$(CC) $(CFLAGS) -c server.c

queue.o: queue.c queue.h util.h
	$(CC) $(CFLAGS) -c queue.c

storageQueue.o: storageQueue.c storageQueue.h common_def.h util.h queue.h config.h common_funcs.h
	$(CC) $(CFLAGS) -c storageQueue.c

intWithLock.o: intWithLock.c intWithLock.h util.h
	$(CC) $(CFLAGS) -c intWithLock.c

common_funcs.o: common_funcs.c common_funcs.h
	$(CC) $(CFLAGS) -c common_funcs.c

config.o: config.c config.h common_def.h
	$(CC) $(CFLAGS) -c config.c

signal_handler.o: signal_handler.c signal_handler.h 
	$(CC) $(CFLAGS) -c signal_handler.c	
