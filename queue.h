#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>

/** Elemento della coda.
 *
 */
typedef struct Node {
    int           data;
    struct Node * next;
} Node_t;

/** Struttura dati coda.
 *
 */
typedef struct Queue {
    Node_t        *head;    // elemento di testa
    Node_t        *tail;    // elemento di coda 
    unsigned long  qlen;    // lunghezza 
    pthread_mutex_t qlock;
    pthread_cond_t  qcond;
} Queue_t;


/** Alloca ed inizializza una coda. Deve essere chiamata da un solo 
 *  thread (tipicamente il thread main).
 *
 *   \retval NULL se si sono verificati problemi nell'allocazione
 *   \retval puntatore alla coda allocata
 */
Queue_t *queue_init();

/** Cancella una coda allocata con initQueue. Deve essere chiamata da
 *  da un solo thread (tipicamente il thread main).
 *  
 *   \param q puntatore alla coda da cancellare
 */
void queue_delete(Queue_t *q);

/** Inserisce un dato nella coda.
 *   \param q puntatore alla coda
 *   \param data dato da inserire
 *  
 *   \retval 0 se successo
 *   \retval -1 se errore (errno settato opportunamente)
 */
int queue_push(Queue_t *q, int data);

/** Estrae un dato dalla coda.
 *   \param q puntatore alla coda
 *
 *   \retval se successo: dato estratto (>= 0)
 *   \retval se errore -1 (errno settato opportunamente)
 */
int queue_pop(Queue_t *q);

/** Cerca fd nella coda * 
 *   \param q puntatore alla coda
 *   \param fd dato da cercare
 * 
 *   \retval puntatore al nodo contenente il dato da cercare
 *   \retval NULL in caso di errore (errno settato opportunamente)
 */
Node_t* queue_find(Queue_t *q, int fd);

/** Rimuove (se c'è) fd dalla coda (la prima occorrenza (ce ne dovrebbe essere solo una))
 *   \param q puntatore alla coda
 *   \param fd dato da rimuovere
 * 
 *   \retval se successo (se presente rimosso): 0 oppure 1 (se fd era l'ultimo elemento della coda)
 *   \retval se errore -1 (errno settato opportunamente)
 */
int queue_deleteNode(Queue_t *q, int fd);

/** Ritorna la lunghezza attuale della coda passata come parametro.
 */
unsigned long queue_length(Queue_t *q);

#endif /* QUEUE_H_ */
