#include <string.h>

#include "hello.h"

static void _mylog(int level, const char* functionName, char* format, ...) {
	va_list variableArguments;
	va_start(variableArguments, format);
	vprintf(format, variableArguments);
	va_end(variableArguments);
}
#define mylog(...) _mylog(0, __FUNCTION__, __VA_ARGS__)

/* our hello code only relies on the log part of shadowlib */
ShadowFunctionTable shadowlibReplacement = {NULL, _mylog, NULL, NULL, NULL};

gint main(gint argc, gchar *argv[]) {
	mylog("Starting hello program");
	Hello* helloState = hello_new(argc, argv, shadowlibReplacement);

	/* do an epoll on the hello descriptors, so we know when
	 * we can wait on any of them without blocking.
	 */
	int epolld = 0;
	if((epolld = epoll_create(1)) == -1) {
		mylog("Error in epoll_create");
		return -1;
	} else {
//		if(server) {
//			struct epoll_event ev;
//			ev.events = EPOLLIN;
//			ev.data.fd = server->epolld;
//			if(epoll_ctl(epolld, EPOLL_CTL_ADD, server->epolld, &ev) == -1) {
//				mylog("Error in epoll_ctl");
//				return -1;
//			}
//		}
	}

	/* main loop - when the client/server epoll fds are ready, activate them */
	while(1) {
		struct epoll_event events[10];
		int nfds = epoll_wait(epolld, events, 10, -1);
		if(nfds == -1) {
			mylog("error in epoll_wait");
		}

		for(int i = 0; i < nfds; i++) {
		}
	}

	return 0;
}
