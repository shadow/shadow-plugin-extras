/*
 * See LICENSE for licensing information
 */

#ifndef SHADOWPYTHON_H_
#define SHADOWPYTHON_H_

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
#include <glib.h>

struct _python_data {
    /* The used sub-interpreter */
    PyThreadState *interpreter;

    /* The module from which we import our functions */
    PyObject *module;

    PyObject *handle; /* Returned on start */
    PyObject *process; /* When data is ready */
    PyObject *finish; /* Shutdown */
};
typedef struct _python_data python_data;

int main(int, char **);
python_data *python_new(int, char **);
int python_ready(python_data *);
void python_free(python_data *);
PyObject* init_logger();
void _py_log(GLogLevelFlags, const gchar*, const gchar*, ...);

#endif /* SHADOWPYTHON_H_ */