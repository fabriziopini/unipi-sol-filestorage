#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>

// TODO negli esercizi erano scritte < > , non " "
#include "util.h"
#include "storageQueue.h" 
#include "common_def.h"
#include "common_funcs.h"
#include "queue.h"
#include "config.h"


/**
 * @file storageQueue.c
 * @brief File di implementazione dell'interfaccia storageQueue.h
 */



/* ------------------- funzioni di utilità -------------------- */

// qui assumiamo (per semplicità) che le mutex non siano mai di
// tipo 'robust mutex' (pthread_mutex_lock(3)) per cui possono
// di fatto ritornare solo EINVAL se la mutex non e' stata inizializzata.

static inline StorageNode_t  *allocStorageNode()           { return malloc(sizeof(StorageNode_t));  }
static inline StorageQueue_t *allocQueue()                 { return malloc(sizeof(StorageQueue_t)); }
static inline void freeStorageNode(StorageNode_t *node)    { queue_delete(node->opener_q); free((void*)node); }
static inline void LockQueue(StorageQueue_t *q)            { LOCK(&q->qlock);   }
static inline void UnlockQueue(StorageQueue_t *q)          { UNLOCK(&q->qlock); }
static inline void UnlockQueueAndWait(StorageQueue_t *q, StorageNode_t *node)   { WAIT(&node->filecond, &q->qlock); }
static inline void UnlockQueueAndSignal(StorageQueue_t *q, StorageNode_t *node) { SIGNAL(&node->filecond); UNLOCK(&q->qlock); }

/** Restituisce il nodo identificato da pathname
 *  (da chiamare con lock)
 * 
 *   \param q puntatore alla coda
 *   \param pathname del file
 *  
 *   \retval puntatore al nodo
 *   \retval NULL se non lo trova
 */
StorageNode_t* queue_s_find(StorageQueue_t *q, char *pathname);

/** Estrae dalla coda q un numero di files adeguato per permettere l'inserimento di un file di buf_len bytes,
 *  non superando la capacità dello storage in bytes
 *  (da chiamare con lock (su q))
 * 
 *   \param q puntatore alla coda
 *   \param buf_len lunghezza del file da aggiungere
 *   \param num_poppedFiles numero di files estratti dalla coda
 *   \param poppedHead puntatore alla testa della coda estratta
 *  
 *   \retval 0 se successo
 *   \retval errno se errore
 */
int queue_s_popUntil(StorageQueue_t *q, int buf_len, int *num_poppedFiles, StorageNode_t **poppedHead);

/** Estrae dalla coda q un nodo (file)
 *  (da chiamare con lock)
 * 
 *   \param q puntatore alla coda
 *  
 *   \retval 0 se successo
 *   \retval errno se errore
 */
StorageNode_t *queue_s_pop(StorageQueue_t *q);

/** Comunica al client sul socket fd, i files della coda poppedH, e libera memoria
 * 
 *   \param fd file descriptor su cui comunicare
 *   \param num_files numero di files da inviare (tutta la coda poppedH)
 *   \param poppedH puntatore alla coda
 *  
 *   \retval 0 se successo
 *   \retval errno se errore
 */
int send_to_client_and_free(int fd, int num_files, StorageNode_t *poppedH);


   


/* ------------------- interfaccia della coda ------------------ */

StorageQueue_t *queue_s_init(int limit_num_files, unsigned long storage_capacity) {
    StorageQueue_t *q = allocQueue();

    if (!q) return NULL;
    
    q->head = NULL;
    q->tail = NULL;    
    q->cur_numfiles = 0;
    q->cur_usedstorage = 0;
    q->max_num_files = 0;
    q->max_used_storage = 0;
    q->replace_occur = 0;
    q->limit_num_files = limit_num_files;
    q->storage_capacity = storage_capacity;


    if (pthread_mutex_init(&q->qlock, NULL) != 0) {
	    perror("mutex init");
	    return NULL;
    }

    return q;
}

