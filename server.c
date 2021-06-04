#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h> /* ind AF_UNIX */
#include <ctype.h>
#include <fcntl.h>

// nell'esercizio 8, testQueue.c le era scritto <queue.h>
#include "queue.h" // definisce il tipo Queue_t
#include "storageQueue.h"
#include "common_def.h"
#include "common_funcs.h"
#include "config.h"
#include "signal_handler.h"
#include "intWithLock.h"

#define SOCKNAME "./mysock" 



// tipo di dato usato per passare gli argomenti ai thread worker
typedef struct threadArgs
{
    int pfd;
    int thid;
    Queue_t *q;
    IntWithLock_t *iwl;
} threadArgs_t;



//variabili globali :
struct config_struct config;

StorageQueue_t *storage_q;

server_status status = CLOSED;



char* myStrerror (int e) {
    if (e == -1) {
        return "Errore nella comunicazione con il client";
    } else if (e == 0) {
        return "Positivo";
    } else return strerror(e);
}

/** Gestisce la richiesta del client, interagendo con lo storage,
 *  ed invia il messaggio di risposta al client
 * 
 *   \param myid id del thread Worker //TODO aggiungere nelle stampe dell'esito il worker all'inizio?
 *   \param fd identificativo del client (socket)
 *   \param req messaggio di richiesta del client
 * 
 *   \retval -1 si sono verificati dei problemi sul socket fd ed è stato chiuso
 *   \retval 0 altrimenti
 * 
 */
