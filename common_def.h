#ifndef COMMON_DEF_H
#define COMMON_DEF_H

#define UNIX_PATH_MAX 104 

typedef enum {
    O_NULL        = 0,
    O_CREATE      = 1,
    O_LOCK        = 2,
    O_CREATE_LOCK = 3
} file_flag;

typedef enum {
    NOTHING = 0,
    OPENFILE = 1,
    READFILE = 2,
    READNFILES = 3,
    WRITEFILE = 4,
    APPENDTOFILE = 5,
    LOCKFILE = 6,
    UNLOCKFILE = 7,
    CLOSEFILE = 8,
    REMOVEFILE = 9

} op_t;

typedef struct {
    op_t            op;
    char            pathname[UNIX_PATH_MAX];
    int             flag; //TODO file_flag al posto di int?
    int             datalen;
} msg_request_t;

typedef struct {
    int result; // 0:success, >0: errno
    char pathname[UNIX_PATH_MAX]; // nome del file (eventualmente)
    int datalen;
} msg_response_t;

#endif
