#ifndef MESSAGE_H
#define MESSAGE_H

#define MAX_PATH_LENGTH 256

typedef enum {
    OPENFILE = 1,
    READFILE = 2,
    WRITEFILE = 3,
    APPENDTOFILE = 4,
    LOCKFILE = 5,
    UNLOCKFILE = 6,
    CLOSEFILE = 7,
    REMOVEFILE = 8

} op_t;

typedef struct {
    op_t            op;
    char            pathname[MAX_PATH_LENGTH];
    int             flag; //TODO file_flag al posto di int?
    int             datalen;
} msg_request_t;

typedef struct {
    int result; // 0:success, >0: errno
    int datalen;
} msg_response_t;

/* VECCHIA VERSIONE
typedef struct {
    int     len;
    char    *buf;
} message_data_t;

/**
 *  @struct messaggio
 *  @brief tipo del messaggio 
 *
 *  @var hdr header
 *  @var data dati
 */
/*
typedef struct {
    op_t            op;
    message_data_t  data;
} message_t;


// openFile VERSIONE URL "stringona"
    char path_key[] = "PATHNAME=";
    char flag_key[] = "FLAG=";
    char flagtext[10];
    sprintf(flagtext, "%d", flags);

    message_t *msg = malloc(sizeof(message_t));
    msg->op = OPENFILE;

    msg->data.len = strlen(path_key) + strlen(pathname) + 1 + strlen(flag_key) + strlen(flagtext) + 1;
    msg->data.buf = malloc(msg->data.len);

    strncpy(msg->data.buf, path_key, strlen(path_key) + 1); 
    strcat(msg->data.buf, pathname); // strncat meglio
    strcat(msg->data.buf, "&");
    strcat(msg->data.buf, flag_key);
    strcat(msg->data.buf, flagtext);

    printf("buf: %s\n", msg->data.buf);

    write(csfd, &(msg->op), sizeof(msg->op));
    write(csfd, &(msg->data.buf), msg->data.len;

*/

#endif
