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

// nell'esercizio 8, testQueue.c le era scritto <queue.h>
#include "queue.h" // definisce il tipo Queue_t

#define UNIX_PATH_MAX 104 
#define SOCKNAME "./mysock"
#define N 100

// tipo di dato usato per passare gli argomenti al thread
typedef struct threadArgs
{
    int pfd;
    int thid;
    Queue_t *q;
} threadArgs_t;

// ritorna max fd attivo
int aggiorna(fd_set *set, int fd_num);


// funzione eseguita dal thread consumatore
void *Worker(void *arg) {
    Queue_t *q = ((threadArgs_t *)arg)->q;
    int myid = ((threadArgs_t *)arg)->thid;
    int pfd_w = ((threadArgs_t *)arg)->pfd; //fd lettura pipe

    int nread; // numero caratteri letti
    char buf[N]; // buffer messaggio


    size_t consumed = 0;
    while (1)
    {
        int *fd;
        fd = pop(q);
        if (*fd == -1)
        {
            free(fd);
            break;
        }
        ++consumed;

        printf("Worker %d: estratto <%d>\n", myid, *fd);
        fflush(stdout);
        
        nread = read(*fd,buf,N);
        if (nread==0) {/* EOF client finito */
            printf("Worker %d, chiudo:%d\n", myid, *fd);
            fflush(stdout);

            close(*fd);
        }
        else { /* nread !=0 */
            printf("Workers %d, Server got: %s, from %d\n", myid, buf, *fd);
            fflush(stdout);

            write(*fd, buf, strlen(buf));
            memset(buf, '\0', sizeof(buf));

            //scrivo nella pipe in modo che manager riaggiunga fd al set

            // TODO trasformare fd (intero) in stringa con terminatore (il terminatore c'è già con \0?? -> poi come lo leggo??)
            int n = 20; //TODO per ora a caso
            char text[n];
            sprintf(text, "%d", *fd);
            // char *text = "1 ";
            int k = write(pfd_w, text, strlen(text)+1); //TODO controllo
        }
        
        free(fd);
    }

    printf("Worker %d, consumed <%ld> messages, now it exits\n", myid, consumed);
    fflush(stdout);
    return NULL;
}

void usage(char *pname)
{
    fprintf(stderr, "\nusa: %s -c <num-workers>\n\n", pname);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    extern char *optarg;
    int w = 0, opt;
    int pfd[2];

    // parsing degli argomenti
    while ((opt = getopt(argc, argv, "w:")) != -1)
    {
        switch (opt)
        {
        case 'w':
            w = atoi(optarg);
            break;
        default:
            usage(argv[0]);
            break;
        }
    }
    if (w == 0)
        usage(argv[0]);
    printf("num workers =%d\n", w);

    pthread_t *th;
    threadArgs_t *thARGS;

    th = malloc(w * sizeof(pthread_t)); // w workers
    thARGS = malloc(w * sizeof(threadArgs_t));
    if (!th || !thARGS)
    {
        fprintf(stderr, "malloc fallita\n");
        exit(EXIT_FAILURE);
    }

    Queue_t *q = initQueue();
    if (!q)
    {
        fprintf(stderr, "initQueue fallita\n");
        exit(errno);
    }

    if (pipe(pfd) == -1) {
        // TODO controllare se setta errno
        fprintf(stderr, "pipe fallita\n");
        exit(errno);
    }

    for (int i = 0; i < w; ++i)
    { //passo ai consumatori thread_id, coda, descrittore in scrittura pipe
        thARGS[i].thid = i;
        thARGS[i].q = q;
        thARGS[i].pfd = pfd[1];
    }

    for (int i = 0; i < w; ++i)
    {
        if (pthread_create(&th[i], NULL, Worker, &thARGS[i]) != 0)
        {
            fprintf(stderr, "pthread_create failed (Workers)\n");
            exit(EXIT_FAILURE);
        }
    }

    int pfd_r = pfd[0]; //fd lettura pipe

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

                        int n = 100; // TODO per ora a caso
                        char buf[n];

                        int k = read(fd, buf, n); //TODO controllo
                        printf("byte letti:%d\n", k);

                        int i = 0;
                        char *rest;
                        while(i < k) {
                            rest = &(buf[i]);
                            //convertire token in un numero (che è il file descriptor)
                            int num = atoi(rest);
                            printf("fd:%d\n", num);
                            fflush(stdout);
                            FD_SET(num, &set);
                            if (num > fd_num) fd_num = num;

                            while(buf[i] != '\0') {
                                i++;
                            }
                            i++; // punto all'inizio della prossima stringa
                        }

                        // char* token;
                        // char* rest = buf;
  
                        // while ((token = strtok_r(rest, " ", &rest))) {
                        //     printf("%s\n", token);
                        //     fflush(stdout);
                        // // while (read(fd, buf, sizeof(buf)) > 0) {
                        // //     printf("-----> %s\n", buf);
                        // //     fflush(stdout);

                        
                    } else {/* sock I/0 pronto */
                        int *fd_queue = malloc(sizeof(int));

                        if (fd_queue == NULL) {
                            perror("Master fd_queue malloc fallita");
                            pthread_exit(NULL);
                        }

                        *fd_queue = fd;

                        // tolgo fd
                        FD_CLR(fd, &set);
                        fd_num = aggiorna(&set, fd_num);

                        if (push(q, fd_queue) == -1) {
                            fprintf(stderr, "Errore: push\n");

                            pthread_exit(NULL);
                        }

                        printf("Master pushed <%d>\n", *fd_queue);
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
    for (int i = 0; i < w; ++i)
    {
        int *eos = malloc(sizeof(int));
        *eos = -1;
        push(q, eos);
    }
    // aspetto la terminazione di tutti i consumatori
    for (int i = 0; i < w; ++i)
        pthread_join(th[i], NULL);

    // libero memoria
    deleteQueue(q);
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
