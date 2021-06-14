#include <pthread.h>

#include "util.h"
#include "intWithLock.h"

IntWithLock_t *initIntWithLock() {
    IntWithLock_t *iwl = malloc(sizeof(IntWithLock_t));

    if (!iwl) return NULL;

    memset(iwl, '\0', sizeof(IntWithLock_t));     

    iwl->total_clients = 0;

    if (pthread_mutex_init(&(iwl->tc_lock), NULL) != 0) {
	    perror("🤖  SERVER: mutex init");
	    return NULL;
    }

    return iwl;
}

void deleteIntWithLock(IntWithLock_t *iwl) {
    if (iwl == NULL) return;

    if (&(iwl->tc_lock))  pthread_mutex_destroy(&(iwl->tc_lock));

    free(iwl);
}

void addClient(IntWithLock_t *iwl){
    LOCK(&(iwl->tc_lock));
    iwl->total_clients++;
    UNLOCK(&(iwl->tc_lock));
}

void deleteClient(IntWithLock_t *iwl){
    LOCK(&(iwl->tc_lock));
    iwl->total_clients--;
    UNLOCK(&(iwl->tc_lock));
}

int checkTotalClients(IntWithLock_t *iwl){
    LOCK(&(iwl->tc_lock));
    int temp = iwl->total_clients;
    UNLOCK(&(iwl->tc_lock));
    return temp;
}
