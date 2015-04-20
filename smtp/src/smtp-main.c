/*
 * See LICENSE for licensing information
 */

#include "smtp.h"
#include <signal.h>
#include <time.h>

int STOP_SMTP = 0;


void handle_sigint(int sig) {
    STOP_SMTP = 1;
}


int main(int argc, char *argv[]) {
    struct timespec sleep = {0, 1000};
    Smtp *s = smtp_new(argc, argv, NULL);
    signal(SIGINT, handle_sigint);
    if(s == NULL)
        exit(1);
    while(!STOP_SMTP) {
        switch(smtp_ready(s)) {
            case 0:
                nanosleep(&sleep, NULL);
                break;
            case 1:
                STOP_SMTP = 1;
                break;
            case -1:
                exit(2);
        }
    }
    smtp_free(s);
}