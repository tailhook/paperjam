#!/usr/bin/env python3
# -*- coding: utf-8 -*-
from waflib import Utils, Options, Errors
from waflib.Build import BuildContext
from waflib.Scripting import Dist
import subprocess
import os.path

APPNAME='paperjam'
if os.path.exists('.git'):
    VERSION=subprocess.getoutput('git describe').lstrip('v').replace('-', '_')
else:
    VERSION='0.1'

top = '.'
out = 'build'

def options(opt):
    opt.load('compiler_c')

def configure(conf):
    conf.load('compiler_c')
    conf.define('PAPERJAM_VERSION', VERSION)
    conf.check(header_name='zmq.h',
               define_name='HAVE_ZMQ',
               mandatory=False)
    conf.check(header_name='xs.h',
               define_name='HAVE_XS',
               mandatory=False)
    conf.check(lib='xs', uselib_store='XS', mandatory=False)
    conf.check(lib='zmq', uselib_store='XS', mandatory=False)
    if not conf.env['HAVE_ZMQ'] and not conf.env['HAVE_XS']:
        raise Errors.ConfigurationError(
            "Either libzmq or libxs is required, none found")
    conf.write_config_header('src/config.h')

def build(bld):
    import coyaml.waf
    bld(
        features     = ['c', 'cprogram', 'coyaml'],
        source       = [
            'src/devices.yaml',
            'src/main.c',
            'src/handle_zmq.c',
            'src/handle_xs.c',
            ],
        target       = 'paperjam',
        includes     = ['src'],
        cflags       = ['-std=gnu99', '-Wall'],
        lib          = ['coyaml', 'yaml'],
        use          = ['XS', 'ZMQ'],
        )

def dist(ctx):
    ctx.excl = [
        'doc/_build/**',
        '.waf*', '*.tar.gz', '*.zip', 'build',
        '.git*', '.lock*', '**/*.pyc', '**/*.swp', '**/*~',
        ]
    ctx.algo = 'tar.gz'


def make_pkgbuild(task):
    import hashlib
    task.outputs[0].write(Utils.subst_vars(task.inputs[0].read(), {
        'VERSION': VERSION,
        'DIST_MD5': hashlib.md5(task.inputs[1].read('rb')).hexdigest(),
        }))


def archpkg(bld):
    distfile = APPNAME + '-' + VERSION + '.tar.gz'
    bld(rule=make_pkgbuild,
        source=['PKGBUILD.tpl', distfile, 'wscript'],
        target='PKGBUILD')
    bld(rule='cp ${SRC} ${TGT}', source=distfile, target='.')
    bld.add_group()
    bld(rule='makepkg -f', source=distfile)
    bld.add_group()
    bld(rule='makepkg -f --source', source=distfile)


class makepkg(BuildContext):
    cmd = 'archpkg'
    fun = 'archpkg'
    variant = 'archpkg'


def _encode_multipart_formdata(fields, files):
    """
    fields is a sequence of (name, value) elements for regular form fields.
    files is a sequence of (name, filename, value) elements for data
    to be uploaded as files
    Return (content_type, body) ready for httplib.HTTP instance
    """
    BOUNDARY = b'----------ThIs_Is_tHe_bouNdaRY'
    CRLF = b'\r\n'
    L = []
    for (key, value) in fields:
        L.append(b'--' + BOUNDARY)
        L.append(('Content-Disposition: form-data; name="%s"' % key)
            .encode('utf-8'))
        L.append(b'')
        L.append(value.encode('utf-8'))
    for (key, filename, value, mime) in files:
        assert key == 'file'
        L.append(b'--' + BOUNDARY)
        L.append(b'Content-Type: ' + mime.encode('ascii'))
        L.append(('Content-Disposition: form-data; name="%s"; filename="%s"'
            % (key, filename)).encode('utf-8'))
        L.append(b'')
        L.append(value)
    L.append(b'--' + BOUNDARY + b'--')
    L.append(b'')
    body = CRLF.join(L)
    content_type = 'multipart/form-data; boundary=%s' % BOUNDARY.decode('ascii')
    return content_type, body

def upload(ctx):
    "quick and dirty command to upload files to github"
    import hashlib
    import urllib.parse
    from http.client import HTTPSConnection, HTTPConnection
    import json
    distfile = APPNAME + '-' + VERSION + '.tar.gz'
    with open(distfile, 'rb') as f:
        distdata = f.read()
    md5 = hashlib.md5(distdata).hexdigest()
    remotes = subprocess.getoutput('git remote -v')
    for r in remotes.splitlines():
        url = r.split()[1]
        if url.startswith('git@github.com:'):
            gh_repo = url[len('git@github.com:'):-len('.git')]
            break
    else:
        raise RuntimeError("repository not found")
    gh_token = subprocess.getoutput('git config github.token').strip()
    gh_login = subprocess.getoutput('git config github.user').strip()
    cli = HTTPSConnection('github.com')
    cli.request('POST', '/'+gh_repo+'/downloads',
        headers={'Host': 'github.com',
                 'Content-Type': 'application/x-www-form-urlencoded'},
        body=urllib.parse.urlencode({
            "file_name": distfile,
            "file_size": len(distdata),
            "description": APPNAME.title() + ' source v'
                + VERSION + " md5: " + md5,
            "login": gh_login,
            "token": gh_token,
        }).encode('utf-8'))
    resp = cli.getresponse()
    data = resp.read().decode('utf-8')
    data = json.loads(data)
    s3data = (
        ("key", data['path']),
        ("acl", data['acl']),
        ("success_action_status", "201"),
        ("Filename", distfile),
        ("AWSAccessKeyId", data['accesskeyid']),
        ("policy", data['policy']),
        ("signature", data['signature']),
        ("Content-Type", data['mime_type']),
        )
    ctype, body = _encode_multipart_formdata(s3data, [
        ('file', distfile, distdata, data['mime_type']),
        ])
    cli.close()
    cli = HTTPSConnection('github.s3.amazonaws.com')
    cli.request('POST', '/',
                body=body,
                headers={'Content-Type': ctype,
                         'Host': 'github.s3.amazonaws.com'})
    resp = cli.getresponse()
    print(resp.read())
