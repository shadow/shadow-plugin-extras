#include <shd-library.h>

static void hello_new(int argc, char* argv[]) {
}

static void hello_free() {
}

static void hello_ready() {
}

void __shadow_plugin_init__(ShadowFunctionTable* shadowlibFuncs) {
	g_assert(shadowlibFuncs);

	/*
	 * we 'register' our function table, telling shadow which of our functions
	 * it can use to notify our plugin
	 */
	int success = shadowlibFuncs->registerPlugin(&hello_new, &hello_free, &hello_ready);

	/* we log through Shadow by using the log function it supplied to us */
	if(success) {
		shadowlibFuncs->log(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
				"successfully registered echo plug-in state");
	} else {
		shadowlibFuncs->log(G_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"error registering echo plug-in state");
	}
}
