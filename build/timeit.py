import time


class timeit(object):
    def __init__(self, tag="timeit"):
        self.tag = tag

    def __enter__(self):
        self.ts = time.time()

    def __exit__(self, *args):
        self.te = time.time()
        print("<{}> cost {:.2f} ms".format(self.tag, (self.te - self.ts) * 1000))
        return False

    def __call__(self, method):
        def timed(*args, **kw):
            ts = time.time()
            result = method(*args, **kw)
            te = time.time()
            print("<{}> cost {:.2f} ms".format(method.__name__, (te - ts) * 1000))
            return result

        return timed
