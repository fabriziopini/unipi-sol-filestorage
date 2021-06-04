#ifndef STORAGE_QUEUE_H
#define STORAGE_QUEUE_H

#include <pthread.h>
#include <stdbool.h>
#include "common_def.h"
#include "queue.h"

#define MAX_NAME_LENGTH 256

// TODO ricordarsi di scrivere che sto poppando file, fregandomene del fatto che potrebbero essere locked


/** Elemento della coda.
 *
 */
typedef struct StorageNode {
    char 	        pathname[MAX_NAME_LENGTH];	// nome completo del file (univoco nello storage)
	void* 	        data;	    // dati del file
	int  	        len;		// lunghezza del file
	bool	        locked;		// flag di lock da client
	int	            fd_locker;	// fd del processo client che ha fatto lock
    bool            locker_can_write; // true se l'ultima operazione fatta dal locker è una open con create_lock
    Queue_t         *opener_q;  // TODO quando un client esce, devo scorrere tutti i nodi della coda, e di ciascun nodo scorrere questa coda (opener) per togliere il client (se c'è)
                                    // E' molto costoso, però va bene così se presuppongo che:
                                    //     il mio serverStorage ha come scopo servire POCHI client (o comunque che condividono pochi file tra di loro) che mettono tanti file a testa (quindi il peso di scorrere le liste dei client è limitato, rispetto allo scorrere le liste dei file per ogni operazione)
                                    //     e che i client facciano varie operazioni, mentre l'uscita è un evento raro
    pthread_cond_t  filecond;   // locked && fd_locker != fd
	struct StorageNode* next;
} StorageNode_t;


/** Struttura dati coda.
 *
 */
typedef struct StorageQueue {
    StorageNode_t*  head;               // elemento di testa
    StorageNode_t*  tail;               // elemento di coda 
    int             cur_numfiles;       // lunghezza ovvero numero di files in storage
    unsigned long   cur_usedstorage;    //dimensione storage attuale in bytes
    
    int max_num_files; 			        // numero massimo raggiunto di file memorizzati nel server
    unsigned long max_used_storage; 	// dimensione massima in bytes raggiunta dal file storage;
    int replace_occur;			        // numero di volte in cui l’algoritmo di rimpiazzamento della cache è stato eseguito
    
    int limit_num_files;                // numero limite di file nello storage
    unsigned long storage_capacity;     // dimensione dello storage in bytes

    pthread_mutex_t qlock;
} StorageQueue_t;


/** Alloca ed inizializza una coda. Deve essere chiamata da un solo 
 *  thread (tipicamente il thread main).
 * 
 *   \param limit_num_files numero limite di file nello storage
 *   \param storage_capacity dimensione dello storage in bytes
 *
 *   \retval NULL se si sono verificati problemi nell'allocazione (errno settato)
 *   \retval puntatore alla coda allocata
 */
StorageQueue_t *queue_s_init(int limit_num_files, unsigned long storage_capacity);


/** Cancella una coda allocata con initQueue. Deve essere chiamata
 *  da un solo thread (tipicamente il thread main).
 *  
 *   \param q puntatore alla coda da cancellare
 */
void queue_s_delete(StorageQueue_t *q);


/** Inserisce un nuovo nodo (file identificato da pathname) nella coda
 *  (inizializzando correttamente i vari campi), ed in caso di successo comunica
 *  al client sul socket fd_locker
 * 
 *   \param q puntatore alla coda
 *   \param pathname del file
 *   \param locked esprime la volontà di avere il lock sul file pathaname
 *   \param fd_locker file descriptor da cui arriva la richiesta
 *  
 *   \retval 0 se successo
 *   \retval -1 se errore sul socket fd
 *   \retval errno altrimenti
 */
int queue_s_push(StorageQueue_t *q, char *pathname, bool locked, int fd_locker);


/** Aggiorna gli opener del nodo, (file) della coda ,identificato da pathname
 *  (inizializzando correttamente i vari campi), ed in caso di successo comunica
 *  al client sul socket fd_locker
 * 
 *   \param q puntatore alla coda
 *   \param pathname del file
 *   \param locked esprime la volontà di avere il lock sul file pathaname
 *   \param fd file descriptor da cui arriva la richiesta
 *  
 *   \retval 0 se successo
 *   \retval -1 se errore sul socket fd
 *   \retval errno altrimenti
 */
int queue_s_updateOpeners(StorageQueue_t *q, char *pathname, bool locked, int fd);


/** Cerca nella coda il file identificato da pathname, nel caso lo trovasse, lo
 *  comunica al client (sul socket fd)
 *   \param q puntatore alla coda
 *   \param pathname del file
 *   \param fd file descriptor da cui arriva la richiesta
 *  
 *   \retval 0 se successo
 *   \retval -1 se errore sul socket fd
 *   \retval errno altrimenti
 */
