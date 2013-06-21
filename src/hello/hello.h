#ifndef HELLO_H_
#define HELLO_H_

#include <stdlib.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <assert.h>

#include <shd-library.h>

typedef struct _Hello Hello;

Hello* hello_new(int argc, char* argv[], ShadowFunctionTable shadowlib);
void hello_free(Hello* h);
void hello_ready(Hello* h);

#endif /* HELLO_H_ */
