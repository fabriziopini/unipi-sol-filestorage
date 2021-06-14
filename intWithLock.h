#ifndef INT_WITH_LOCK_H
#define INT_WITH_LOCK_H

#include <pthread.h>

// intero con lock
typedef struct IntWithLock {
    int total_clients; // numero dei client connessi
    pthread_mutex_t tc_lock;
} IntWithLock_t;

/** Inizializza la struttura, settando 'total_clients' a zero, ed inizializzando il mutex tc_lock
 *  Deve essere chiamata da un solo thread (tipicamente il thread main).
 * 
 *   \retval puntatore alla struttura allocata ed inizializzata (NULL se errore)
 */
IntWithLock_t *initIntWithLock();

/** Cancella la struttura allocata con initIntWithLock. Deve essere chiamata
 *  da un solo thread (tipicamente il thread main)
 *  
 *   \param iwl puntatore alla struttura (IntWithLock) da cancellare
 */
void deleteIntWithLock(IntWithLock_t *iwl);

/** Aumenta di uno l'intero (iwl->total_clients)
 *  
 *   \param iwl puntatore alla struttura (IntWithLock)
 */
void addClient(IntWithLock_t *iwl);

/** Diminuisce di uno l'intero (iwl->total_clients)
 *  
 *   \param iwl puntatore alla struttura (IntWithLock)
 */
void deleteClient(IntWithLock_t *iwl);

/** Restituisce l'intero (iwl->total_clients)
 *  
 *   \param iwl puntatore alla struttura (IntWithLock)
 * 
 *   \retval l'intero contenuto nella struttura (iwl->total_clients)
 */
int checkTotalClients(IntWithLock_t *iwl);


#endif
