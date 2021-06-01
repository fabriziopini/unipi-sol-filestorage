#ifndef COMMON_FUNCS_H
#define COMMON_FUNCS_H

#include <sys/types.h>

/* Read "n" bytes from a descriptor */
ssize_t readn(int fd, void *ptr, size_t n);

/* Write "n" bytes to a descriptor */
ssize_t writen(int fd, void *ptr, size_t n);

/**
 * Creo e scrivo il file di contenuto buf in dirname
 * 
 * \param pathname path assoluto del file (è stato salvato così sul server)
 * 
 * 
 */
int createWriteInDir (char *pathname, void *buf, size_t size, char *dirname);

#endif
