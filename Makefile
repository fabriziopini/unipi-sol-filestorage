#
# File Storage Server Progetto del corso di LSO 2021 
# 
# Dipartimento di Informatica Università di Pisa
# Studente: Fabrizio Pini 530755
#
#

##########################################################
# IMPORTANTE: completare la lista dei file da consegnare
# 
FILE_DA_CONSEGNARE=Makefile *.c *.h Config Config/* Espulsi Files Files/* Letti LettiFinal OtherFiles OtherFiles/* tests tests/*\
		   Relazione_Pini530755.pdf

FILE_DA_ESCLUDERE=.DS_Store

TARNAME=Fabrizio_Pini

CORSO=CorsoA
#
###########################################################

CC		=  gcc
CFLAGS  = -w
#CFLAGS  = -Wall -pedantic

#default
all: server client


clean: 
	rm -f server client *.o *.a Letti/* Espulsi/* LettiFinal/*

##########
# SERVER #
##########
server: server.o queue.o common_funcs.o storageQueue.o config.o signal_handler.o intWithLock.o
	$(CC) -pthread server.o queue.o common_funcs.o storageQueue.o config.o signal_handler.o intWithLock.o -o server

server.o: server.c common_def.h
	$(CC) $(CFLAGS) -c server.c

queue.o: queue.c queue.h util.h config.h
	$(CC) $(CFLAGS) -c queue.c

storageQueue.o: storageQueue.c storageQueue.h common_def.h util.h queue.h config.h common_funcs.h
	$(CC) $(CFLAGS) -c storageQueue.c

intWithLock.o: intWithLock.c intWithLock.h util.h
	$(CC) $(CFLAGS) -c intWithLock.c

common_funcs.o: common_funcs.c common_funcs.h
	$(CC) $(CFLAGS) -c common_funcs.c

config.o: config.c config.h common_def.h
	$(CC) $(CFLAGS) -c config.c

signal_handler.o: signal_handler.c signal_handler.h config.h
	$(CC) $(CFLAGS) -c signal_handler.c	

# end SERVER 

##########
# CLIENT #
##########
client: client.o common_funcs.o opt_queue.o libapi.a
	$(CC) client.o common_funcs.o opt_queue.o -o client -L . -lapi 

client.o: client.c common_def.h client_def.h util.h
	$(CC) $(CFLAGS) -c client.c

libapi.a: api.o
	ar rvs libapi.a api.o

api.o: api.c api.h common_def.h
	$(CC) $(CFLAGS) -c api.c

opt_queue.o: opt_queue.c opt_queue.h common_def.h util.h client_def.h
	$(CC) $(CFLAGS) -c opt_queue.c

# commentato perchè fatto anche dal server
#common_funcs.o: common_funcs.c common_funcs.h
#	$(CC) $(CFLAGS) -c common_funcs.c

# end CLIENT


##########
# TEST 1 #
##########
test1: 
	valgrind --leak-check=full ./server -c ./Config/test1.conf & echo $$! > server.PID \
	& bash tests/test1.sh


##########
# TEST 2 #
##########
test2:
	./server -c ./Config/test2.conf & echo $$! > server.PID \
	& bash tests/test2.sh


############
# consegna #
############
consegna:
	tar cvzf $(TARNAME)-$(CORSO).tar.gz --exclude="$(FILE_DA_ESCLUDERE)" $(FILE_DA_CONSEGNARE)
	@echo "TAR $(TARNAME)-$(CORSO).tar.gz PRONTO"