// da chiamare quando server "muore"
void queue_s_delete(StorageQueue_t *q) {
    if (q == NULL) return;

    while(q->head != NULL) {
        StorageNode_t *p = (StorageNode_t*)q->head;
        q->head = q->head->next;

        if (p->len > 0) free(p->data);
        if (&p->filecond)  pthread_cond_destroy(&p->filecond);
        freeStorageNode(p);
    }

    if (&q->qlock)  pthread_mutex_destroy(&q->qlock);
    
    
    free(q);
}

void queue_s_deleteNodes (StorageNode_t *head) {
    while(head != NULL) {
        StorageNode_t *p = (StorageNode_t*) head;
        head = head->next;

        if (p->len > 0) free(p->data);
        if (&p->filecond)  pthread_cond_destroy(&p->filecond);
        freeStorageNode(head);
    }

}

int queue_s_push(StorageQueue_t *q, char *pathname, bool locked, int fd) {

    StorageNode_t *poppedNode = NULL;

    if ((q == NULL) || (pathname == NULL)) { 
        return EINVAL;
    }

    printf("queue_s_push, fd: %d, pathname: %s\n", fd, pathname);
    fflush(stdout);

    StorageNode_t *n = allocStorageNode();

    if (!n) return ENOMEM;

    if (pthread_cond_init(&n->filecond, NULL) != 0) {
	    perror("mutex cond");
	    
        return NULL;
    }   
    
    strncpy(n->pathname, pathname, MAX_NAME_LENGTH); 
    n->locked = locked; 
    n->fd_locker = (locked) ? fd : -1; 
    n->locker_can_write = locked;

    n->opener_q = queue_init();
    if (!n->opener_q)
    {
        fprintf(stderr, "fd: %d, initQueue fallita\n", fd);
        if (&n->filecond)  pthread_cond_destroy(&n->filecond);
        freeStorageNode(n);
        return EAGAIN;
    }
    if (queue_push(n->opener_q, fd) != 0) {
        fprintf(stderr, "fd: %d, push opener_q fallita\n", fd);
        if (&n->filecond)  pthread_cond_destroy(&n->filecond);
        freeStorageNode(n);
        return EAGAIN;
    }

    n->next = NULL;


    LockQueue(q);
    if (queue_s_find(q, pathname) != NULL) {
        if (&n->filecond)  pthread_cond_destroy(&n->filecond);
        freeStorageNode(n);

        UnlockQueue(q);

        return EPERM;
    }

    if (q->cur_numfiles == 0) {
        q->head = n;
        q->tail = n;
    } else {
        printf("q->tail: %d\n", q->tail); // TODO togliere
        fflush(stdout);

        q->tail->next = n;
        q->tail = n;
    }

    if (q->cur_numfiles == q->limit_num_files) {
        printf("Limite di files raggiunto\n");
        if((poppedNode = queue_s_pop(q)) == NULL) {
            UnlockQueue(q);
            return errno;
        }

        q->replace_occur++;
        q->cur_numfiles += 1;
        UnlockQueue(q);

        if (send_to_client_and_free(fd, 1, poppedNode) == -1) {
            return -1;
        }

        return 0;
    } else {
        if (q->max_num_files == q->cur_numfiles) q->max_num_files++;
    }
    q->cur_numfiles += 1;

    UnlockQueue(q);

    msg_response_t res;
    res.result = 0;
    res.datalen = 0;

    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {
        close(fd); //TODO gestire nel client
        printf("writen res queue_s_openFile errore\n");
        fflush(stdout);
        return -1;
    }

    return 0;
}

