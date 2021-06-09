#ifndef OPT_QUEUE_H
#define OPT_QUEUE_H

#include <pthread.h>
#include "client_def.h"

#define MAX_NAME_LENGTH 256

/** Elemento della coda.
 *
 */
typedef struct Opt {
    input_opt  opt; // enum operazione
    char       arg[MAX_NAME_LENGTH]; // argomento operazione
    char       dirname[MAX_NAME_LENGTH]; // eventuale directory per opt. di read/write
    struct Opt *next;
} OptNode_t;

/** Struttura dati coda.
 *
 */
typedef struct OptQueue {
    OptNode_t        *head;    // elemento di testa
    OptNode_t        *tail;    // elemento di coda 
    unsigned long  qlen;    // lunghezza 
} OptQueue_t;

// Tutte le operazioni devono essere chiamate da un solo thread

/** Alloca ed inizializza una coda
 *
 *   \retval NULL se si sono verificati problemi nell'allocazione
 *   \retval puntatore alla coda allocata
 */
OptQueue_t *opt_queue_init();

/** Cancella una coda allocata con opt_queue_init.
 *  
 *   \param q puntatore alla coda da cancellare
 */
void opt_queue_delete(OptQueue_t *q);

/** Inserisce un nuovo nodo (operazione) nella coda
 *
 *   \param q puntatore alla coda
 *   \param opt operazione da inserire
 *   \param arg argomento operazione
 *  
 *   \retval 0 se successo
 *   \retval errno se errore
 */
int opt_queue_push(OptQueue_t *q, int opt, char *arg);

/** Setta q->dirname =  dirname, se l'operazione contenuta nella tail della coda è WRITEDIRNAME o WRITELIST
 *
 *   \param q puntatore alla coda
 *   \param dirname nome della directory (dove vengono scritti i file rimossi dal server)
 *  
 *   \retval 0 se successo
 *   \retval errno se errore
 */
int opt_queue_setWDirname(OptQueue_t *q, char *dirname);

/** Setta q->dirname =  dirname, se l'operazione contenuta nella tail della coda è READLIST o READN
 *
 *   \param q puntatore alla coda
 *   \param dirname nome della directory (dove vengono scritti i file letti dal server)
 *  
 *   \retval 0 se successo
 *   \retval errno se errore
 */
int opt_queue_setRDirname(OptQueue_t *q, char *dirname);


#endif /* OPT_QUEUE_H_ */
