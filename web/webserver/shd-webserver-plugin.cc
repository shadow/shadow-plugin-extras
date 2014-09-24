#include "webserver.hpp"

ShadowLogFunc logfn;
ShadowCreateCallbackFunc scheduleCallback;

/* my global structure to hold all variable, node-specific application state */
webserver_t *webserver;

/* create a new node using this plug-in */
static void webserverplugin_new(int argc, char* argv[]) {
    logfn(SHADOW_LOG_LEVEL_INFO, __FUNCTION__, "create new webserver node");
    webserver = new webserver_t();
    webserver->start(argc, argv);
}

static void webserverplugin_free() {
#if 0
    /* freeing can cause some crashing problems sometimes, so let's
     * not free. current we only free at the end of experiments, so
     * don't need to worry about memory leaks. */
    logfn(SHADOW_LOG_LEVEL_INFO, __FUNCTION__, "freeing webserver node");
    delete webserver;
#endif
}

static void webserverplugin_activate() {
//    logfn(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__, "activate plugin");
    /* tell libevent to check epoll and activate the ready sockets without blocking */
    webserver->activate(false);
}

/* shadow calls this function for a one-time initialization
 *
 * !WARNING! dont malloc() (or g_new()) anything until filetransferplugin_new
 * unless that memory region is registered with shadow by giving a pointer to it.
 * its better to register as little as possible because everything that is
 * registered is copied on every shadow-to-plugin context switch.
 */
extern "C" void __shadow_plugin_init__(ShadowFunctionTable* shadowlibFuncs) {
    /* save the shadow functions we will use since it will be the same for all nodes */
    logfn = shadowlibFuncs->log;
    scheduleCallback = shadowlibFuncs->createCallback;

    /*
     * tell shadow which of our functions it can use to notify our plugin,
     * and allow it to track our state for each instance of this plugin
     */
    bool success = shadowlibFuncs->registerPlugin(&webserverplugin_new, &webserverplugin_free, &webserverplugin_activate);
    if(success) {
        logfn(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__, "successfully registered webserver plug-in state");
    } else {
        logfn(SHADOW_LOG_LEVEL_INFO, __FUNCTION__, "error registering webserver plug-in state");
    }
}