int queue_s_updateOpeners(StorageQueue_t *q, char *pathname, bool locked, int fd) {
    if ((q == NULL) || (pathname == NULL)) { 
        return EINVAL;
    }

    StorageNode_t *item;

    LockQueue(q);
    if ((item = queue_s_find(q, pathname)) == NULL) {
        UnlockQueue(q);
        return ENOENT;
    }

    while (item->locked && item->fd_locker != fd) {
        printf("queue_s_openFile, fd: %fd, file: %s locked, aspetto...\n");
        UnlockQueueAndWait(q, item);
    }
    if (item->locked) { //&& item->fd_locker == fd
        if (!locked) { //non lo voglio lockare, unlocko
            item->locked = false;
            item->fd_locker = -1;
        }
        item->locker_can_write = false;
    } else {
        if (locked) {
            item->locked = true;
            item->fd_locker = fd;
            item->locker_can_write = false; //superfluo
        }
    }
    
    if(item->opener_q == NULL) {
        item->opener_q = queue_init();
        if (!q)
        {
            fprintf(stderr, "fd: %d, initQueue fallita\n", fd);
            UnlockQueue(q);
            return EAGAIN;
        }
        if (queue_push(item->opener_q, fd) != 0) {
            fprintf(stderr, "fd: %d, push opener_q fallita\n", fd);
            UnlockQueue(q);
            return EAGAIN;
        }
    } else if (queue_find(item->opener_q, fd) == NULL) {
        if(queue_push(item->opener_q, fd) != 0) {
            fprintf(stderr, "fd: %d, push opener_q fallita\n", fd);
            UnlockQueue(q);
            return EAGAIN;
        }
    } 

    UnlockQueue(q);

    msg_response_t res;
    res.result = 0;
    res.datalen = 0;

    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {
        close(fd); //TODO gestire nel client
        printf("writen res queue_s_openFile errore\n");
        fflush(stdout);
        return -1;
    }
    
    return 0;
}


int queue_s_readFile(StorageQueue_t *q, char *pathname, int fd) {
    if ((q == NULL) || (pathname == NULL)) { 
        return EINVAL;
    }

    StorageNode_t *item;

    LockQueue(q);
    if ((item = queue_s_find(q, pathname)) == NULL) {
        UnlockQueue(q);
        return ENOENT;
    }

    if (item->locked && item->fd_locker != fd) {
        UnlockQueue(q);
        return EPERM;
    }

    if (item->locked) { //&& item->fd_locker == fd
        item->locker_can_write = false;
    }

    if (queue_find(item->opener_q, fd) == NULL) {
        UnlockQueue(q); 
        return EPERM;
    } 

    msg_response_t res; // messaggio risposta al client
    res.result = 0;
    res.datalen = item->len;
    strcpy(res.pathname, item->pathname); //superfluo

    //invio messaggio al client
    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {
        close(fd); //TODO gestire nel client
        printf("writen res queue_s_readFile errore\n");
        fflush(stdout);

        UnlockQueue(q);
        return -1;
    }

    if (writen(fd, item->data, res.datalen) != res.datalen) {
        //TODO gestione errore
        close(fd); //TODO gestire nel client
        printf("writen data queue_s_readFile errore\n");
        fflush(stdout);

        UnlockQueue(q);
        return -1;
    }

    printf("Letto (ed inviato) file %s, di lunghezza %d\n", item->pathname, item->len);
    printf("Contenuto: %s\n", item->data + '\0'); //TODO togliere
    fflush(stdout);

    UnlockQueue(q);

    return 0;
}

int queue_s_readNFiles(StorageQueue_t *q, char *pathname, int fd, int n) {
    msg_response_t res; // messaggio risposta al client
    res.result = 0;

    if ((q == NULL) || (pathname == NULL)) { 
        return EINVAL;
    }

    LockQueue(q);

    if (q->cur_numfiles < n || n <= 0) res.datalen = q->cur_numfiles;
    else res.datalen = n;

    printf("Numero di files che verranno inviati: %d\n", res.datalen);

    // invio messaggio al client
    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {
        close(fd); //TODO gestire nel client
        printf("writen res queue_s_readFile errore\n");
        fflush(stdout);

        UnlockQueue(q);
        return -1;
    }

    StorageNode_t *temp = q->head;
    int num = res.datalen;
    int i = 1;
    while (i <= num) {
        res.datalen = temp->len;
        strcpy(res.pathname, temp->pathname);

        //invio messaggio al client
        if (writen(fd, &res, sizeof(res)) != sizeof(res)) {
            close(fd); //TODO gestire nel client
            printf("writen res queue_s_readFile errore\n");
            fflush(stdout);

            UnlockQueue(q);
            return -1;
        }

        if (writen(fd, temp->data, res.datalen) != res.datalen) {
            //TODO gestione errore
            close(fd); //TODO gestire nel client
            printf("writen data queue_s_readFile errore\n");
            fflush(stdout);

            UnlockQueue(q);
            return -1;
        }


        printf("Letto (ed inviato) file %s, di lunghezza %d\n", temp->pathname, temp->len);
        printf("Contenuto: %s\n", temp->data + '\0'); //TODO togliere
        fflush(stdout);

        temp = temp->next;
        i++;

    }

    UnlockQueue(q);

    return 0;
}


