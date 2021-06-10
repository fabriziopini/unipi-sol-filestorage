#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>

#include "util.h"
#include "opt_queue.h"
#include "client_def.h"


/**
 * @file opt_queue.c
 * @brief File di implementazione dell'interfaccia per la coda delle operazioni
 */



/* ------------------- funzioni di utilita' -------------------- */

static inline OptNode_t  *allocNode()                  { return malloc(sizeof(OptNode_t));  }
static inline OptQueue_t *allocQueue()                 { return malloc(sizeof(OptQueue_t)); }
static inline void freeNode(OptNode_t *node)           { free((void*)node); }

/* ------------------- interfaccia della coda ------------------ */

OptQueue_t *opt_queue_init() {
    OptQueue_t *q = allocQueue();
    
    if (!q) return NULL;

    q->head = NULL;
    q->tail = NULL;    
    q->qlen = 0;

    return q;
}


void opt_queue_delete(OptQueue_t *q) {
    if (q == NULL) return;

    while(q->head != NULL) {
	    OptNode_t *p = (OptNode_t*)q->head;
	    q->head = q->head->next;
	    freeNode(p);
    }

    free(q);
}


int opt_queue_push(OptQueue_t *q, int opt, char *arg) {
    if ((q == NULL) || (opt == -1) || arg == NULL) {
        return EINVAL;
    }

    OptNode_t *n = allocNode();
    if (!n) return ENOMEM;
    memset(n, '\0', sizeof(OptNode_t));


    n->opt = opt;
    strncpy(n->arg, arg, PATH_MAX); 
    printf("opt_queue_push arg:%s\n", n->arg);
    fflush(stdout);
    n->next = NULL;

    if (q->qlen == 0) {
        q->head = n;
        q->tail = n;
    } else {
        q->tail->next = n;
        q->tail = n;
    }
    
    q->qlen += 1;

    return 0;
}

int opt_queue_setRDirname(OptQueue_t *q, char *dirname) {
    
    if ((q == NULL) || (dirname == NULL)) { 
        return EINVAL;
    }

    if ((q->tail == NULL) || (q->tail->opt != READLIST && q->tail->opt != READN)) {
        return ENOENT;
    }

    strncpy(q->tail->dirname, dirname, PATH_MAX); 
    
    return 0;
}

int opt_queue_setWDirname(OptQueue_t *q, char *dirname) {
    if ((q == NULL) || (dirname == NULL)) { 
        return EINVAL;
    }

    if ((q->tail == NULL) || (q->tail->opt != WRITELIST && q->tail->opt != WRITEDIRNAME)) {
        return ENOENT;
    }

    strncpy(q->tail->dirname, dirname, PATH_MAX); 
    
    return 0;
}

int opt_queue_setADirname(OptQueue_t *q, char *dirname) {
    if ((q == NULL) || (dirname == NULL)) { 
        return EINVAL;
    }

    if ((q->tail == NULL) || (q->tail->opt != APPEND)) {
        return ENOENT;
    }

    strncpy(q->tail->dirname, dirname, PATH_MAX); 
    
    return 0;
}


