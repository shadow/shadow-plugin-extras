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

#include <shd-library.h>

struct _python_data {
    /* the function we use to log messages
     * needs level, functionname, and format */
    ShadowLogFunc log;

    /* The used sub-interpreter */
    PyThreadState *interpreter;

    /* Our two logger instances */
    PyObject *stdout_logger;
    PyObject *stderr_logger;

    /* The module from which we import our functions */
    PyObject *module;

    PyObject *handle; /* Returned on start */
    PyObject *process; /* When data is ready */
    PyObject *finish; /* Shutdown */
};
typedef struct _python_data python_data;

int main(int, char **);
python_data *python_new(int, char **, ShadowLogFunc);
int python_ready(python_data *);
void python_free(python_data *);
PyObject* init_logger();

#endif /* MIXMINOION_H_ */