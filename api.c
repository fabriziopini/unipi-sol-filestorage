#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/un.h> /* ind AF_UNIX */
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>


#include "common_def.h"
#include "common_funcs.h"
#include "api.h"

extern int pid;
extern bool p;

int csfd = -1;

int checkPathname(const char* pathname) {
    if (pathname == NULL || pathname[0] == '\0' ) {
        return -1;
    }

    return 0;
}

/** Legge dal socket 'fd' 'nfiles' files e li scrive in dirname(se != NULL)
 * 
 *   \param fd file descrittore socket
 *   \param nfiles numero di files da leggere (e poi scrivere)
 *   \param dirname directory in cui scrivere i file letti
 * 
 *   \retval 0 se successo
 *   \retval -1 se errore (errno settato opportunamente)
 */
int writeSocketFiles(int fd, int nfiles, char *dirname);


int openConnection(const char* sockname, int msec, const struct timespec abstime) {

    if (sockname == NULL) {
        errno = EINVAL;
        return -1;
    }

    struct sockaddr_un sa;
    strncpy(sa.sun_path, sockname, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;

    csfd = socket(AF_UNIX, SOCK_STREAM, 0);

    if(csfd < 0) {

        return -1;
    }

    int microseconds = msec *1000;
    struct timespec current_time;

    do {

        if (connect(csfd, (struct sockaddr*)&sa, sizeof(sa)) != -1) return 0;

        if (errno == ENOENT) usleep(microseconds);
        else {
            return -1;
        }

        if(clock_gettime(CLOCK_REALTIME, &current_time) == -1){
            close(csfd);
            return -1;
        }
    } while (current_time.tv_sec < abstime.tv_sec || current_time.tv_nsec < abstime.tv_nsec);

    errno =  ETIMEDOUT;
    return -1;
}

int closeConnection(const char* sockname) {
    int temp = csfd;
    csfd = -1;
    return close(temp);
}

int openFile(const char* pathname, int flags, const char* dirname) {

    if (checkPathname(pathname) != 0) {
        errno = EINVAL;
        return -1;
    }

    char abspathname[PATH_MAX];
    if (!realpath(pathname, abspathname)) {
        return -1;
    }

    msg_request_t *req = malloc(sizeof(msg_request_t));
    msg_response_t *res = malloc(sizeof(msg_response_t));

    memset(req, 0, sizeof(msg_request_t));
    memset(res, 0, sizeof(msg_response_t));

    req->op = OPENFILE;
    strncpy(req->pathname, abspathname, strlen(abspathname)+1);
    req->flag = flags;
    req->datalen = 0; //superfluo

    if(writen(csfd, req, sizeof(msg_request_t)) != sizeof(msg_request_t)) { 
        free(req);
        free(res);
        return -1;
    }

    if(readn(csfd, res, sizeof(msg_response_t)) != sizeof(msg_response_t)) {
        free(req);
        free(res);
        return -1;
    }

    if(res->result != 0) {
        errno = res->result;
        free(res);
        free(req);
        return -1;
    }

    int nfiles = res->datalen; //numero di files eliminati
    if (nfiles > 1 || (nfiles != 0 && (req->flag != O_CREATE_LOCK && req->flag != O_CREATE))) {
        free(res);
        free(req);
        errno = EAGAIN; //ha sbagliato qualcosa il server
        return -1;
    }

    free(req);

    free(res);

    return writeSocketFiles(csfd, nfiles, dirname);

}


int readFile(const char* pathname, void** buf, size_t* size) {

    if (checkPathname(pathname) != 0) {
        errno = EINVAL;
        return -1;
    }

    char abspathname[PATH_MAX];
    if (!realpath(pathname, abspathname)) {
        return -1;
    }

    msg_request_t *req = malloc(sizeof(msg_request_t));
    msg_response_t *res = malloc(sizeof(msg_response_t));

    memset(req, 0, sizeof(msg_request_t));
    memset(res, 0, sizeof(msg_response_t));

    req->op = READFILE;
    strncpy(req->pathname, abspathname, strlen(abspathname)+1);
    req->flag = O_NULL; //superfluo
    req->datalen = 0; //superfluo

    if (writen(csfd, req, sizeof(msg_request_t)) != sizeof(msg_request_t)) {
        free(req);
        free(res);
        return -1;
    }

    free(req);

    if (readn(csfd, res, sizeof(msg_response_t)) != sizeof(msg_response_t)) {
        free(res);
        return -1;
    }
    int result = res->result;

    printf("res->datalen: %d\n", res->datalen);

    if(result == 0) {  //esito positivo, mi verrà inviato il file
        char *buffer = malloc(res->datalen);
        if(readn(csfd, buffer, res->datalen) != res->datalen) {
            free(res);
            free(buffer);
            return -1;
        }

        *buf = buffer; 
        *size = res->datalen;

        free(res);

        return 0;
    }
    else {
        free(res);
        errno = result;
        return -1;
    }

}


int readNFiles(int N, const char* dirname) {

    msg_request_t *req = malloc(sizeof(msg_request_t));
    msg_response_t *res = malloc(sizeof(msg_response_t));

    memset(req, 0, sizeof(msg_request_t));
    memset(res, 0, sizeof(msg_response_t));

    req->op = READNFILES;
    req->flag = O_NULL; //superfluo
    req->datalen = N; // numero di files che richiediamo di leggere (datalen ha un significato diverso in altre operazioni)


    if (writen(csfd, req, sizeof(msg_request_t)) != sizeof(msg_request_t)) {
        free(req);
        free(res);
        return -1;
    }

    free(req);

    if (readn(csfd, res, sizeof(msg_response_t)) != sizeof(msg_response_t)) {
        free(res);
        return -1;
    }

    if(res->result != 0) {
        errno = res->result;
        free(res);
        return -1;
    }

    int nfiles = res->datalen; //numero di files letti

    free(res);

    int esitowrites = writeSocketFiles(csfd, nfiles, dirname);
    if (esitowrites != -1) {
        return nfiles;
    } else return -1;

}


int writeFile(const char* pathname, const char* dirname) {

    if (checkPathname(pathname) != 0) {
        errno = EINVAL;
        return -1;
    }

    char abspathname[PATH_MAX];
    if (!realpath(pathname, abspathname)) {
        return -1;
    }

    FILE *file;
    void *file_out = NULL;
    unsigned long file_size = 0;

    if ((file = fopen(abspathname,"r")) == NULL) {
        return -1;
    }

    if (file != NULL) {
        // Scopriamo la dimensione del file
        fseek(file, 0L, SEEK_END); 
        file_size = ftell(file); 
        rewind(file); 

        file_out = (void *)malloc(file_size);
        if (file_out == NULL) {
            fclose(file);
            return -1;
        } else {
            size_t bytes_read = fread(file_out, 1, file_size, file);
            if (bytes_read != file_size)
            {
                // Some kind of I/O error 
                fclose(file);
                free(file_out);
                errno = EIO;
                return -1;
            }
        }
        fclose(file);
    }


    msg_request_t *req = malloc(sizeof(msg_request_t));
    msg_response_t *res = malloc(sizeof(msg_response_t));

    memset(req, 0, sizeof(msg_request_t));
    memset(res, 0, sizeof(msg_response_t));

    req->op = WRITEFILE;
    strncpy(req->pathname, abspathname, strlen(abspathname)+1);
    req->flag = O_NULL; //superfluo
    req->datalen = file_size;

    if (writen(csfd, req, sizeof(msg_request_t)) != sizeof(msg_request_t)) {
        free(file_out);
        free(req);
        free(res);
        return -1;
    }

    free(req);

    if (writen(csfd, file_out, file_size) != file_size) {
        free(file_out); 
        free(res);
        return -1;
    }

    free(file_out);

    if (readn(csfd, res, sizeof(msg_response_t)) != sizeof(msg_response_t)) {
        free(res);
        return -1;
    }

    if(res->result != 0) {
        errno = res->result;
        free(res);
        return -1;
    }

    int nfiles = res->datalen; //numero di files eliminati

    free(res);
    return writeSocketFiles(csfd, nfiles, dirname);

}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname) {

    if (checkPathname(pathname) != 0) {
        errno = EINVAL;
        return -1;
    }

    char abspathname[PATH_MAX];
    if (!realpath(pathname, abspathname)) {
        return -1;
    }

    msg_request_t *req = malloc(sizeof(msg_request_t));
    msg_response_t *res = malloc(sizeof(msg_response_t));

    memset(req, 0, sizeof(msg_request_t));
    memset(res, 0, sizeof(msg_response_t));

    req->op = APPENDTOFILE;
    strncpy(req->pathname, abspathname, strlen(abspathname)+1);
    req->flag = O_NULL; //superfluo
    req->datalen = size;

    if (writen(csfd, req, sizeof(msg_request_t)) != sizeof(msg_request_t)) { 
        free(req);
        free(res);
        return -1;
    }

    free(req);

    if (writen(csfd, buf, size) != size) {
        free(res);
        return -1;
    }

    if (readn(csfd, res, sizeof(msg_response_t)) != sizeof(msg_response_t)) {
        free(res);
        return -1;
    }

    if(res->result != 0) {
        errno = res->result;
        free(res);
        return -1;
    }

    int nfiles = res->datalen; //numero di files eliminati

    free(res);
    return writeSocketFiles(csfd, nfiles, dirname);

}

