#include "browser2.hpp"
#include <openssl/evp.h>

ShadowLogFunc logfn;
ShadowCreateCallbackFunc scheduleCallback;

/* my global structure to hold all variable, node-specific application state */
browser_t *browser;

/* create a new node using this plug-in */
static void browserplugin_new(int argc, char* argv[]) {
    logfn(SHADOW_LOG_LEVEL_INFO, __FUNCTION__, "create new browser node");
    browser = new browser_t();
    browser->start(argc, argv);
}

static void browserplugin_free() {
#if 0
    /* freeing can cause some crashing problems sometimes, so let's
     * not free. current we only free at the end of experiments, so
     * don't need to worry about memory leaks. */
    logfn(SHADOW_LOG_LEVEL_INFO, __FUNCTION__, "freeing browser node");
    browser->close();
    delete browser;
#endif
}

static void browserplugin_activate() {
//    logfn(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__, "activate plugin");
    /* tell libevent to check epoll and activate the ready sockets without blocking */
    browser->activate(false);
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

    browser_t::digest_algo_ = EVP_md5();

    /*
     * tell shadow which of our functions it can use to notify our plugin,
     * and allow it to track our state for each instance of this plugin
     */
    gboolean success = shadowlibFuncs->registerPlugin(&browserplugin_new, &browserplugin_free, &browserplugin_activate);
    if(success) {
        logfn(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__, "successfully registered browser plug-in state");
    } else {
        logfn(SHADOW_LOG_LEVEL_INFO, __FUNCTION__, "error registering browser plug-in state");
    }
}
