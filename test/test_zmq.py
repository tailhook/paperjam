from . import base
from time import sleep


class Any(object):
    def __eq__(self, other):
        return True


class TestZmq(base.Test):

    COMMAND_LINE = ['$PAPERJAM', '-c', '$CONFIGDIR/zmq.yaml']

    def testReq(self):
        req = self.socket('REQ', 'ipc:///tmp/paperjam-rep')
        rep = self.socket('REP', 'ipc:///tmp/paperjam-req')
        sleep(0.4)  # wait until sockets connect
        req.send_multipart([b'hello', b'world'])
        self.assertEqual(rep.recv_multipart(),
            [b'hello', b'world'])
        rep.send_multipart([b'world', b'hello'])
        self.assertEqual(req.recv_multipart(),
            [b'world', b'hello'])

    def testPipeline(self):
        push = self.socket('PUSH', 'ipc:///tmp/paperjam-pull')
        pull = self.socket('PULL', 'ipc:///tmp/paperjam-push')
        sleep(0.4)  # wait until sockets connect
        push.send_multipart([b'hello', b'world'])
        self.assertEqual(pull.recv_multipart(),
            [b'hello', b'world'])
        push.send_multipart([b'world', b'hello'])
        self.assertEqual(pull.recv_multipart(),
            [b'world', b'hello'])

    def testDistrib(self):
        pub = self.socket('PUB', 'ipc:///tmp/paperjam-sub')
        sub = self.socket('SUB', 'ipc:///tmp/paperjam-pub')
        sub.subscribe(b'')
        sleep(0.4)  # wait until sockets connect
        pub.send_multipart([b'hello', b'world'])
        self.assertEqual(sub.recv_multipart(),
            [b'hello', b'world'])
        pub.send_multipart([b'world', b'hello'])
        self.assertEqual(sub.recv_multipart(),
            [b'world', b'hello'])

class TestZmqMon(TestZmq):

    COMMAND_LINE = ['$PAPERJAM', '-c', '$CONFIGDIR/zmqmon.yaml']

    # Usual tests with unconnected monitor socket also here

    def testMonReq(self):
        req = self.socket('REQ', 'ipc:///tmp/paperjam-rep')
        rep = self.socket('REP', 'ipc:///tmp/paperjam-req')
        mon = self.socket('PULL', 'ipc:///tmp/paperjam-reqrep-mon')
        sleep(0.4)  # wait until sockets connect
        req.send_multipart([b'hello', b'world'])
        self.assertEqual(mon.recv_multipart(),
            [b'in', Any(), b'', b'hello', b'world'])
        self.assertEqual(rep.recv_multipart(),
            [b'hello', b'world'])
        rep.send_multipart([b'world', b'hello'])
        self.assertEqual(mon.recv_multipart(),
            [b'out', Any(), b'', b'world', b'hello'])
        self.assertEqual(req.recv_multipart(),
            [b'world', b'hello'])

    def testMonPipeline(self):
        push = self.socket('PUSH', 'ipc:///tmp/paperjam-pull')
        pull = self.socket('PULL', 'ipc:///tmp/paperjam-push')
        mon = self.socket('PULL', 'ipc:///tmp/paperjam-pipeline-mon')
        sleep(0.4)  # wait until sockets connect
        push.send_multipart([b'hello', b'world'])
        self.assertEqual(mon.recv_multipart(),
            [b'out', b'hello', b'world'])
        self.assertEqual(pull.recv_multipart(),
            [b'hello', b'world'])
        push.send_multipart([b'world', b'hello'])
        self.assertEqual(mon.recv_multipart(),
            [b'out', b'world', b'hello'])
        self.assertEqual(pull.recv_multipart(),
            [b'world', b'hello'])

    def testMonDistrib(self):
        pub = self.socket('PUB', 'ipc:///tmp/paperjam-sub')
        sub = self.socket('SUB', 'ipc:///tmp/paperjam-pub')
        mon = self.socket('PULL', 'ipc:///tmp/paperjam-distrib-mon')
        sub.subscribe(b'')
        sleep(0.4)  # wait until sockets connect
        pub.send_multipart([b'hello', b'world'])
        self.assertEqual(mon.recv_multipart(),
            [b'out', b'hello', b'world'])
        self.assertEqual(sub.recv_multipart(),
            [b'hello', b'world'])
        pub.send_multipart([b'world', b'hello'])
        self.assertEqual(mon.recv_multipart(),
            [b'out', b'world', b'hello'])
        self.assertEqual(sub.recv_multipart(),
            [b'world', b'hello'])


class TestZmqXs(TestZmq):

    COMMAND_LINE = ['$PAPERJAM', '-c', '$CONFIGDIR/zmqxs.yaml']