int queue_s_writeFile(StorageQueue_t *q, char *pathname, int fd, void *buf, int buf_len) {
    if ((q == NULL) || (pathname == NULL) || buf == NULL) { 
        return EINVAL;
    }

    StorageNode_t *item;
    void *data = NULL;

    LockQueue(q);
    if ((item = queue_s_find(q, pathname)) == NULL) {
        UnlockQueue(q);
        return ENOENT;
    }

    if (!(item->locked) || item->fd_locker != fd || !(item->locker_can_write)) {
        UnlockQueue(q);
        return EPERM;
    }

    if (queue_find(item->opener_q, fd) == NULL) {
        UnlockQueue(q);
        return EPERM;
    }

    if (buf_len > q->storage_capacity) {
        UnlockQueue(q);
        return ENOMEM;
    }

    StorageNode_t *poppedHead = NULL;
    int num_poppedFiles = 0;
    int result = queue_s_popUntil(q, buf_len, &num_poppedFiles, &poppedHead);
    if (result != 0) {
        UnlockQueue(q);
        return result;
    }

    void *tmp = malloc(buf_len);
    if (tmp == NULL) {
        UnlockQueue(q);

        fprintf(stderr, "fd: %d, queue_s_writeToFile malloc fallita\n", fd);
        return ENOMEM;
    } 

    memcpy(tmp, buf, buf_len);

    item->len = buf_len;
    item->data = tmp;

    printf("Scritto il file %s, per una lunghezza di %d\n", item->pathname, item->len);
    printf("Contenuto: %s\n", item->data + '\0'); //TODO togliere
    fflush(stdout);

    q->cur_usedstorage += item->len;

    if (q->max_used_storage < q->cur_usedstorage) q->max_used_storage = q->cur_usedstorage;

    item->locker_can_write = false;

    UnlockQueue(q);

    if (num_poppedFiles != 0) { 
        if (send_to_client_and_free(fd, num_poppedFiles, poppedHead) == -1) {
            return -1;
        }

        return 0;
    }

    msg_response_t res;
    res.result = 0;
    res.datalen = 0;

    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {
        close(fd); //TODO gestire nel client
        printf("writen res queue_s_writeFile errore\n");
        fflush(stdout);
        return -1;
    }

    return 0;
}


