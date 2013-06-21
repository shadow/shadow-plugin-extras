
#include "hello.h"

struct _Hello {
	ShadowFunctionTable shadowlib;
};

Hello* hello_new(int argc, char* argv[], ShadowFunctionTable shadowlib) {
	Hello* h = calloc(1, sizeof(Hello));

	h->shadowlib = shadowlib;

	return h;
}

void hello_free(Hello* h) {
	free(h);
}

void hello_ready(Hello* h) {

}
