#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>  //api posix   
#include <sys/wait.h>  
#include <sys/types.h> 
#include <sys/socket.h> 
#include <sys/stat.h>
#include <sys/un.h> //dominio AF_UNIX
#include <string.h>
#include <pthread.h> 
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include "signal_handler.h"

// Thread che gestisce i segnali (cambia lo stato del server, inoltre tramite pipe (chiudendola) sveglia il server nel caso fosse bloccato su select)
void *SignalHandler(void *arg) {
    signalThreadArgs_t *puntatore = (signalThreadArgs_t*) arg;

    fprintf (stdout," #%d: ## signal thread partito ##\n",getpid());

    server_status * status = puntatore->status;
    int pfd_w = puntatore->pfd_w;

    /* costruisco la maschera */
    sigset_t set;
    int sig;
    sigaddset(&set, SIGINT);
	sigaddset(&set, SIGQUIT);
	sigaddset(&set, SIGHUP);

    /* blocco */
    sigprocmask(SIG_BLOCK, &set, NULL);
  
    while(*status == RUNNING) {
        sigwait(&set, &sig);

        switch (sig) {
            case SIGINT:
                printf("SignalHandler: Ricevuto SigInt\n");
                fflush(stdout);
                *status = CLOSED;
                break;
            case SIGQUIT:
                printf("SignalHandler: Ricevuto SigQuit\n");
                *status = CLOSED;
                break;
            case SIGHUP:
                printf("SignalHandler: Ricevuto SigHup\n");
                *status = CLOSING;
                break;
            default:
                break;
        }        
    }

    close(pfd_w);

    return NULL;
}


pthread_t createSignalHandlerThread(signalThreadArgs_t *signalArg) {
    sigset_t set;
	sigfillset(&set);	
    /* blocco tutti i segnali prima dell'avvio del signal handler  */
    
    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1)
        perror("Err: signal_set");


    /* signal handler thread */
    pthread_t tid;
    if(pthread_create(&tid, NULL, SignalHandler, signalArg) != 0) {
        fprintf(stderr, "pthread_create failed (SignalHandler)\n");
        return NULL;
    }

    pthread_detach(tid); 

    return tid;
}
