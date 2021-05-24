#ifndef COMMON_FUNCS_H
#define COMMON_FUNCS_H

#include <sys/types.h>

/* Read "n" bytes from a descriptor */
ssize_t readn(int fd, void *ptr, size_t n);

/* Write "n" bytes to a descriptor */
ssize_t writen(int fd, void *ptr, size_t n);

#endif