int requestHandler (int myid, int fd, msg_request_t req) {
    msg_response_t res;
    res.result = 0;
    res.datalen = 0; //superfluo

    switch(req.op) {
        /** 
         * res.result ==
         *  • -1: si sono verificati dei problemi sul socket fd ed è stato chiuso
         *  • 0: (ho già comunicato al client fd)
         *  • >0: errore da comunicare
         */
        case OPENFILE: {
            if (req.flag == O_CREATE) {
                res.result = queue_s_push(storage_q, req.pathname, false, fd);
                printf(">Openfile_O_CREATE di %s, fd: %d, esito: %s\n", req.pathname, fd, myStrerror(res.result));
                if (res.result <= 0) return res.result;

            } else if (req.flag == O_CREATE_LOCK) {
                res.result = queue_s_push(storage_q, req.pathname, true, fd);
                printf(">Openfile_O_CREATE_LOCK di %s, fd: %d, esito: %s\n", req.pathname, fd, myStrerror(res.result));
                if (res.result <= 0) return res.result;

            } else if (req.flag == O_LOCK) {
                res.result = queue_s_updateOpeners(storage_q, req.pathname, true, fd);
                printf(">Openfile_O_LOCK di %s, fd: %d, esito: %s\n", req.pathname, fd, myStrerror(res.result));
                if (res.result <= 0) return res.result;

            } else if (req.flag == O_NULL) {
                res.result = queue_s_updateOpeners(storage_q, req.pathname, false, fd);
                printf(">Openfile_O_NULL di %s, fd: %d, esito: %s\n", req.pathname, fd, myStrerror(res.result));
                if (res.result <= 0) return res.result;

            } else {
                // flag non riconosciuto
                printf(">Openfile fd: %d, flag non riconosciuto\n", fd);
                res.result = EINVAL;
            }    
        }
        break;
        case READFILE: {
            res.result = queue_s_readFile(storage_q, req.pathname, fd);
            printf(">Readfile, fd: %d, esito: %s\n", fd, myStrerror(res.result));
            if (res.result <= 0) return res.result;
        }
        break;
        case READNFILES: {
            res.result = queue_s_readNFiles(storage_q, req.pathname, fd, req.datalen);
            printf(">ReadNfiles, fd: %d, esito: %s\n", fd, myStrerror(res.result));
            if (res.result <= 0) return res.result;
        }
        break;
        case WRITEFILE: {
            void *buf = malloc(req.datalen);
            if (buf == NULL) {
                fprintf(stderr, "fd: %d, malloc WriteToFile fallita\n", fd);
                fflush(stdout);
                res.result = ENOMEM;
            }

            int nread = readn(fd, buf, req.datalen);
            if (nread == 0) {/* EOF client finito */
                printf("⛔️ Worker %d, chiudo:%d\n", myid, fd);
                fflush(stdout);

                free(buf);

                printf("Close fd:%d files, %s", fd, myStrerror(queue_s_closeFdFiles(storage_q, fd)));
                close(fd);

                return -1;
            } else if (nread != req.datalen) {
            // TODO gestione errore (lettura parziale) (come sopra?)
                fprintf(stderr, "❌ ERRORE nread worker, lettura parziale\n");
                fflush(stderr);

                free(buf);
            } else {
                res.result = queue_s_writeFile(storage_q, req.pathname, fd, buf, req.datalen);
                free(buf);
                printf(">WriteToFile, fd: %d, esito: %s\n", fd, myStrerror(res.result));
                if (res.result <= 0) return res.result;

            }
        }
        break;
        case APPENDTOFILE: {
            void *buf = malloc(req.datalen);
            if (buf == NULL) {
                fprintf(stderr, "fd: %d, malloc appendToFile fallita\n", fd);
                fflush(stdout);
                res.result = ENOMEM;
            }

            int nread = readn(fd, buf, req.datalen);
            if (nread == 0) {/* EOF client finito */
                printf("⛔️ Worker %d, chiudo:%d\n", myid, fd);
                fflush(stdout);

                free(buf);

                printf("Close fd:%d files, esito:%s", fd, myStrerror(queue_s_closeFdFiles(storage_q, fd)));
                close(fd);

                return -1;
            } else if (nread != req.datalen) {
            // TODO gestione errore (lettura parziale) (come sopra?)
                fprintf(stderr, "❌ ERRORE nread worker, lettura parziale\n");
                fflush(stderr);

                free(buf);
            } else {
                res.result = queue_s_appendToFile(storage_q, req.pathname, fd, buf, req.datalen);
                free(buf);
                printf(">AppendToFile, fd: %d, esito: %s\n", fd, myStrerror(res.result));
                if (res.result <= 0) return res.result;
            }
        }
        break;
        case LOCKFILE: {
            res.result = queue_s_lockFile(storage_q, req.pathname, fd);
            printf(">LockFile, fd: %d, esito: %s\n", fd, myStrerror(res.result));
            if (res.result <= 0) return res.result;
        }
        break;
        case UNLOCKFILE: {
            res.result = queue_s_unlockFile(storage_q, req.pathname, fd);
            printf(">UnlockFile, fd: %d, esito: %s\n", fd, myStrerror(res.result));
            if (res.result <= 0) return res.result;
        }
        break;
        case CLOSEFILE: {
            res.result = queue_s_closeFile(storage_q, req.pathname, fd);
            printf(">CloseFile, fd: %d, esito: %s\n", fd, myStrerror(res.result));
            if (res.result <= 0) return res.result;
        }
        break;
        case REMOVEFILE: {
            res.result = queue_s_removeFile(storage_q, req.pathname, fd);
            printf(">RemoveFile, fd: %d, esito: %s\n", fd, myStrerror(res.result));
            if (res.result <= 0) return res.result;
        }
        break;
        default: { // non dovrebbe succedere (api scritta male)
            res.result = EPERM;
            printf(">Operazione richiesta da fd: %d non riconosciuta", fd);
        }
    }

    //invio messaggio al client (finisco qui nel caso in caso di errore (e fd ancora aperto))
    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {
        close(fd); //TODO gestire nel client
        printf("writen res requestHandler errore\n");
        fflush(stdout);
        return -1;
    }

    fflush(stdout);
    fflush(stderr);

    return 0;
}



