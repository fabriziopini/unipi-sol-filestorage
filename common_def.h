#define MAX_PATH_LENGTH 256

typedef enum {
    O_NULL =        0,
    O_CREATE =      1,
    O_LOCK =        2,
    O_CREATE_LOCK = 3
} file_flag;

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