int queue_s_appendToFile(StorageQueue_t *q, char *pathname, int fd, void *buf, int buf_len) {

    if ((q == NULL) || (pathname == NULL)) { 
        return EINVAL;
    }

    StorageNode_t *item = NULL;
    void *data = NULL;

    LockQueue(q);
    if ((item = queue_s_find(q, pathname)) == NULL) {
        UnlockQueue(q);
        return ENOENT;
    }

    if (queue_find(item->opener_q, fd) == NULL) {
        printf("item->opener_q == NULL ? %d\n", (item->opener_q == NULL));
        fflush(stdout);
        UnlockQueue(q);
        return EPERM;
    } 

    // HO SOLO GUARDATO DI ESSERE UN OPENER
    // L'ATOMICITA' è garantita da LockQueue(q)

    StorageNode_t *poppedHead = NULL;
    int num_poppedFiles = 0;
    int result = queue_s_popUntil(q, buf_len, &num_poppedFiles, &poppedHead);
    if (result != 0) {
        UnlockQueue(q);
        return result;
    }

    void *tmp = realloc(item->data, (item->len) + buf_len);
    if (tmp == NULL) {
        UnlockQueue(q);

        fprintf(stderr, "fd: %d, queue_s_appendToFile realloc fallita\n", fd);
        return ENOMEM;
    } 

    memcpy(&tmp[item->len], buf, buf_len);

    item->data = tmp;
    item->len += buf_len;

    printf("Append sul file %s, lunghezza attuale di %d\n", item->pathname, item->len);
    printf("Contenuto: %s\n", item->data + '\0'); //TODO togliere
    fflush(stdout);

    q->cur_usedstorage += buf_len;

    if (q->max_used_storage < q->cur_usedstorage) q->max_used_storage = q->cur_usedstorage;

    UnlockQueue(q);

    if (num_poppedFiles != 0) { 
        if (send_to_client_and_free(fd, num_poppedFiles, poppedHead) == -1) {
            return -1;
        }

        return 0;
    }

    msg_response_t res;
    res.result = 0;
    res.datalen = 0;

    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {
        close(fd); //TODO gestire nel client
        printf("writen res queue_s_appendToFile errore\n");
        fflush(stdout);
        return -1;
    }

    return 0;
}

int queue_s_lockFile(StorageQueue_t *q, char *pathname, int fd) {
    if ((q == NULL) || (pathname == NULL)) { 
        return EINVAL;
    }

    StorageNode_t *item;

    LockQueue(q);
    if ((item = queue_s_find(q, pathname)) == NULL) {
        UnlockQueue(q);
        return ENOENT;
    }

    while (item->locked && item->fd_locker != fd) {
        printf("queue_s_lockFile, fd: %fd, file: %s locked, aspetto...\n");   
        UnlockQueueAndWait(q, item);
    }

    if (queue_find(item->opener_q, fd) == NULL) {
        UnlockQueue(q);
        return EPERM;
    } 

    item->locked = true;
    item->fd_locker = fd;
    item->locker_can_write = false;

    UnlockQueue(q);

    msg_response_t res;
    res.result = 0;
    res.datalen = 0;

    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {
        close(fd); //TODO gestire nel client
        printf("writen res queue_s_lockFile errore\n");
        fflush(stdout);
        return -1;
    }

    return 0;
}


int queue_s_unlockFile(StorageQueue_t *q, char *pathname, int fd) {
    if ((q == NULL) || (pathname == NULL)) { 
        return EINVAL;
    }

    StorageNode_t *item;

    LockQueue(q);
    if ((item = queue_s_find(q, pathname)) == NULL) {
        UnlockQueue(q);
        return ENOENT;
    }

    if (!(item->locked) || item->fd_locker != fd) {
        UnlockQueue(q);
        return EPERM;
    }

    if (queue_find(item->opener_q, fd) == NULL) {
        UnlockQueue(q);
        return EPERM;
    } 

    item->locked = false;
    item->fd_locker = -1;
    item->locker_can_write = false;

    UnlockQueueAndSignal(q, item);

    msg_response_t res;
    res.result = 0;
    res.datalen = 0;

    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {
        close(fd); //TODO gestire nel client
        printf("writen res queue_s_writeFile errore\n");
        fflush(stdout);
        return -1;
    }

    return 0;
}


int queue_s_closeFile(StorageQueue_t *q, char *pathname, int fd) {
    if ((q == NULL) || (pathname == NULL)) {  
        return EINVAL;
    }

    StorageNode_t *item;

    LockQueue(q);
    if ((item = queue_s_find(q, pathname)) == NULL) {
        UnlockQueue(q);
        return ENOENT;
    }
    
    int esito;
    if ((esito = queue_deleteNode(item->opener_q, fd)) == -1) {
        UnlockQueue(q);
        return EPERM;
    } else if (esito == 1) { //è andata bene la delete (fd era l'ultimo opener)
        printf("deleteNode OK (ed fd era l'ultimo opener)\n");
        item->opener_q = NULL;
    }

    if(item->locked && item->fd_locker == fd) {
        item->locked = false;
        item->fd_locker = -1;
        item->locker_can_write = false;
    }

    UnlockQueue(q);

    msg_response_t res;
    res.result = 0;
    res.datalen = 0;

    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {
        close(fd); //TODO gestire nel client
        printf("writen res queue_s_closeFile errore\n");
        fflush(stdout);
        return -1;
    }

    return 0;
}

