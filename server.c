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
//#include <message.h> // TODO: devo mettere tutto in common_def?
#include "common_def.h"
#include "common_funcs.h"
#include "config.h"

#define SOCKNAME "./mysock" 



// tipo di dato usato per passare gli argomenti al thread
typedef struct threadArgs
{
    int pfd;
    int thid;
    Queue_t *q;
} threadArgs_t;



//variabili globali :
struct config_struct config;

StorageQueue_t *storage_q;



char* myStrerror (int e) {
    if (e == -1) {
        return "Errore nella comunicazione con il client";
    } else if (e == 0) {
        return "Positivo";
    } else return strerror(e);
}

int requestHandler (int myid, int fd, msg_request_t req) {
    msg_response_t res;
    res.result = 0;
    res.datalen = 0; //superfluo

    switch(req.op) {
        /** 
         * res.result ==
         *  • -1: non devo riaggiungere fd pipe set server
         *  • 0: (ho già comunicato al client fd)
         *  • >0: errore da comunicare
         */
        case OPENFILE: {
            if (req.flag == O_CREATE) {
                res.result = queue_s_push(storage_q, req.pathname, false, fd);
                printf("Openfile_O_CREATE di %s, fd: %d, esito: %s\n", req.pathname, fd, myStrerror(res.result));
                if (res.result <= 0) return res.result;

            } else if (req.flag == O_CREATE_LOCK) {
                res.result = queue_s_push(storage_q, req.pathname, true, fd);
                printf("Openfile_O_CREATE_LOCK di %s, fd: %d, esito: %s\n", req.pathname, fd, myStrerror(res.result));
                if (res.result <= 0) return res.result;

            } else if (req.flag == O_LOCK) {
                res.result = queue_s_updateOpeners(storage_q, req.pathname, true, fd);
                printf("Openfile_O_LOCK di %s, fd: %d, esito: %s\n", req.pathname, fd, myStrerror(res.result));
                if (res.result <= 0) return res.result;

            } else if (req.flag == O_NULL) {
                res.result = queue_s_updateOpeners(storage_q, req.pathname, false, fd);
                printf("Openfile_O_NULL di %s, fd: %d, esito: %s\n", req.pathname, fd, myStrerror(res.result));
                if (res.result <= 0) return res.result;

            } else {
                // flag non riconosciuto
                printf("Openfile fd: %d, flag non riconosciuto\n", fd);
                res.result = EINVAL;
            }    
        }
        break;
        case READFILE: {
            res.result = queue_s_readFile(storage_q, req.pathname, fd);
            printf("Readfile, fd: %d, esito: %s\n", fd, myStrerror(res.result));
            if (res.result <= 0) return res.result;
        }
        break;
        case READNFILES: { //TODO
            res.result = queue_s_readNFiles(storage_q, req.pathname, fd, req.datalen);
            printf("ReadNfiles, fd: %d, esito: %s\n", fd, myStrerror(res.result));
            if (res.result <= 0) return res.result;
        }
        break;
        case WRITEFILE: {
            void *buf = malloc(req.datalen); //TODO controllo malloc
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
            // TODO gestione errore (lettura parziale)
                fprintf(stderr, "❌ ERRORE nread worker, lettura parziale\n");
                fflush(stderr);

                free(buf);
            } else {
                res.result = queue_s_writeFile(storage_q, req.pathname, fd, buf, req.datalen);
                free(buf);
                printf("WriteToFile, fd: %d, esito: %s\n", fd, myStrerror(res.result));
                if (res.result <= 0) return res.result;

            }
        }
        break;
        case APPENDTOFILE: {
            void *buf = malloc(req.datalen); //TODO controllo malloc
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
            // TODO gestione errore (lettura parziale)
                fprintf(stderr, "❌ ERRORE nread worker, lettura parziale\n");
                fflush(stderr);

                free(buf);
            } else {
                res.result = queue_s_appendToFile(storage_q, req.pathname, fd, buf, req.datalen);
                free(buf);
                printf("AppendToFile, fd: %d, esito: %s\n", fd, myStrerror(res.result));
                if (res.result <= 0) return res.result;
            }
        }
        break;
        case LOCKFILE: {
            res.result = queue_s_lockFile(storage_q, req.pathname, fd);
            printf("LockFile, fd: %d, esito: %s\n", fd, myStrerror(res.result));
            if (res.result <= 0) return res.result;
        }
        break;
        case UNLOCKFILE: {
            res.result = queue_s_unlockFile(storage_q, req.pathname, fd);
            printf("UnlockFile, fd: %d, esito: %s\n", fd, myStrerror(res.result));
            if (res.result <= 0) return res.result;
        }
        break;
        case CLOSEFILE: {
            res.result = queue_s_closeFile(storage_q, req.pathname, fd);
            printf("CloseFile, fd: %d, esito: %s\n", fd, myStrerror(res.result));
            if (res.result <= 0) return res.result;
        }
        break;
        case REMOVEFILE: {
            res.result = queue_s_removeFile(storage_q, req.pathname, fd);
            printf("RemoveFile, fd: %d, esito: %s\n", fd, myStrerror(res.result));
            if (res.result <= 0) return res.result;
        }
        break;
        default: { // non dovrebbe succedere (api scritta male)
            res.result = EPERM;
            printf("Operazione richiesta da fd: %d non riconosciuta", fd);
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
    int myid = ((threadArgs_t *)arg)->thid;
    int pfd_w = ((threadArgs_t *)arg)->pfd; //fd lettura pipe

    int nread; // numero caratteri letti
    msg_request_t req; // messaggio richiesta dal client

    size_t consumed = 0;
    while (1)
    {
        int fd;
        fd = queue_pop(q);
        if (fd == -1)
        {
            //TODO
            break;
        }
        ++consumed;

        printf("⛏ Worker %d: estratto <%d>\n", myid, fd);
        fflush(stdout);
        
        nread = readn(fd, &req, sizeof(msg_request_t));
        if (nread == 0) {/* EOF client finito */
            printf("⛔️ Worker %d, chiudo:%d\n", myid, fd);
            fflush(stdout);

            printf("Close fd: %d, esito: %s\n", fd, myStrerror(queue_s_closeFdFiles(storage_q, fd)));

            close(fd);
        } else if (nread != sizeof(msg_request_t)) {
            // TODO gestione errore (lettura parziale)
            printf("❌ ERRORE nread worker, lettura parziale\n");
            fflush(stdout);
        } else { /* nread == sizeof(msg_request_t) */

            printf("Workers %d, Server got: %d, from %d\n", myid, req.op, fd);
            fflush(stdout);

            if (requestHandler(myid, fd, req) != 0) {
                //TODO
            } else {
                //scrivo nella pipe in modo che manager riaggiunga fd al set

                if (writen(pfd_w, &fd, sizeof(fd)) != sizeof(fd)) {
                    //TODO gestione errore
                    printf("❌ ERRORE writen worker, parziale\n");
                    fflush(stdout);
                } else {
                    printf("📬 fd %d messo nella pipe\n", fd);
                    fflush(stdout);
                }
            }

            memset(&req, '\0', sizeof(req));
        }     
    }

    printf("Worker %d, consumed <%ld> messages, now it exits\n", myid, consumed);
    fflush(stdout);
    return NULL;
}


