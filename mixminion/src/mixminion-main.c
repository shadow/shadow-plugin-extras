/*
 * See LICENSE for licensing information
 */

#include "mixminion.h"
#include <signal.h>
#include <time.h>

int STOP_MM = 0;


void handle_sigint(int sig) {
    STOP_MM = 1;
}


int main(int argc, char *argv[]) {
    struct timespec sleep = {0, 1000};
    Mixminion *m = mixminion_new(argc, argv, NULL);
    signal(SIGINT, handle_sigint);
    if(m == NULL)
        exit(1);
    while(!STOP_MM) {
        switch(mixminion_ready(m)) {
            case 0:
                nanosleep(&sleep, NULL);
                break;
            case 1:
                STOP_MM = 1;
                break;
            case -1:
                exit(2);
        }
    }
    mixminion_free(m);
}