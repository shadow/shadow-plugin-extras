/*
 * See LICENSE for licensing information
 */

#include "mixminion.h"
#include <signal.h>

int STOP_MM = 0;


void handle_sigint(int sig) {
    STOP_MM = 1;
}


int main(int argc, char *argv[]) {
    Mixminion *m = mixminion_new(argc, argv, NULL);
    signal(SIGINT, handle_sigint);
    if(m == NULL)
        exit(1);
    while(!STOP_MM) {
        if(mixminion_ready(m) != 0)
            exit(2);
        sleep(1);
    }
    mixminion_free(m);
}