int main(int argc, char *argv[])
{
    int pfd[2];

    //TODO read config
    config.num_workers = 3;
    strncpy(config.sockname, "./mysock", UNIX_PATH_MAX);
    config.limit_num_files = 3;
    config.storage_capacity = 100;


    pthread_t *th;
    threadArgs_t *thARGS;

    th = malloc(config.num_workers * sizeof(pthread_t)); // w workers
    thARGS = malloc(config.num_workers * sizeof(threadArgs_t));
    if (!th || !thARGS)
    {
        fprintf(stderr, "malloc fallita\n");
        exit(EXIT_FAILURE);
    }

    Queue_t *q = queue_init();
    if (!q)
    {
        fprintf(stderr, "initQueue fallita\n");
        exit(errno);
    }

    storage_q = queue_s_init(config.limit_num_files, config.storage_capacity);
    if (!storage_q)
    {
        fprintf(stderr, "initQueue storage fallita\n");
        exit(errno);
    }

    if (pipe(pfd) == -1) {
        // TODO controllare se setta errno
        fprintf(stderr, "pipe fallita\n");
        exit(errno);
    }

    for (int i = 0; i < config.num_workers; ++i)
    { //passo ai consumatori thread_id, coda, descrittore in scrittura pipe
        thARGS[i].thid = i;
        thARGS[i].q = q;
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

    FD_ZERO(&set);
    FD_SET(fd_sk, &set);
    FD_SET(pfd_r, &set);

    while (1) {
        printf("Inizio while\n");

        rdset = set; /* preparo maschera per select */

        if (select(fd_num+1, &rdset, NULL, NULL, NULL) == -1) {
            fprintf(stderr, "pipe fallita\n");
            exit(errno);
        }
        else { /* select OK */
            printf("select ok\n");
            fflush(stdout);

            for (fd = 0; fd <= fd_num; fd++) {
                if (FD_ISSET(fd, &rdset)) { 
                    if (fd == fd_sk) {/* sock connect pronto */
                        fflush(stdout);

                        fd_c = accept(fd,NULL,0);

                        printf("Appena fatta accept, nuovo fd:%d\n", fd_c);
                        fflush(stdout);

                        FD_SET(fd_c, &set);

                        if (fd_c > fd_num) fd_num = fd_c;
                    } else if (fd == pfd_r) {
                        //aggiungo in set gli fd che leggo dalla pipe
                        printf("leggo dalla pipe\n");
                        fflush(stdout);

                        int num; 
                        int nread;

                        while ((nread = readn(fd, &num, sizeof(num))) == sizeof(num)) {
                            printf("Letto fd:%d dalla pipe\n", num);
                            fflush(stdout);
                            FD_SET(num, &set);
                            if (num > fd_num) fd_num = num;
                        }
                        if (nread != 0) {
                            //TODO gestione errore: c'era qualcosa di parziale
                        }
                        
                        printf("Non c'è più niente nella pipe\n");
                        fflush(stdout);


                    } else {/* sock I/0 pronto */
                        // int *fd_queue = malloc(sizeof(int));

                        // if (fd_queue == NULL) {
                        //     perror("Master fd_queue malloc fallita");
                        //     pthread_exit(NULL);
                        // }

                        // *fd_queue = fd;

                        if (queue_push(q, fd) == -1) {
                            fprintf(stderr, "Errore: push\n");

                            pthread_exit(NULL);
                        }

                        // tolgo fd
                        FD_CLR(fd, &set);
                        fd_num = aggiorna(&set, fd_num);

                        printf("Master pushed <%d>\n", fd);
                        fflush(stdout);
                    }
                }
            }
        }
    }
    close(fd_sk);
    close(pfd_r);

    /* possibile protocollo di terminazione:
     * quindi si inviano 'c' valori speciali (-1)
     * quindi si aspettano gli worker
     */
    // quindi termino tutti i worker
    for (int i = 0; i < config.num_workers; ++i)
    {
        int eos = -1;
        queue_push(q, eos);
    }
    // aspetto la terminazione di tutti i consumatori
    for (int i = 0; i < config.num_workers; ++i)
        pthread_join(th[i], NULL);

    // libero memoria
    queue_delete(q);
    free(th);
    free(thARGS);
    return 0;
}


// ritorna max fd attivo
// DA CAMBIARE, SCRITTA DA ME COSI... guardare soluzioni esercizio
int aggiorna(fd_set *set, int fd_num) {
    int max_fd = 0;
    for(int i=0; i <= fd_num; i++) {
        if (FD_ISSET(i, set)) {
            max_fd = i;
        }
    }

    return max_fd;
}
