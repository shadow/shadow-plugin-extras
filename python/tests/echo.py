from __future__ import unicode_literals
import logging
import select
import errno
import socket
import argparse
import string
import random


log = logging.getLogger(__name__)


class EPollMaster(dict):

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
        dict.__init__(self)

    def process(self, timeout):
        try:
            events = self.epoll.poll(timeout)
        except IOError as e:
            if e.errno == errno.EINTR:
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
            log.debug(
                'Updating connection %d with the following status: '
                'wants read: %d, wants write: %d, is open: %d',
                fd, wr, ww, isopen)
            if not isopen:
                del self[fd]
                continue
            self.epoll.modify(fd, self.EVENT_MASK[wr, ww])
        return 1 if not self.connections else 0

    def finish(self):
        log.debug('Finishing...')
        fds = list(self.connections.keys())
        for fd in fds:
            del self[fd]

    def __setitem__(self, fd, c):
        wr, ww, isopen = c.get_status()
        log.debug(
            'Adding new connection with the following status: wants read: %d, '
            'wants write: %d, is open: %d', wr, ww, isopen)
        if not isopen:
            return
        self.connections[fd] = c
        mask = self.EVENT_MASK[(wr, ww)]
        self.epoll.register(fd, mask)

    def __delitem__(self, fd):
        self.epoll.unregister(fd)
        self.connections[fd].close()
        del self.connections[fd]


class BaseConnection(object):

    def __init__(self):
        self._open = 1

    def close(self):
        try:
            fno = self._socket.fileno()
            if fno != -1:
                log.info(
                    'Closing connection on socket %d', fno)
                self._socket.close()
        except socket.error as e:
            # socket already closed
            if e.errno == errno.EBADF:
                pass
        self._open = 0

    def get_status(self):
        raise NotImplementedError

    def process(self, read, write, err):
        raise NotImplementedError


class ClientConnection(BaseConnection):

    def __init__(self, conn, addr):
        self._socket = conn
        self.ip, self.port = addr
        self._buf = b''
        BaseConnection.__init__(self)

    def process(self, read, write, err):
        if read:
            new_data = self._socket.recv(4096)
            if not new_data:
                self.close()
            else:
                logging.info(
                    'server socket %d read %d bytes',
                    self._socket.fileno(), len(new_data),
                )
                self._buf += new_data
        if write:
            bytes_sent = self._socket.send(self._buf)
            if bytes_sent == 0:
                log.info(
                    'Could not send any data on socket %d, closing connection',
                    self._socket.fileno())
                self.close()
            else:
                logging.info(
                    'server socket %d wrote %d bytes',
                    self._socket.fileno(), bytes_sent,
                )
                self._buf = self._buf[bytes_sent:]
        if err:
            logging.info(
                'Connection %d closed by remote', self._socket.fileno())
            self.close()
        return self.get_status()

    def get_status(self):
        return 1, 1 if self._buf else 0, self._open


class Server(BaseConnection):

    def __init__(self, ip, port, master):
        self._socket = socket.socket()
        self._socket.setblocking(0)
        self._socket.bind((ip, port))
        self._socket.listen(10)
        BaseConnection.__init__(self)
        self._master = master
        self._master[self._socket.fileno()] = self

    def process(self, read, write, err):
        if read:
            try:
                conn, addr = self._socket.accept()
            except:
                logging.warn('error accepting socket')
                return self.get_status()
            logging.debug('Accepted new connection from %s:%d' % addr)
            conn.setblocking(0)
            self._master[conn.fileno()] = ClientConnection(conn, addr)
        if err:
            self.close()
        return self.get_status()

    def get_status(self):
        return 1, 0, self._open


def rand_string(num_bytes):
    chars = string.ascii_uppercase + string.digits
    rand_chars = (random.choice(chars).encode('ascii')
                  for _ in range(num_bytes))
    return b''.join(rand_chars)


class Client(BaseConnection):

    def __init__(self, ip, port, master, byte_count):
        self._socket = socket.socket()
        self._socket.setblocking(0)
        try:
            self._socket.connect((ip, port))
        except IOError as e:
            # Nonblocking + connect, this is expected
            if e.errno == errno.EINPROGRESS:
                pass
        BaseConnection.__init__(self)
        self._data_sent = 0
        self._data_size = byte_count
        self._outbuf = rand_string(byte_count)
        self._outdata = self._outbuf
        self._inbuf = b''
        self._master = master
        self._master[self._socket.fileno()] = self

    def process(self, read, write, err):
        if read:
            log.debug('trying to read socket %d', self._socket.fileno())
            new_data = self._socket.recv(4096)
            if not new_data:
                self.close()
            else:
                logging.info(
                    'server socket %d read %d bytes',
                    self._socket.fileno(), len(new_data),
                )
                self._inbuf += new_data
                if len(self._inbuf) == len(self._outdata):
                    if self._inbuf != self._outdata:
                        log.info('inconsistent echo received!')
                    else:
                        log.info('consistent echo received!')
                    self.close()
                else:
                    log.info(
                        'echo progress: %d of %d bytes',
                        len(self._inbuf), len(self._outbuf)
                    )
        if write:
            log.debug('trying to write socket %d', self._socket.fileno())
            bytes_sent = self._socket.send(self._outbuf)
            log.debug(
                'client socket %d wrote %d bytes',
                self._socket.fileno(), bytes_sent
            )
            self._outbuf = self._outbuf[bytes_sent:]
        if err:
            self.close()
        return self.get_status()

    def get_status(self):
        want_read = 1 if len(self._inbuf) != self._data_size else 0
        want_write = 1 if self._outbuf else 0
        return want_read, want_write, self._open


class LogHandler(logging.Handler):

    def __init__(self, level=logging.NOTSET):
        # This is necessary because shadow handles the levels, not us
        log.setLevel(logging.DEBUG)
        logging.Handler.__init__(self, level)

    def emit(self, record):
        import shadow_python
        msg = self.format(record)
        return shadow_python.write(record.levelno, msg)

    def flush(self):
        pass


class TestClass(object):

    def __init__(self):
        self._done = False

    def process(self, timeout=0):
        self.finish()
        return self._done

    def finish(self):
        self._done = True


def get_server(args):
    master = EPollMaster()
    Server(args.ip, args.port, master)
    return master


def get_client(args):
    master = EPollMaster()
    Client(args.ip, args.port, master, args.data)
    return master


def get_handle(shadow_logging=True):
    if shadow_logging:
        handler = LogHandler()
        log.addHandler(handler)
        log.propagate = False

    parser = argparse.ArgumentParser()
    parser.add_argument('--port', '-p', default=1337, type=int)
    subparsers = parser.add_subparsers()

    client_parser = subparsers.add_parser('client')
    client_parser.add_argument(
        '--data', default=20000,
        help='The amount of bytes to send')
    client_parser.add_argument('ip')
    client_parser.set_defaults(func=get_client)

    server_parser = subparsers.add_parser('server')
    server_parser.add_argument('ip')
    server_parser.set_defaults(func=get_server)

    args = parser.parse_args()
    return args.func(args)


if __name__ == '__main__':
    import sys
    logging.basicConfig(
        level=logging.DEBUG,
        stream=sys.stderr,
    )
    instance = get_handle(shadow_logging=False)
    log.info('Started module in standalone mode')
    try:
        while True:
            if instance.process(10):
                break
    finally:
        instance.finish()
