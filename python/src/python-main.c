/*
 * See LICENSE for licensing information
 */

#include "python-plugin.h"
#include <signal.h>
#include <time.h>

 static void _mylog(ShadowLogLevel level, const char* functionName, const char* format, ...) {
    va_list variableArguments;
    fprintf(stderr, "Shadow-Log: ");
    va_start(variableArguments, format);
    vfprintf(stderr, format, variableArguments);
    va_end(variableArguments);
    fprintf(stderr, "%s", "\n");
}

int STOP_INTERPRETER = 0;


void handle_sigint(int sig) {
    STOP_INTERPRETER = 1;
}


int main(int argc, char *argv[]) {
    struct timespec sleep = {0, 1000};
    python_data *instance = python_new(argc, argv, &_mylog);
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