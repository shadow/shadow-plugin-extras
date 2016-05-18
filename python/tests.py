import logging


log = logging.getLogger(__name__)


class LogHandler(logging.Handler):

    def __init__(self, level=logging.NOTSET):
        # This is necessary because shadow handles the levels, not us
        log.setLevel(logging.DEBUG)
        import shadow_python
        self._logger = shadow_python.Logger()
        logging.Handler.__init__(self, level)

    def emit(self, record):
        severity_map = {
            logging.CRITICAL: 0,
            logging.ERROR: 1,
            logging.WARNING: 2,
            logging.INFO: 3,
            logging.DEBUG: 4,
        }
        return self._logger.write(
            severity_map[record.levelno], record.msg)

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


def get_handle(shadow_logging=True):
    if shadow_logging:
        handler = LogHandler()
        log.addHandler(handler)
        log.propagate = False
    return TestClass()


if __name__ == '__main__':
    instance = get_handle(shadow_logging=False)
    print("Running...")
    try:
        while True:
            if instance.process(10):
                break
    finally:
        instance.finish()