int queue_s_readFile(StorageQueue_t *q, char *pathname, int fd);

/** Comunica al client (sul socket fd_locker), i files contenuti nella coda, fino
 *  ad un massimo di n (se n<=0 comunica tutti i file contenuti nella coda)
 *  
 *   \param q puntatore alla coda
 *   \param pathname del file
 *   \param fd file descriptor da cui arriva la richiesta
 *  
 *   \retval 0 se successo
 *   \retval -1 se errore sul socket fd
 *   \retval errno altrimenti
 * 
 */
int queue_s_readNFiles(StorageQueue_t *q, char *pathname, int fd, int n);


/** Cerca nella coda il file identificato da pathname (controlla che l'ultima
 *  operazione sul file sia una open con i flag create e lock), copia il file puntato da buf
 *  ed in caso di successo comunica al client sul socket fd
 * 
 *   \param q puntatore alla coda
 *   \param pathname del file
 *   \param fd file descriptor da cui arriva la richiesta
 *   \param buf dati da copiare
 *   \param buf_len lunghezza dei dati da copiare
 *  
 *   \retval 0 se successo
 *   \retval -1 se errore sul socket fd
 *   \retval errno altrimenti
 */
int queue_s_writeFile(StorageQueue_t *q, char *pathname, int fd, void *buf, int buf_len);


/** Cerca nella coda il file identificato da pathname, aggiunge ad esso in append il file puntato da buf
 *  ed in caso di successo comunica al client sul socket fd
 * 
 *   \param q puntatore alla coda
 *   \param pathname del file
 *   \param fd file descriptor da cui arriva la richiesta
 *   \param buf dati da copiare
 *   \param buf_len lunghezza dei dati da copiare
 *  
 *   \retval 0 se successo
 *   \retval -1 se errore sul socket fd
 *   \retval errno altrimenti
 */
int queue_s_appendToFile(StorageQueue_t *q, char *pathname, int fd, void *buf, int buf_len);


/** Cerca di prendere il lock (cioè fd_locker = fd) sul node (file) idetificato da pathname,
 *  in caso di successo comunica al client sul socket fd
 *  //TODO al momento fallisce subito, fare wait
 * 
 *   \param q puntatore alla coda
 *   \param pathname del file
 *   \param fd file descriptor da cui arriva la richiesta
 *  
 *   \retval 0 se successo
 *   \retval -1 se errore sul socket fd
 *   \retval errno altrimenti
 */
int queue_s_lockFile(StorageQueue_t *q, char *pathname, int fd);


/** Rilascia il lock, ed in caso di successo comunica al client sul socket fd
 * 
 *   \param q puntatore alla coda
 *   \param pathname del file
 *   \param fd file descriptor da cui arriva la richiesta
 *  
 *   \retval 0 se successo (fd era il possessore della lock sul file identificato da pathname)
 *   \retval -1 se errore sul socket fd
 *   \retval errno altrimenti
 */
int queue_s_unlockFile(StorageQueue_t *q, char *pathname, int fd);

/** Client fd chiude il file identificato da pathname (si aggiorna opener_q), se lo aveva aperto,
 *  in caso di successo comunica al client sul socket fd
 * 
 *   \param q puntatore alla coda
 *   \param pathname del file
 *   \param fd file descriptor da cui arriva la richiesta
 *  
 *   \retval 0 se successo
 *   \retval -1 se errore sul socket fd
 *   \retval errno altrimenti
 */
int queue_s_closeFile(StorageQueue_t *q, char *pathname, int fd);


/** Chiude (aggiornando i loro opener_q) tutti i file aperti da fd,
 *  in caso di successo comunica al client sul socket fd
 * 
 *   \param q puntatore alla coda
 *   \param fd file descriptor da cui arriva la richiesta
 *  
 *   \retval 0 se successo
 *   \retval -1 se errore sul socket fd
 *   \retval errno altrimenti
 */
int queue_s_closeFdFiles(StorageQueue_t *q, int fd);


/** Rimuove il file identificato da pathname dalla coda,
 *  in caso di successo comunica al client sul socket fd
 * 
 *   \param q puntatore alla coda
 *   \param pathname del file
 *   \param fd file descriptor da cui arriva la richiesta
 *  
 *   \retval 0 se successo (fd doveva avere il lock sul file)
 *   \retval -1 se errore sul socket fd
 *   \retval errno altrimenti
 */
int queue_s_removeFile(StorageQueue_t *q, char *pathname, int fd);

/** Stampa la lista dei file contenuti nella coda
 * 
 *   \param q puntatore alla coda
 *  
 */
void queue_s_printListFiles(StorageQueue_t *q);

/** Ritorna la lunghezza attuale della coda passata come parametro.
 */
unsigned long queue_s_length(StorageQueue_t *q);

#endif /* STORAGE_QUEUE_H_ */
