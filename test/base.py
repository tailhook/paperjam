import unittest
import subprocess
import os
import os.path
import fcntl
from select import select

try:
    import zmq
except ImportError:
    has_pyzmq = False
else:
    has_pyzmq = True


class Wrapper(object):

    def __init__(self, sock):
        self.sock = sock
        sock.setsockopt(zmq.LINGER, 0)

    def send_multipart(self, data):
        self.sock.send_multipart(data, zmq.DONTWAIT)

    def recv_multipart(self):
        s, _, _ = zmq.select([self.sock], [], [], 1.0)
        if s != [self.sock]:
            raise AssertionError('Timeout')
        return self.sock.recv_multipart(zmq.DONTWAIT)

    def subscribe(self, data):
        self.sock.setsockopt(zmq.SUBSCRIBE, data)


class ProcessWrapper(object):

    def __init__(self, proc):
        self.proc = proc
        fcntl.fcntl(self.proc.stdout, fcntl.F_SETFL, os.O_NONBLOCK)

    def read(self):
        r, w, x = select([self.proc.stdout], [], [], 1.0)
        if not r:
            raise AssertionError("Timeout expired")
        return self.proc.stdout.read()

    def wait(self):
        return self.proc.wait()

    def terminate(self):
        if self.proc.poll() is None:
            self.proc.terminate()


class BaseTest(unittest.TestCase):

    check_env = []
    COMMAND_LINE = ['$PAPERJAM', '-c', '$CONFIGDIR/zmq.yaml']

    def setUp(self):
        for i in self.check_env:
            if not os.environ.get(i):
                raise unittest.case.SkipTest("No "+i)
        os.environ.setdefault("PAPERJAM", './build/paperjam')
        os.environ.setdefault("PJMONITOR", './build/pjmonitor')
        os.environ.setdefault("PJUTIL", './build/pjutil')
        os.environ.setdefault("CONFIGDIR", './test')
        self.proc = subprocess.Popen([os.path.expandvars(arg)
                                      for arg in self.COMMAND_LINE])
        self.addCleanup(self.proc.terminate)


class PyzmqTest(BaseTest):

    def setUp(self):
        super().setUp()
        if not has_pyzmq:
            raise unittest.case.SkipTest("No pyzmq")
        self.ctx = zmq.Context(1)
        self.addCleanup(self.ctx.term)

    def socket(self, kind, addr):
        res = self.ctx.socket(getattr(zmq, kind.upper()))
        self.addCleanup(res.close)
        res.connect(addr)
        return Wrapper(res)


class FullCliTest(BaseTest):
    lib = 'zmq'

    def pjutil(self, type, addr, *args):
        sub = subprocess.Popen(
            ('pjutil', '--'+self.lib, '--'+type.lower(),
                       '--connect', addr) + args,
            executable=os.environ['PJUTIL'],
            stdout=subprocess.PIPE)
        self.addCleanup(sub.stdout.close)
        sub = ProcessWrapper(sub)
        self.addCleanup(sub.terminate)
        return sub


class AliasCliTest(BaseTest):
    lib = 'zmq'

    def pjutil(self, type, addr, *args):
        sub = subprocess.Popen(
            (self.lib + type.lower(), '--connect', addr) + args,
            executable=os.environ['PJUTIL'],
            stdout=subprocess.PIPE)
        self.addCleanup(sub.stdout.close)
        sub = ProcessWrapper(sub)
        self.addCleanup(sub.terminate)
        return sub
