import sys


sys.path[0:0] = ['/home/javex/mnt/laptop/.shadow/lib/python2.7/site-packages']

import os
from mixminion.server.ServerMain import (
    configFromServerArgs, _SERVER_START_USAGE,
    checkHomedirVersion, _QUIET_OPT, _ECHO_OPT, LOG, UIError,
    EventStats, installSIGCHLDHandler, installSignalHandlers,
    MixminionServer)
import mixminion.Main
import mixminion.Common
from mixminion.directory.Directory import Directory
from mixminion.directory.ServerInbox import ServerQueuedException
from mixminion.Common import UIError
from mixminion.directory.DirMain import cmd_import, getDirectory, cmd_generate
from mixminion.ServerInfo import ServerInfo
import BaseHTTPServer
import SocketServer
import select
import socket
import errno
import urlparse
import StringIO


class EPollMaster(object):
    """Subclass of SelectAsyncServer that uses 'poll' where available.  This
       is more efficient, but less universal."""

    def __init__(self):
        self.epoll = select.epoll()
        self.connections = {}
        self.EVENT_MASK = {
            (0, 0): 0,
            (1, 0): select.EPOLLIN + select.EPOLLERR,
            (0, 1): select.EPOLLOUT + select.EPOLLERR,
            (0, 2): select.EPOLLOUT + select.EPOLLERR,
            (1, 1): select.EPOLLIN + select.EPOLLOUT + select.EPOLLERR,
            (1, 2): select.EPOLLIN + select.EPOLLOUT + select.EPOLLERR,
        }

    def process(self, timeout):
        try:
            events = self.epoll.poll(timeout)
        except IOError as e:
            if e[0] == errno.EINTR:
                return
            else:
                raise e

        if not events:
            return

        for fd, mask in events:
            c = self.connections[fd]
            wr, ww, isopen = c.process(
                mask & select.EPOLLIN,
                mask & select.EPOLLOUT,
                mask & (select.EPOLLERR | select.EPOLLHUP))
            if not isopen:
                self.remove(c, fd)
                continue
            print "Conn %s wr/ww: %d/%d" % (c, wr, ww)
            self.epoll.modify(fd, self.EVENT_MASK[wr, ww])

    def register(self, c):
        fd = c.fileno()
        wr, ww, isopen = c.get_status()
        if not isopen:
            return
        self.connections[fd] = c
        mask = self.EVENT_MASK[(wr, ww)]
        self.epoll.register(fd, mask)

    def remove(self, c, fd=None):
        if fd is None:
            fd = c.fileno()
        self.epoll.unregister(fd)
        del self.connections[fd]


class EPollHTTPServer(BaseHTTPServer.HTTPServer):

    def __init__(self, server_address, RequestHandlerClass, directory_path,
                 bind_and_activate=True):
        SocketServer.BaseServer.__init__(
            self, server_address, RequestHandlerClass)
        self.socket = socket.socket(self.address_family,
                                    self.socket_type)
        self.socket.setblocking(0)
        self.master = EPollMaster()
        self.directory_path = directory_path
        if bind_and_activate:
            self.server_bind()
            self.server_activate()

    def get_status(self):
        return self.open, 0, self.open

    def server_bind(self):
        BaseHTTPServer.HTTPServer.server_bind(self)
        self.open = 1
        self.master.register(self)

    def server_close(self):
        BaseHTTPServer.HTTPServer.server_close(self)
        self.open = 0

    def process_request(self, request, client_address):
        """Call finish_request.

        Overridden by ForkingMixIn and ThreadingMixIn.

        """
        self.finish_request(request, client_address)

    def process(self, r, w, x):
        if not (r & select.EPOLLIN):
            raise IOError("Invalid events: %s, %s, %s" % (r, w, x))
        self._handle_request_noblock()
        return self.open, 0, self.open


class EPollHTTPRequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):

    def __init__(self, socket, client_address, server):
        self.data = ''
        self.wfile = StringIO.StringIO()
        self.socket = socket
        self.client_address = client_address
        self.server = server
        self.open = 1
        self.want_read = 1
        self.want_write = 0
        self.close_connection = 0

        self.socket.setblocking(0)
        server.master.register(self)
        self._state_fn = self._read_header_data

    def finish(self):
        self.open = 0
        self.socket.close()
        self.wfile.close()

    def fileno(self):
        return self.socket.fileno()

    def _handle_headers(self):
        self.raw_requestline = self._raw_headers[0]
        if len(self.raw_requestline) > 65536:
            self.requestline = ''
            self.request_version = ''
            self.command = ''
            self.send_error(414)
            return
        if not self.raw_requestline:
            self.close_connection = 1
            return
        self.rfile = StringIO.StringIO('\r\n'.join(self._raw_headers[1:]))
        if not self.parse_request():
            # An error code has been sent, just exit
            return
        self.rfile.close()
        mname = 'do_' + self.command
        if not hasattr(self, mname):
            self.send_error(501, "Unsupported method (%r)" % self.command)
            return
        method = getattr(self, mname)
        self._state_fn = method
        method()

    def do_GET(self):
        self.want_read = 0
        if self.path == '/Directory.gz':
            dir_file = os.path.join(self.server.directory_path, 'Directory.gz')
            with open(dir_file) as f:
                dir_data = f.read()
            self.send_response(200)
            self.send_header("Content-type", "text/html")
            self.send_header('Content-Length', str(len(dir_data)))
            self.end_headers()
            self.wfile.write(dir_data)

    def do_POST(self, r=0):
        if r:
            self.data += self.socket.recv(4096)
        expected_length = int(self.headers['Content-Length'])
        if expected_length > len(self.data):
            self.want_read = 1
            return
        self.want_read = 0
        if self.path == '/minion-cgi/publish':
            os.environ['MINION_DIR_CONF'] = (
                '/home/javex/mnt/laptop/Studium/master-thesis/src/'
                'mixminion/data/directory/mixminion-dir.conf')
            self.send_response(200)
            self.send_header("Content-type", "text/plain")
            form = urlparse.parse_qs(self.data)
            if 'desc' not in form:
                self.send_error(500, "Nope")
                return
            desc = form['desc'][0]
            d = getDirectory()
            inbox = d.getInbox()

            address = "<%s:%s>" % self.socket.getpeername()
            output = ''
            try:
                os.umask(022)
                inbox.receiveServer(desc, address)
                output = "Status: 1\nMessage: Accepted."
            except UIError as e:
                output = "Status: 0\nMessage: %s" % e
            except ServerQueuedException as e:
                output = "Status: 1\nMessage: %s" % e
            finally:
                server = ServerInfo(string=desc)
                nick = server.getNickname()
                cmd_import([nick])
                cmd_generate([])
            self.send_header("Content-Length", str(len(output)))
            self.end_headers()
            self.wfile.write(output)

    def _read_header_data(self, r=0):
        new_data = self.socket.recv(65537)
        if not new_data:
            self.close_connection = 1
        self.data += new_data
        header_end = self.data.find('\r\n\r\n')
        if header_end != -1:
            self._raw_headers = self.data[:header_end].splitlines()
            self.data = self.data[header_end + 4:]
            self.want_read = 0
            self._handle_headers()
        else:
            self.want_read = 1

    def process(self, r, w, x):
        if r:
            self._state_fn(r)
        if w:
            data = self.wfile.getvalue()
            if data:
                sent = self.socket.send(data)
                self.wfile.close()
                remaining_data = data[sent:]
                self.wfile = StringIO.StringIO(remaining_data)
                if not remaining_data:
                    self.want_write = 0

        if not x and self.open and self.wfile.getvalue():
            self.want_write = 1

        # if x or self.close_connection and not self.want_write:
        #     self.finish()

        return self.want_read, self.want_write, self.open

    def get_status(self):
        return self.want_read, self.want_write, self.open


class MMClient(object):

    def __init__(self, args):
        args = [sys.argv[0]] + args
        self.args = args

    def process(self):
        mixminion.Main.main(self.args)
        self._done = True
        return not self._done

    def is_done(self):
        return self._done

    def finish(self):
        return


class MMServer(object):

    def __init__(self, args):
        self._server = None
        self._done = False

        cmd = args.pop(0)
        if cmd == 'start':
            self._start_server(cmd, args)
        else:
            mixminion.Main.main([cmd] + args, 1)
            self._done = True

    def _start_server(self, cmd, args):
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
        self._server = server

    def process(self, timeout=0):
        if not self._server:
            return
        try:
            # We keep the console log open as long as possible so we can catch
            # more errors.
            # LOG.debug("Running step")
            self._done = not self._server.run_step(timeout)
        except:
            info = sys.exc_info()
            LOG.fatal_exc(info, "Exception while running server")
            LOG.fatal("Shutting down because of exception: %s", info[0])
            raise
        return not self._done

    def is_done(self):
        return self._done

    def finish(self):
        if self._server:
            LOG.info("Server shutting down")
            self._server.close()
            LOG.info("Server is shut down")
            LOG.close()


class MMDirectory(object):

    def __init__(self, args):
        directory_path = args[0]
        self._server = EPollHTTPServer(
            ('localhost', 8080), EPollHTTPRequestHandler,
            directory_path)

    def process(self, timeout):
        self._server.master.process(timeout)

    def is_done(self):
        return not bool(self._server.open)

    def finish(self):
        self._server.server_close()


def get_handle():
    args = sys.argv[1:]
    type_ = args.pop(0)
    cls_map = {
        'client': MMClient,
        'server': MMServer,
        'directory': MMDirectory,
    }
    return cls_map[type_](args)


if __name__ == '__main__':
    instance = get_handle()
    print("Running...")
    try:
        while not instance.is_done():
            instance.process(10)
    finally:
        instance.finish()
