import unittest
import subprocess
import os
import os.path

import zmq


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


class Test(unittest.TestCase):

    COMMAND_LINE = ['$PAPERJAM', '-c', '$CONFIGDIR/zmq.yaml']

    def setUp(self):
        os.environ.setdefault("PAPERJAM", './build/paperjam')
        os.environ.setdefault("CONFIGDIR", './test')
        self.proc = subprocess.Popen([os.path.expandvars(arg)
                                      for arg in self.COMMAND_LINE])
        self.addCleanup(self.proc.terminate)
        self.ctx = zmq.Context(1)
        self.addCleanup(self.ctx.term)

    def socket(self, kind, addr):
        res = self.ctx.socket(getattr(zmq, kind.upper()))
        self.addCleanup(res.close)
        res.connect(addr)
        return Wrapper(res)