int lockFile(const char* pathname) {

    if (checkPathname(pathname) != 0) {
        errno = EINVAL;
        return -1;
    }

    char abspathname[PATH_MAX];
    if (!realpath(pathname, abspathname)) {
        return -1;
    }

    msg_request_t *req = malloc(sizeof(msg_request_t));
    msg_response_t *res = malloc(sizeof(msg_response_t));

    memset(req, 0, sizeof(msg_request_t));
    memset(res, 0, sizeof(msg_response_t));

    req->op = LOCKFILE;
    strncpy(req->pathname, abspathname, strlen(abspathname)+1);
    req->flag = O_NULL; //superfluo
    req->datalen = 0; //superfluo

    if (writen(csfd, req, sizeof(msg_request_t)) != sizeof(msg_request_t)) {
        free(req);
        free(res);
        return -1;
    }

    free(req);

// mi blocco sulla read, fino a quando il server non mi dà il lock, o errore per qualche motivo
    if (read(csfd, res, sizeof(msg_response_t)) != sizeof(msg_response_t)) {
        free(res);
        return -1;
    }
    int result = res->result;
    
    free(res);

    if(result != 0) {
        errno = result;
        return -1;
    }

    return 0;
}

int unlockFile(const char* pathname) {

    if (checkPathname(pathname) != 0) {
        errno = EINVAL;
        return -1;
    }

    char abspathname[PATH_MAX];
    if (!realpath(pathname, abspathname)) {
        return -1;
    }

    msg_request_t *req = malloc(sizeof(msg_request_t));
    msg_response_t *res = malloc(sizeof(msg_response_t));

    memset(req, 0, sizeof(msg_request_t));
    memset(res, 0, sizeof(msg_response_t));

    req->op = UNLOCKFILE;
    strncpy(req->pathname, abspathname, strlen(abspathname)+1);
    req->flag = O_NULL; //superfluo
    req->datalen = 0; //superfluo

    if (writen(csfd, req, sizeof(msg_request_t)) != sizeof(msg_request_t)) {
        free(req);
        free(res);
        return -1;
    }

    free(req);

    if (readn(csfd, res, sizeof(msg_response_t)) != sizeof(msg_response_t)) {
        free(res);
        return -1;
    }
    int result = res->result;
    
    free(res);

    if(result != 0) {
        errno = result;
        return -1;
    }

    return 0;
}

