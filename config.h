#ifndef COMMON_DEF_H
#include "common_def.h"
#endif

#if !defined(CONFIG_H_)
#define CONFIG_H_

#define CONFIG_LINE_BUFFER_SIZE 128 //lunghezza massima di una riga del file di configurazione
#define MAX_CONFIG_VARIABLE_LEN 32 //lunghezza massima del nome di una variabile nel file di configurazione

// struttura contenente le info di configurazione per il server
struct config_struct {
    int             num_workers; 			        // numero di workers (config)
    char 	        sockname[UNIX_PATH_MAX]; 	    // socket file
    int             limit_num_files;	            // numero limite di file nello storage (config)
    unsigned long   storage_capacity;	            // dimensione dello storage in bytes (config)
    int             v;                              // livello di verbosità (printLevel)
};

/** Legge il file di configurazione e imposta le variabili di 'config' (variabile globale extern di tipo struct config_struct) nella struttura
 *
 *   \param config_filename puntatore alla string del filename
 * 
 */
void read_config_file(char* config_filename);

#endif
