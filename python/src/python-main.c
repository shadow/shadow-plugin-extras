/*
 * See LICENSE for licensing information
 */

#include "python-plugin.h"
#include <signal.h>
#include <time.h>

int STOP_INTERPRETER = 0;


void handle_sigint(int sig) {
    fprintf(stderr, "Handling SIGINT\n");
    STOP_INTERPRETER = 1;
}


int main(int argc, char *argv[]) {
    struct timespec sleep = {0, 100000};
    python_data *instance = python_new(argc, argv);
    signal(SIGINT, handle_sigint);
    if(instance == NULL)
        exit(1);
    while(!STOP_INTERPRETER) {
        switch(python_ready(instance)) {
            case 0:
                nanosleep(&sleep, NULL);
                break;
            case 1:
                STOP_INTERPRETER = 1;
                break;
            case -1:
                exit(2);
        }
    }
    python_free(instance);
}