int queue_s_closeFdFiles(StorageQueue_t *q, int fd) {
    if (q == NULL) { 
        return EINVAL;
    }

    LockQueue(q);

    StorageNode_t *item = q->head;

    while(item != NULL) {
        int esito;
        if (item->opener_q == NULL) {
            item = item->next;
            continue;
        }
        if ((esito = queue_deleteNode(item->opener_q, fd)) == -1) {
            UnlockQueue(q);
            return EPERM;
        } else if (esito == 1) { //è andata bene la delete (fd era l'ultimo opener)
            item->opener_q = NULL;
        }

        if(item->locked && item->fd_locker == fd) {
            item->locked = false;
            item->fd_locker = -1;
            item->locker_can_write = false;
        }

        item = item->next;
    }

    UnlockQueue(q);

    return 0;


}

int queue_s_removeFile(StorageQueue_t *q, char *pathname, int fd) {
    if ((q == NULL) || (pathname == NULL)) { 
        return EINVAL;
    }

    LockQueue(q);

    StorageNode_t *temp = q->head;
    StorageNode_t *prev = NULL;
     
    // Se la testa conteneva fd
    if (temp != NULL && (strcmp(temp->pathname, pathname) == 0)) {
        q->head = temp->next;
        if (q->head == NULL) // cioè q->cur_numfiles è 1 (che stiamo togliendo adesso)
            q->tail = NULL;

    } else {
        while (temp != NULL && (strcmp(temp->pathname, pathname) != 0)){
            prev = temp;
            temp = temp->next;
        }
    
        // Se pathname non era presente
        if (temp != NULL) {
            if (q->tail == temp) q->tail = prev;

            prev->next = temp->next;
        }
    }

    if (temp != NULL) {
        if (!(temp->locked) || temp->fd_locker != fd) {
            UnlockQueue(q); 
            return EPERM;
        }

        if (queue_find(temp->opener_q, fd) == NULL) {
            UnlockQueue(q);
            return EPERM;
        } 

        q->cur_numfiles -= 1;
        q->cur_usedstorage = q->cur_usedstorage - temp->len;

        free(temp->data);
        if (&temp->filecond)  pthread_cond_destroy(&temp->filecond);
        freeStorageNode(temp);
    }

    UnlockQueue(q);

    msg_response_t res;
    res.result = 0;
    res.datalen = 0;


    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {
        close(fd); //TODO gestire nel client
        printf("writen res queue_s_deleteFile errore\n");
        fflush(stdout);
        return -1;
    }
    return 0;
}

void queue_s_printListFiles(StorageQueue_t *q) {

    if (q == NULL) return;

    LockQueue(q);

    StorageNode_t *curr = q->head;

    while(curr != NULL) {
        printf("%s\n", curr->pathname);
        fflush(stdout);

        curr = curr->next;

    }

    UnlockQueue(q);

}


// NOTA: in questa funzione si puo' accedere a q->cur_numfiles NON in mutua esclusione
//       ovviamente il rischio e' quello di leggere un valore non aggiornato, ma nel
//       caso di piu' produttori e consumatori la lunghezza della coda per un thread
//       e' comunque un valore "non affidabile" se non all'interno di una transazione
//       in cui le varie operazioni sono tutte eseguite in mutua esclusione. 
unsigned long queue_s_length(StorageQueue_t *q) {
    LockQueue(q);
    unsigned long len = q->cur_numfiles;
    UnlockQueue(q);
    return len;
}


