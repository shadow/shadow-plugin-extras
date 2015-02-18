/*
 * See LICENSE for licensing information
 */

#ifndef MIXMINOION_H_
#define MIXMINOION_H_

/* Needs to be first */
#include <Python.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>

#include <shd-library.h>

struct _Mixminion {
    /* the function we use to log messages
     * needs level, functionname, and format */
    ShadowLogFunc slogf;

    /* The module from which we import our functions */
    PyObject *module;

    /* The server object on which we run */
    PyObject *server;

    /* The run function */
    PyObject *run_server_step;
    PyObject *run_server_step_args;

    /* Stop functio */
    PyObject *server_stop;
};
typedef struct _Mixminion Mixminion;

int main(int, char **);
Mixminion *mixminion_new(int, char **, ShadowLogFunc);
int mixminion_ready(Mixminion *);
void mixminion_free(Mixminion *);

#endif /* MIXMINOION_H_ */