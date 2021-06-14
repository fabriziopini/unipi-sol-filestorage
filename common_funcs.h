#ifndef COMMON_FUNCS_H
#define COMMON_FUNCS_H

#include <sys/types.h>

/* Read "n" bytes from a descriptor */
ssize_t readn(int fd, void *ptr, size_t n);

/* Write "n" bytes to a descriptor */
ssize_t writen(int fd, void *ptr, size_t n);

/** Creo (ricavando il nome da 'pathname') e scrivo il file di contenuto 'buf' in 'dirname'
 * 
 *   \param pathname path assoluto del file (è stato salvato così sul server)
 *   \param buf buffer contenente i dati da scrivere nel file
 *   \param size dimensione del buffer
 *   \param dirname directory dove scrivere il file
 * 
 *   \retval
 * 
 */
int createWriteInDir (char *pathname, void *buf, size_t size, char *dirname);

#endif
