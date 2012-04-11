from . import base


class TestSingle(base.Test):

    def testReq(self):
        req = self.socket('REQ', 'ipc:///tmp/paperjam-rep')
        rep = self.socket('REP', 'ipc:///tmp/paperjam-req')
        req.send_multipart([b'hello', b'world'])
        self.assertEqual(rep.recv_multipart(),
            [b'hello', b'world'])
        rep.send_multipart([b'world', b'hello'])
        self.assertEqual(req.recv_multipart(),
            [b'world', b'hello'])

    def testPipeline(self):
        push = self.socket('PUSH', 'ipc:///tmp/paperjam-pull')
        pull = self.socket('PULL', 'ipc:///tmp/paperjam-push')
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
        pub.send_multipart([b'hello', b'world'])
        self.assertEqual(sub.recv_multipart(),
            [b'hello', b'world'])
        pub.send_multipart([b'world', b'hello'])
        self.assertEqual(sub.recv_multipart(),
            [b'world', b'hello'])
