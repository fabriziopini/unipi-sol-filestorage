#ifndef API_H
#define API_H

/** Apre una connessione AF_UNIX al socket file sockname. Se il server non accetta immediatamente la
 *  richiesta di connessione, la connessione da parte del client viene ripetuta dopo ‘msec’ millisecondi e fino allo
 *  scadere del tempo assoluto ‘abstime’
 * 
 *   \param sockname nome del socket file a cui connetersi
 *   \param msec tempo in millisecondi
 *   \param abstime tempo assoluto massimo che si aspetta
 * 
 *
 *   \retval 0 se successo
 *   \retval -1 se errore, errno settato opportunamente
 */
int openConnection(const char* sockname, int msec, const struct timespec abstime);


/** Chiude la connessione AF_UNIX associata al socket file sockname
 * 
 *   \param sockname nome del socket file a cui connetersi
 *
 *   \retval 0 se successo
 *   \retval -1 se errore, errno settato opportunamente
 */
int closeConnection(const char* sockname);


/** Richiesta di apertura o creazione del file identificato da pathname (con o senza lock)
 *  La semantica dipende da i flags passati
 * 
 *   \param pathname nome del file
 *   \param flags 
 *   \param dirname nome della directory dove scrivere l'eventuale file espulso dal server
 *
 *   \retval 0 se successo
 *   \retval -1 se errore, errno settato opportunamente
 */
int openFile(const char* pathname, int flags, const char* dirname);


/** Legge tutto il contenuto del file dal server (se esiste) ritornando un puntatore
 *  ad un'area allocata sullo heap nel parametro ‘buf’, mentre ‘size’ conterrà
 *  la dimensione del buffer dati (ossia la dimensione in bytes del file letto)
 * 
 *   \param pathname nome del file
 *   \param buf puntatore
 *   \param size
 *
 *   \retval 0 se successo
 *   \retval -1 se errore, errno settato opportunamente (‘buf‘e ‘size’ non sono validi)
 */
int readFile(const char* pathname, void** buf, size_t* size);


/** Richiede al server la lettura di ‘N’ files qualsiasi da memorizzare nella
 *  directory ‘dirname’ lato client. Se il server ha meno di ‘N’ file disponibili,
 *  li invia tutti. Se N<=0 la richiesta al server è quella di leggere tutti i file
 *  memorizzati al suo interno
 * 
 *   \param N numero di files da leggere
 *   \param dirname nome della directory dove scrivere i file letti dal server
 *
 *   \retval >=0 se successo (numero di files letti)
 *   \retval -1 se errore, errno settato opportunamente
 */
int readNFiles(int N, const char* dirname);


/** Scrive tutto il file puntato da pathname nel file del server
 *  Ritorna successo solo se la precedente operazione, terminata con successo,
 *  è stata openFile(pathname, O_CREATE_LOCK). Se ‘dirname’ è diverso da NULL,
 *  i file eventualmente spediti dal server perchè espulsi dalla cache per far posto
 *  al file ‘pathname’ dovranno essere scritti in ‘dirname’
 * 
 *   \param pathname nome del file
 *   \param dirname nome della directory dove scrivere gli eventuali file espulsi dal server
 *
 *   \retval 0 se successo
 *   \retval -1 se errore, errno settato opportunamente
 */
int writeFile(const char* pathname, const char* dirname);


/** Richiesta di scrivere in append al file ‘pathname‘ i ‘size‘ bytes contenuti nel buffer ‘buf’
 *  Se ‘dirname’ è diverso da NULL, i file eventualmente spediti dal server perchè espulsi
 *  dalla cache per far posto ai nuovi dati di ‘pathname’ dovranno essere scritti in ‘dirname’
 * 
 *   \param pathname nome del file
 *   \param buf buffer contenente i dati
 *   \param size bytes del buffer
 *   \param dirname nome della directory dove scrivere gli eventuali file espulsi dal server
 *
 *   \retval 0 se successo
 *   \retval -1 se errore, errno settato opportunamente
 */
int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname);


/** Setta il flag O_LOCK al file. Se il file era stato aperto/creato con il flag O_LOCK e la 
 *  richiesta proviene dallo stesso processo, oppure se il file non ha il flag O_LOCK settato,
 *  l’operazione termina immediatamente con successo, altrimenti l’operazione non
 *  viene completata fino a quando il flag O_LOCK non viene resettato dal detentore della lock
 * 
 *   \param pathname nome del file
 *
 *   \retval 0 se successo (setta il flag O_LOCK al file)
 *   \retval -1 se errore, errno settato opportunamente
 */
int lockFile(const char* pathname);


/** Resetta il flag O_LOCK sul file ‘pathname’. L’operazione ha successo solo se
 *  l’owner della lock è il processo che ha richiesto l’operazione, altrimenti
 *  l’operazione termina con errore
 * 
 *   \param pathname nome del file
 *
 *   \retval 0 se successo
 *   \retval -1 se errore, errno settato opportunamente
 */
int unlockFile(const char* pathname);


/** Richiesta di chiusura del file puntato da ‘pathname’
 *  (Eventuali operazioni sul file dopo la closeFile falliscono)
 * 
 *   \param pathname nome del file
 *
 *   \retval 0 se successo
 *   \retval -1 se errore, errno settato opportunamente
 */
int closeFile(const char* pathname);


/** Rimuove il file cancellandolo dal file storage server.
 *  L’operazione fallisce se il file non è in stato locked, o è in stato locked 
 *  da parte di un processo client diverso da chi effettua la removeFile
 * 
 *   \param pathname nome del file
 *
 *   \retval 0 se successo
 *   \retval -1 se errore, errno settato opportunamente
 */
int removeFile(const char* pathname);

#endif