int closeFile(const char* pathname) {

    if (checkPathname(pathname) != 0) {
        errno = EINVAL;
        return -1;
    }

    char abspathname[PATH_MAX];
    if (!realpath(pathname, abspathname)) {
        return -1;
    }

    msg_request_t *req = malloc(sizeof(msg_request_t));
    msg_response_t *res = malloc(sizeof(msg_response_t));

    memset(req, 0, sizeof(msg_request_t));
    memset(res, 0, sizeof(msg_response_t));

    req->op = CLOSEFILE;
    strncpy(req->pathname, abspathname, strlen(abspathname)+1);
    req->flag = O_NULL; //superfluo
    req->datalen = 0; //superfluo

    if (writen(csfd, req, sizeof(msg_request_t)) != sizeof(msg_request_t)) {
        free(req);
        free(res);
        return -1;
    }

    free(req);

    if (readn(csfd, res, sizeof(msg_response_t)) != sizeof(msg_response_t)) {
        free(res);
        return -1;
    }
    int result = res->result;
    
    free(res);

    if(result != 0) {
        errno = result;
        return -1;
    }

    return 0;
}

int removeFile(const char* pathname) {

    if (checkPathname(pathname) != 0) {
        errno = EINVAL;
        return -1;
    }
    
    char abspathname[PATH_MAX];
    if (!realpath(pathname, abspathname)) {
        return -1;
    }

    msg_request_t *req = malloc(sizeof(msg_request_t));
    msg_response_t *res = malloc(sizeof(msg_response_t));

    memset(req, 0, sizeof(msg_request_t));
    memset(res, 0, sizeof(msg_response_t));

    req->op = REMOVEFILE;
    strncpy(req->pathname, abspathname, strlen(abspathname)+1);
    req->flag = O_NULL; //superfluo
    req->datalen = 0; //superfluo

    if (writen(csfd, req, sizeof(msg_request_t)) != sizeof(msg_request_t)) {
        free(req);
        free(res);
        return -1;
    }

    free(req);

    if (readn(csfd, res, sizeof(msg_response_t)) != sizeof(msg_response_t)) {
        free(res);
        return -1;
    }
    int result = res->result;

    free(res);

    if (result != 0) {
        errno = result;
        return -1;
    }

    return 0;
}


