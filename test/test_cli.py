from time import sleep

from . import base

class TestZmq(base.FullCliTest):

    def testReq(self):
        rep = self.pjutil('REP', 'ipc:///tmp/paperjam-req', 'world', 'hello')
        req = self.pjutil('REQ', 'ipc:///tmp/paperjam-rep', 'hello', 'world')
        self.assertEqual(req.read(),
            b'"world" "hello"\n')
        self.assertEqual(rep.read(),
            b'"hello" "world"\n')
        req = self.pjutil('REQ', 'ipc:///tmp/paperjam-rep', 'test1', 'test2')
        self.assertEqual(req.read(),
            b'"world" "hello"\n')
        self.assertEqual(rep.read(),
            b'"test1" "test2"\n')

    def testPipeline(self):
        pull = self.pjutil('PULL', 'ipc:///tmp/paperjam-push')
        self.pjutil('PUSH', 'ipc:///tmp/paperjam-pull', 'abc').wait()
        self.assertEqual(pull.read(),
            b'"abc"\n')
        self.pjutil('PUSH', 'ipc:///tmp/paperjam-pull', 'what', 'is').wait()
        self.assertEqual(pull.read(),
            b'"what" "is"\n')

    def testDistrib(self):
        sub1 = self.pjutil('SUB', 'ipc:///tmp/paperjam-pub')
        sub2 = self.pjutil('SUB', 'ipc:///tmp/paperjam-pub')
        sleep(0.4)
        self.pjutil('PUB', 'ipc:///tmp/paperjam-sub', 'hello', 'there').wait()
        self.assertEqual(sub1.read(),
            b'"hello" "there"\n')
        self.assertEqual(sub2.read(),
            b'"hello" "there"\n')
        self.pjutil('PUB', 'ipc:///tmp/paperjam-sub', 'msg', '2', '3').wait()
        self.assertEqual(sub1.read(),
            b'"msg" "2" "3"\n')
        self.assertEqual(sub2.read(),
            b'"msg" "2" "3"\n')

class TestSurvey(base.FullCliTest):
    lib = 'xs'
    check_env = ['HAVE_SURVEY']
    COMMAND_LINE = ['$PAPERJAM', '-c', '$CONFIGDIR/survey.yaml']

    def testSurvey(self):
        rep1 = self.pjutil('RESPONDENT',
            'ipc:///tmp/paperjam-surveyor', 'one')
        rep2 = self.pjutil('RESPONDENT',
            'ipc:///tmp/paperjam-surveyor', 'two')

        req = self.pjutil('SURVEYOR',
            'ipc:///tmp/paperjam-respondent', 'hello')
        self.assertEqual(rep1.read(), b'"hello"\n')
        self.assertEqual(rep2.read(), b'"hello"\n')
        sleep(0.1)
        self.assertIn(req.read(),
            (b'"one"\n"two"\n', b'"two"\n"one"\n'))

        req = self.pjutil('SURVEYOR',
            'ipc:///tmp/paperjam-respondent', 'hello2')
        self.assertEqual(rep1.read(), b'"hello2"\n')
        self.assertEqual(rep2.read(), b'"hello2"\n')
        sleep(0.1)
        self.assertIn(req.read(),
            (b'"one"\n"two"\n', b'"two"\n"one"\n'))


class TestZmq2(base.AliasCliTest, TestZmq):
    pass


class TestXs(TestZmq):
    lib = 'xs'
    check_env = ['HAVE_XS']
    COMMAND_LINE = ['$PAPERJAM', '-c', '$CONFIGDIR/xs.yaml']

class TestXs2(TestZmq2):
    lib = 'xs'
    check_env = ['HAVE_XS']
    COMMAND_LINE = ['$PAPERJAM', '-c', '$CONFIGDIR/xs.yaml']

class TestSurvey2(base.AliasCliTest, TestSurvey):
    lib = 'xs'
    check_env = ['HAVE_SURVEY']
    COMMAND_LINE = ['$PAPERJAM', '-c', '$CONFIGDIR/survey.yaml']
