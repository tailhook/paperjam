import unittest
import subprocess
import os

import zmq


class Wrapper(object):

    def __init__(self, sock):
        self.sock = sock

    def send_multipart(self, data):
        self.sock.send_multipart(data, zmq.DONTWAIT)

    def recv_multipart(self):
        s, _, _ = zmq.select([self.sock], [], [], 1000)
        if s != [self.sock]:
            raise AssertionError('Timeout')
        return self.sock.recv_multipart(zmq.DONTWAIT)


class Test(unittest.TestCase):

    def setUp(self):
        binary = os.environ.get("PAPERJAM", './build/paperjam')
        configdir = os.environ.get("CONFIGDIR", './test')
        self.proc = subprocess.Popen([binary, '-c',  configdir + '/zmq.yaml'])
        print("COMMANDLINE", binary, '-c',  configdir + '/zmq.yaml')
        self.addCleanup(self.proc.terminate)
        self.ctx = zmq.Context(1)
        self.addCleanup(self.ctx.term)

    def socket(self, kind, addr):
        res = self.ctx.socket(getattr(zmq, kind.upper()))
        res.connect(addr)
        self.addCleanup(res.close)
        return Wrapper(res)