int writeSocketFiles(int fd, int nfiles, char *dirname) {

    if (dirname != NULL && dirname[0] != '\0') {
        struct stat statbuf;
        if (stat(dirname, &statbuf) == 0) {
            if (!S_ISDIR(statbuf.st_mode)) {
                fprintf(stderr, "👦 %d: ❌ %s is not a directory\n", pid, dirname);
                dirname = NULL;
            } else { //dirname è una directory
                if (p && nfiles > 0) printf("👦 %d: Salvo in dirname %s, %d files\n", pid, dirname, nfiles);
            }
        } else {
            fprintf(stderr, "👦 %d: ❌ stat %s, ERRORE %s\n", pid, dirname, strerror(errno));
            dirname = NULL;
        }
    }

    msg_response_t res;
    memset(&res, '\0', sizeof(msg_response_t));


    int i = 0;
    while (i < nfiles) {
        memset(&res, 0, sizeof(msg_response_t));

        if (readn(csfd, &res, sizeof(msg_response_t)) != sizeof(msg_response_t)) {
            return -1;
        }

        if (res.result != 0) {
            errno = EAGAIN; // ha sbagliato qualcosa il server
            return -1;
        }

        char *buffer = malloc(res.datalen);
        if(readn(csfd, buffer, res.datalen) != res.datalen) {
            free(buffer);
            return -1;
        }

        if (p) printf("👦 %d: Salva in dirname %s, il file %s di len %d\n", pid, dirname, res.pathname, res.datalen);
        fflush(stdout);

        if (dirname != NULL && dirname[0] != '\0') { //dirname è una directory valida (controllato all'inizio della funzione)
            // creare e scrivere file nella directory
            if (createWriteInDir(res.pathname, buffer, res.datalen, dirname) != 0) {
                //errno dovrebbe settarlo creareWriteInDir
                free(buffer);
                return -1;
            }
        }

        free(buffer);
        i++;
    }

    return 0;
}
