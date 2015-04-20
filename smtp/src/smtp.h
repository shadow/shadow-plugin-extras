/*
 * See LICENSE for licensing information
 */

#ifndef SMTP_H_
#define SMTP_H_

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
#include <locale.h>

#include <shd-library.h>

struct _Smtp {
    /* the function we use to log messages
     * needs level, functionname, and format */
    ShadowLogFunc slogf;

    /* The module from which we import our functions */
    PyObject *module;

    PyObject *handle; /* Returned on start */
    PyObject *process; /* When data is ready */
    PyObject *finish; /* Shutdown */
};
typedef struct _Smtp Smtp;

int main(int, char **);
Smtp *smtp_new(int, char **, ShadowLogFunc);
int smtp_ready(Smtp *);
void smtp_free(Smtp *);

#endif /* SMTP_H_ */