StorageNode_t* queue_s_find(StorageQueue_t *q, char *pathname) {

    StorageNode_t *curr = q->head;
    StorageNode_t *found = NULL;

    while(found == NULL && curr != NULL) {
        printf("queue_s_find, locked: %d, fd: %d, cwrite: %d, pathname: %s\n", curr->locked, curr->fd_locker, curr->locker_can_write, curr->pathname);
        fflush(stdout);
        
        if (strcmp(curr->pathname, pathname) == 0) {
            found = curr;
        } else {
            curr = curr->next;   
        }

    }

    return found;
}


StorageNode_t *queue_s_pop(StorageQueue_t *q) {        
    if (q == NULL) { 
        errno= EINVAL; 
        return NULL;
    }
    
    if (q->cur_numfiles == 0) {
	    errno = ENOTTY;
        return NULL;
    }
    
    assert(q->head);
    StorageNode_t *n  = (StorageNode_t *)q->head;
    StorageNode_t *poppedNode = q->head;
    q->head = q->head->next;
    poppedNode->next = NULL;

    if (q->cur_numfiles == 1) //il nodo da poppare è l'ultimo
        q->tail = q->head;

    q->cur_numfiles -= 1;
    q->cur_usedstorage = q->cur_usedstorage - n->len;

    return poppedNode;

} 


int queue_s_popUntil(StorageQueue_t *q, int buf_len, int *num_poppedFiles, StorageNode_t **poppedHead) {
    if (q == NULL) { 
        return EINVAL;
    }
    
    if (q->cur_numfiles == 0) {
        return ENOTTY;
    }

    if (buf_len > q->storage_capacity) {
        return ENOMEM;
    }
    
    *poppedHead = q->head;
    StorageNode_t *prec = NULL;

    bool cache_replacement = false;
    while ((q->cur_usedstorage + buf_len) > q->storage_capacity) {

        cache_replacement = true;
        q->cur_numfiles -= 1;
        q->cur_usedstorage = q->cur_usedstorage - q->head->len;

        *num_poppedFiles = (*num_poppedFiles) +1;

        prec = q->head;
        q->head = q->head->next;
    }
    if (prec != NULL) prec->next = NULL;

    if (q->head == NULL)
        q->tail = NULL;

    if (cache_replacement)
        q->replace_occur++;


    return 0;
}


int send_to_client_and_free(int fd, int num_files, StorageNode_t *poppedH) {
    StorageNode_t *temp = NULL;
    msg_response_t res; // messaggio risposta al client
    res.result = 0;
    res.datalen = num_files;

    // invio messaggio al client (quanti files riceverà)
    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {
        close(fd); //TODO gestire nel client
        printf("writen res send_to_client_and_free errore\n");
        fflush(stdout);
        queue_s_deleteNodes(poppedH);
        return -1;
    }

    printf("Numero di files in espulsione: %d\n", num_files);    

    while(poppedH != NULL) {
        temp = poppedH;
        res.datalen = temp->len;
        strcpy(res.pathname, temp->pathname);

        printf("Espulso file %s, di lunghezza %d\n", temp->pathname, temp->len);
        printf("Contenuto: %s\n", temp->data + '\0'); //TODO togliere
        fflush(stdout);

        if (writen(fd, &res, sizeof(res)) != sizeof(res)) {
            close(fd); //TODO gestire nel client
            printf("writen res send_to_client_and_free errore\n");
            fflush(stdout);
            queue_s_deleteNodes(poppedH);
            return -1;
        }

        if (res.datalen > 0) {
            if (writen(fd, temp->data, res.datalen) != res.datalen) {
                    //TODO gestione errore
                close(fd); //TODO gestire nel client
                printf("writen data send_to_client_and_free errore\n");
                fflush(stdout);
                queue_s_deleteNodes(poppedH);
                return -1;
            }

            free(temp->data);

        }



        num_files--;
        poppedH = poppedH->next;

        if (&temp->filecond)  pthread_cond_destroy(&temp->filecond);
        freeStorageNode(temp);

    }

    assert(num_files == 0);

    return 0;
}