// ritorna max fd attivo
int aggiorna(fd_set *set, int fd_num);


// funzione eseguita dal thread worker
void *Worker(void *arg) {
    Queue_t *q = ((threadArgs_t *)arg)->q;
    IntWithLock_t *iwl = ((threadArgs_t *)arg)->iwl;
    int myid = ((threadArgs_t *)arg)->thid;
    int pfd_w = ((threadArgs_t *)arg)->pfd; //fd lettura pipe

    int nread; // numero caratteri letti
    msg_request_t req; // messaggio richiesta dal client

    size_t consumed = 0;
    while (1) {
        int fd;
        fd = queue_pop(q);
        if (fd == -1) {
            printf("Worker %d, consumed <%ld> messages, now it exits\n", myid, consumed);
            fflush(stdout);
            return NULL;
        }
        ++consumed;

        printf("⛏ Worker %d: estratto <%d>\n", myid, fd);
        fflush(stdout);
        
        nread = readn(fd, &req, sizeof(msg_request_t));
        if (nread == 0) {/* EOF client finito */
            printf("⛔️ Worker %d, chiudo:%d\n", myid, fd);
            fflush(stdout);
            
            //TODO aggiungere nelle stampe dell'esito il worker all'inizio?
            printf("Close fd: %d, esito: %s\n", fd, myStrerror(queue_s_closeFdFiles(storage_q, fd)));

            close(fd);
            fd = -fd;

            deleteClient(iwl);

        } else if (nread != sizeof(msg_request_t)) {
            // TODO gestione errore (lettura parziale) faccio come sopra?
            printf("❌ ERRORE nread worker, lettura parziale\n");
            fflush(stdout);
        } else { /* nread == sizeof(msg_request_t) */

            printf("Worker %d, Server got: %d, from %d\n", myid, req.op, fd);
            fflush(stdout);

            if (requestHandler(myid, fd, req) != 0) { // client disconnesso
                fd = -fd; // in modo che nel main non si riaggiunga nel set
                deleteClient(iwl);
            } 
        }

        if (writen(pfd_w, &fd, sizeof(fd)) != sizeof(fd)) {
            //TODO gestione errore
            printf("❌ ERRORE writen worker, parziale\n");
            fflush(stdout);
        } else {
            printf("📬 fd %d messo nella pipe\n", fd);
            fflush(stdout);
        }

        memset(&req, '\0', sizeof(req));     
    }

    //TODO close pfd_w? (direi di NO! che chiudo a fa? tanto thread finisce)

    printf("Worker %d, ha consumato <%ld> messaggi, adesso esce\n", myid, consumed);
    fflush(stdout);
    return NULL;
}


