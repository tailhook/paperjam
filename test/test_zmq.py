from . import base


class Test(base.Test):

    def testReq(self):
        req = self.socket('REQ', 'ipc:///tmp/paperjam-req')
        rep = self.socket('REP', 'ipc:///tmp/paperjam-rep')
        req.send_multipart([b'hello', b'world'])
        self.assertEqual(rep.recv_multipart(),
            [b'hello', b'world'])
        rep.send_multipart([b'world', b'hello'])
        self.assertEqual(req.recv_multipart(),
            [b'world', b'hello'])
