
import select
import sys
import errno
import smtpd

DEBUGSTREAM = sys.stderr
NEWLINE = '\n'
EMPTYSTRING = ''
COMMASPACE = ', '


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
                del self[fd]
                continue
            self.epoll.modify(fd, self.EVENT_MASK[wr, ww])

    def __setitem__(self, fd, c):
        wr, ww, isopen = c.get_status()
        if not isopen:
            return
        self.connections[fd] = c
        mask = self.EVENT_MASK[(wr, ww)]
        self.epoll.register(fd, mask)

    def __delitem__(self, fd):
        self.epoll.unregister(fd)
        del self.connections[fd]


class SMTPChannel(smtpd.SMTPChannel):

    def process(self, r, w, x):
        if r:
            self.handle_read_event()
        if w:
            self.handle_write_event()
        if x:
            self.handle_close()
        return self.get_status()

    def get_status(self):
        return bool(self.readable()), bool(self.writable()), self._open

    def add_channel(self, *args, **kw):
        self._open = 1
        return smtpd.SMTPServer.add_channel(self, *args, **kw)

    def del_channel(self, *args, **kw):
        self._open = 0
        return smtpd.SMTPServer.del_channel(self, *args, **kw)


class SMTPServer(smtpd.SMTPServer):
    channel_class = SMTPChannel

    def __init__(self, args, master):
        localhost = args.pop(0)
        localport = int(args.pop(0))
        localaddr = (localhost, localport)
        # remotehost = args.pop(0)
        # remoteport = int(args.pop(0))
        # remoteaddr = (remotehost, remoteport)
        self._open = 0
        smtpd.SMTPServer.__init__(self, localaddr, None, map=master)
        # smtpd.SMTPServer.__init__(self, localaddr, remoteaddr, map=master)

    def get_status(self):
        return self.readable(), self.writable(), self._open

    def add_channel(self, *args, **kw):
        self._open = 1
        return smtpd.SMTPServer.add_channel(self, *args, **kw)

    def del_channel(self, *args, **kw):
        self._open = 0
        return smtpd.SMTPServer.del_channel(self, *args, **kw)

    def process(self, r, w, x):
        if r:
            self.handle_read_event()
        if w:
            self.handle_write_event()
        if x:
            self.handle_close()
        return self.get_status()


class SMTPHandle:

    def __init__(self, args):
        self._master = EPollMaster()
        self._done = False
        self._server = SMTPServer(args, self._master)

    def process(self, timeout=0):
        self._master.process(timeout)
        return not self._done

    def is_done(self):
        return self._done

    def finish(self):
        self._server.close()
        self._done = True


def get_handle():
    args = sys.argv[1:]
    return SMTPHandle(args)


if __name__ == '__main__':
    instance = get_handle()
    print("Running...")
    try:
        while not instance.is_done():
            instance.process(10)
    finally:
        instance.finish()