int main(int argc, char *argv[])
{
    int pfd[2]; //pipe per comunicazone tra workers e main
    int spfd[2]; //signal pipe

    //TODO read config
    config.num_workers = 3;
    strncpy(config.sockname, "./mysock", UNIX_PATH_MAX);
    config.limit_num_files = 3;
    config.storage_capacity = 100;


    // Creo ed inizializzo la coda utilizzata dal main per comunicare i fd agli Workers
    Queue_t *q = queue_init();
    if (!q)
    {
        fprintf(stderr, "initQueue fallita\n");
        exit(errno);
    }

    // Creo ed inizializzo la coda dello storage
    storage_q = queue_s_init(config.limit_num_files, config.storage_capacity);
    if (!storage_q)
    {
        fprintf(stderr, "initQueue storage fallita\n");
        exit(errno);
    }

    // Creo ed inizializzo l'intero con lock per contare il numero di client
    IntWithLock_t *iwl = initIntWithLock();
    if (!iwl)
    {
        fprintf(stderr, "initIntWithLock fallita\n");
        exit(errno);
    }

    // Creo pipe per comunicazione tra Workers e main
    if (pipe(pfd) == -1) {
        // TODO controllare se setta errno
        fprintf(stderr, "pipe fallita\n");
        exit(errno);
    }

    // Creo pipe per comunicazione (dei segnali) tra il thread signal_handler e main
    if (pipe(spfd) == -1) {
        // TODO controllare se setta errno
        fprintf(stderr, "pipe fallita\n");
        exit(errno);
    }


    status = RUNNING; // inizalizzo lo stato del server
    // Inizializzo gli argomenti del thread che gestisce i segnali
    signalThreadArgs_t signalArg;
    signalArg.pfd_w = spfd[1];
    signalArg.status = &status;

    // Chiamo il metodo che fa partire il thread che gestisce i segnali
    if(createSignalHandlerThread(&signalArg) == -1) {
        return -1;
    }


    // Creo e faccio partire i thread Workers (con gli opportuni argomenti)
    pthread_t *th = malloc(config.num_workers * sizeof(pthread_t));
    threadArgs_t *thARGS = malloc(config.num_workers * sizeof(threadArgs_t));

    if (!th || !thARGS)
    {
        fprintf(stderr, "malloc fallita\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < config.num_workers; ++i)
    { //passo ai consumatori thread_id, coda, descrittore in scrittura pipe
        thARGS[i].thid = i;
        thARGS[i].q = q;
        thARGS[i].iwl = iwl;
        thARGS[i].pfd = pfd[1];
    }

    for (int i = 0; i < config.num_workers; ++i)
    {
        if (pthread_create(&th[i], NULL, Worker, &thARGS[i]) != 0)
        {
            fprintf(stderr, "pthread_create failed (Workers)\n");
            exit(EXIT_FAILURE);
        }
    }


    int pfd_r = pfd[0]; //fd lettura pipe
    fcntl(pfd_r, F_SETFL, O_NONBLOCK);

    int fd_sk; // socket di connessione
    int fd_c; // socket di I/O con un client
    int fd_num = 0; // max fd attivo
    int fd; // indice per verificare risultati select
    fd_set set; // l’insieme dei file descriptor attivi
    fd_set rdset; // insieme fd attesi in lettura
    struct sockaddr_un sa;
    strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;

    unlink(SOCKNAME);
    fd_sk = socket(AF_UNIX,SOCK_STREAM,0);
    if(fd_sk < 0) {
        perror("error opening socket");
        return (void*)-1;
    }
    if(bind(fd_sk,(struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("error on binding");
        return (void*)-1;
    }
    listen(fd_sk, SOMAXCONN);

    // mantengo il massimo indice di descrittore attivo in fd_num
    if (fd_sk > fd_num) fd_num = fd_sk;
    if (pfd_r > fd_num) fd_num = pfd_r;
    if (spfd[0] > fd_num) fd_num = spfd[0];

    FD_ZERO(&set);

    FD_SET(fd_sk, &set);
    FD_SET(pfd_r, &set);
    FD_SET(spfd[0], &set);

    int temp = -1;

    while (((temp = checkTotalClients(iwl)) != 0 && status == CLOSING) || status == RUNNING) {
        printf("Inizio while, totalCLients = %d\n", temp);

        rdset = set; /* preparo maschera per select */

        if (select(fd_num+1, &rdset, NULL, NULL, NULL) == -1) {
            fprintf(stderr, "select fallita\n");
            exit(errno);
        }
        else { /* select OK */
            printf("select ok\n");
            fflush(stdout);

            for (fd = 0; fd <= fd_num; fd++) {
                if (FD_ISSET(fd, &rdset)) { 

                    if (fd == fd_sk) {/* sock connect pronto */
                    
                        fd_c = accept(fd,NULL,0);
                        if (fd_c == -1) {
                            exit(errno);                        
                        }

                        printf("Appena fatta accept, nuovo fd:%d\n", fd_c);
                        fflush(stdout);

                        FD_SET(fd_c, &set);
                        addClient(iwl);

                        if (fd_c > fd_num) fd_num = fd_c;
                    } else if (fd == pfd_r) {
                        //aggiungo in set gli fd (se > 0) che leggo dalla pipe (con workers)

                        int num; 
                        int nread;

                        while ((nread = readn(fd, &num, sizeof(num))) == sizeof(num)) {
                            printf("Letto fd:%d dalla pipe\n", num);
                            fflush(stdout);
                            if (num > 0) {
                                FD_SET(num, &set);
                                if (num > fd_num) fd_num = num;
                            }                 
                        }
                        if (nread != 0) {
                            //TODO gestione errore: c'era qualcosa di parziale
                        }

                    } else if (fd == spfd[0]) { //pipe con il signal_handler
                        FD_CLR(spfd[0], &set); //tolgo pipe con signal_handler dal set
                        fd_num = aggiorna(&set, fd_num);
                        close(spfd[0]);

                        if (status == CLOSING) {                    
                            printf("Non accetto nuove connessioni\n");
                            FD_CLR(fd_sk, &set); //tolgo socket di connessione dal set
                            fd_num = aggiorna(&set, fd_num);
                            close(fd_sk);          
                       
                        }
                        else if (status == CLOSED) {
                            printf("Non accetto nuove richieste\n");
                            // (finisco il ciclo for in cui sono, quando torno al while esco)
                            
                        } else {
                            fprintf(stderr, "Stato server non consistente\n");
                            status = CLOSED;
                            //come quello sopra (finisci di servire le richieste e chiudi)
                        }
                    } else {/* sock I/0 pronto */

                        if (queue_push(q, fd) == -1) {
                            fprintf(stderr, "Errore: push\n");

                            pthread_exit(NULL); //TODO chiudere server in modo più elegante (rilasciare un po' di risorse)
                        }

                        // tolgo fd dal set
                        FD_CLR(fd, &set);
                        fd_num = aggiorna(&set, fd_num);

                        printf("Master pushed <%d>\n", fd);
                        fflush(stdout);
                    }
                }
            }
        }
    }

    printf("SERVER IN FASE DI CHIUSURA (uscito dal while)\n");

    if (CLOSED) close(fd_sk);
    unlink(SOCKNAME);
    close(pfd_r);
    close(spfd[0]);

    /* Protocollo di terminazione:
     * Si inviano 'c' valori speciali (-1)
     * quindi si aspettano gli worker
     */
    // quindi termino tutti i worker
    for (int i = 0; i < config.num_workers; ++i)
    {
        int eos = -1;
        if (queue_push(q, eos) == -1) fprintf(stderr, "Errore: push\n");
    }

    // aspetto la terminazione di tutti worker
    for (int i = 0; i < config.num_workers; ++i)
        pthread_join(th[i], NULL);

    // stampa del sunto delle operazioni effettuate
    printf("\n-------- Sunto operazioni effettuate --------\n");
    printf("Numero di file massimo memorizzato nel server: %d\n", storage_q->max_num_files);
    printf("Dimensione massima in Mbytes raggiunta: %lu\n", storage_q->max_used_storage);
    printf("Algoritmo della cache eseguito %d volte\n", storage_q->replace_occur);
    printf("Lista dei file contenuti nello storage al momento della chiusura:\n");
    queue_s_printListFiles(storage_q);
    printf("----------------------------------------------\n\n");



    // libero memoria
    queue_delete(q);
    queue_s_delete(storage_q);
    deleteIntWithLock(iwl);
    free(th);
    free(thARGS);
    return 0;
}


// ritorna max fd attivo
// TODO DA CAMBIARE, SCRITTA DA ME COSI... guardare soluzioni esercizio
int aggiorna(fd_set *set, int fd_num) {
    int max_fd = 0;
    for(int i=0; i <= fd_num; i++) {
        if (FD_ISSET(i, set)) {
            max_fd = i;
        }
    }

    return max_fd;
}
