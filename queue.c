#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>

// TODO negli esercizi erano scritte < > , non " "
#include "util.h"
#include "queue.h"

/**
 * @file queue.c
 * @brief File di implementazione dell'interfaccia per la coda
 */



/* ------------------- funzioni di utilità -------------------- */

// qui assumiamo (per semplicità) che le mutex non siano mai di
// tipo 'robust mutex' (pthread_mutex_lock(3)) per cui possono
// di fatto ritornare solo EINVAL se la mutex non e' stata inizializzata.

static inline Node_t  *allocNode()                  { return malloc(sizeof(Node_t));  }
static inline Queue_t *allocQueue()                 { return malloc(sizeof(Queue_t)); }
static inline void freeNode(Node_t *node)           { free((void*)node); }
static inline void LockQueue(Queue_t *q)            { LOCK(&q->qlock);   }
static inline void UnlockQueue(Queue_t *q)          { UNLOCK(&q->qlock); }
static inline void UnlockQueueAndWait(Queue_t *q)   { WAIT(&q->qcond, &q->qlock); }
static inline void UnlockQueueAndSignal(Queue_t *q) { SIGNAL(&q->qcond); UNLOCK(&q->qlock); }

/* ------------------- interfaccia della coda ------------------ */

Queue_t *queue_init() {
    Queue_t *q = allocQueue();
    
    if (!q) return NULL;

    q->head = NULL;
    q->tail = NULL;    
    q->qlen = 0;

    if (pthread_mutex_init(&q->qlock, NULL) != 0) {
        perror("mutex init");
        return NULL;
    }

    if (pthread_cond_init(&q->qcond, NULL) != 0) {
	    perror("mutex cond");

        if (&q->qlock) pthread_mutex_destroy(&q->qlock);
           
        return NULL;
    }  

    return q;
}


void queue_delete(Queue_t *q) {
    if (q == NULL) return;

    while(q->head != NULL) {
	    Node_t *p = (Node_t*)q->head;
	    q->head = q->head->next;
	    freeNode(p);
    }

    if (&q->qlock)  pthread_mutex_destroy(&q->qlock);
    if (&q->qcond)  pthread_cond_destroy(&q->qcond);
    free(q);
}


int queue_push(Queue_t *q, int data) {
    if (q == NULL) { errno= EINVAL; return -1;}
    Node_t *n = allocNode();
    if (!n) return -1;
    n->data = data; 
    n->next = NULL;

    LockQueue(q);

    if (q->qlen == 0) {
        q->head = n;
        q->tail = n;
    } else {
        q->tail->next = n;
        q->tail = n;
    }
    
    q->qlen += 1;
    UnlockQueueAndSignal(q);
    return 0;
}


int queue_pop(Queue_t *q) {        
    if (q == NULL) {
        errno= EINVAL; 
        return -1;
    }

    LockQueue(q);
    while(q->qlen == 0) {
	    UnlockQueueAndWait(q);
    }

    Node_t *n  = (Node_t *)q->head;
    int data = q->head->data;
    q->head    = q->head->next;
    q->qlen   -= 1;

    UnlockQueue(q);
    freeNode(n);
    return data;
} 


Node_t* queue_find(Queue_t *q, int fd) {
    if (q == NULL) {
        errno= EINVAL; 
        return -1;
    }
    LockQueue(q);
    Node_t *curr = q->head;
    Node_t *found = NULL;

    printf("queue_find fd:%d, in opener_q\n", fd);

    while(found == NULL && curr != NULL) {
        printf("fd: %d\n", curr->data);
        if (fd == curr->data) {
            found = curr;
        } else {
            curr = curr->next;   
        }
    }

    UnlockQueue(q);
    return found;
}


int queue_deleteNode(Queue_t *q, int fd) {
    if (q == NULL) {
        errno = EINVAL; 
        return -1;
    }

    LockQueue(q);

    Node_t* temp = q->head;
    Node_t* prev = NULL;
     
    // Se la testa conteneva fd
    if (temp != NULL && temp->data == fd)
    {
        q->head = temp->next;

        freeNode(temp);
        q->qlen   -= 1;
        if (q->qlen == 0) {
            if (&q->qlock)  pthread_mutex_destroy(&q->qlock);
            if (&q->qcond)  pthread_cond_destroy(&q->qcond);
            free(q);

            return 1;
        } else {
            q->qlen -= 1;
            UnlockQueue(q);
        }

        return 0;
    } else {
        while (temp != NULL && temp->data != fd) {
            prev = temp;
            temp = temp->next;
        }
 
        // Se fd non era presente
        if (temp == NULL) {
            UnlockQueue(q);
            return 0;
        }
    
        prev->next = temp->next;
    
        freeNode(temp);
    }

    q->qlen -= 1;

    UnlockQueue(q);
    return 0;
}


// NOTA: in questa funzione si puo' accedere a q->qlen NON in mutua esclusione
//       ovviamente il rischio e' quello di leggere un valore non aggiornato, ma nel
//       caso di piu' produttori e consumatori la lunghezza della coda per un thread
//       e' comunque un valore "non affidabile" se non all'interno di una transazione
//       in cui le varie operazioni sono tutte eseguite in mutua esclusione. 
unsigned long queue_length(Queue_t *q) {
    LockQueue(q);
    unsigned long len = q->qlen;
    UnlockQueue(q);
    return len;
}



