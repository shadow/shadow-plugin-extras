#include <pthread.h>
#include <unistd.h>

#include "shd-utility.h"

int shouldForwardToLibC();
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
           void *(*start_routine) (void *), void *arg) {
    utility_assert(shouldForwardToLibC() && "pthread_create was called from plugin");
    return 0;
}

pid_t fork() {
    utility_assert(shouldForwardToLibC() && "fork was called from plugin");
    return 0;
}