import sys


sys.path[0:0] = ['/home/javex/.shadow/lib/python2.7/site-packages']


import os
from mixminion.server.ServerMain import (
    configFromServerArgs, _SERVER_START_USAGE,
    checkHomedirVersion, _QUIET_OPT, _ECHO_OPT, LOG, UIError,
    EventStats, installSIGCHLDHandler, installSignalHandlers,
    MixminionServer)
import mixminion.Common


def get_server_handle(cmd, args):
    config = configFromServerArgs(cmd, args, _SERVER_START_USAGE)
    checkHomedirVersion(config)
    quiet = _QUIET_OPT and not _ECHO_OPT
    try:
        # Configure the log, but delay disabling stderr until the last
        # possible minute; we want to keep echoing to the terminal until
        # the main loop starts.
        mixminion.Common.LOG.configure(config, keepStderr=(not quiet))
        LOG.debug("Configuring server")
    except UIError:
        raise
    except:
        info = sys.exc_info()
        LOG.fatal_exc(info, "Exception while configuring server")
        LOG.fatal("Shutting down because of exception: %s", info[0])
        sys.exit(1)

    os.umask(0000)

    # Configure event log
    try:
        EventStats.configureLog(config)
    except UIError:
        raise
    except:
        LOG.fatal_exc(sys.exc_info(), "")
        os._exit(0)

    installSIGCHLDHandler()
    installSignalHandlers()

    try:
        mixminion.Common.configureShredCommand(config)
        mixminion.Common.configureFileParanoia(config)
        mixminion.Crypto.init_crypto(config)

        server = MixminionServer(config)
        server.prepare_run()
    except UIError:
        raise
    except:
        info = sys.exc_info()
        LOG.fatal_exc(info, "Exception while configuring server")
        LOG.fatal("Shutting down because of exception: %s", info[0])
        sys.exit(1)
    LOG.info("Starting server: Mixminion %s", mixminion.__version__)
    return server


def run_server_step(server):
    try:
        # We keep the console log open as long as possible so we can catch
        # more errors.
        LOG.debug("Running step")
        server.run_step()
    except:
        info = sys.exc_info()
        LOG.fatal_exc(info, "Exception while running server")
        LOG.fatal("Shutting down because of exception: %s", info[0])
        server_stop(server)
        raise


def server_stop(server):
    LOG.info("Server shutting down")
    server.close()
    LOG.info("Server is shut down")
    LOG.close()
