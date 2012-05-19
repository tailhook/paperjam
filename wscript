#!/usr/bin/env python3
# -*- coding: utf-8 -*-
from waflib import Utils, Options, Errors
from waflib.Build import BuildContext
from waflib.Scripting import Dist
from waflib.Task import Task
from waflib.TaskGen import extension, before
import subprocess
import os.path

APPNAME='paperjam'
if os.path.exists('.git'):
    VERSION=subprocess.getoutput('git describe').lstrip('v').replace('-', '_')
else:
    VERSION='0.2'

top = '.'
out = 'build'

def options(opt):
    opt.load('compiler_c')
    opt.add_option('--no-symlinks', dest="symlinks",
        help="Do not add symlinks for pjutil (xspush, xspull, zmqpush, ...)",
        default=True, action='store_false')

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
    conf.check(lib='zmq', uselib_store='ZMQ', mandatory=False)
    if not conf.env['HAVE_ZMQ'] and not conf.env['HAVE_XS']:
        raise Errors.ConfigurationError(
            "Either libzmq or libxs is required, none found")
    conf.write_config_header('src/config.h')


class concat_config(Task):
    run_str = 'cat ${SRC} ${SRC[0].parent.abspath()}/devices.yaml > ${TGT}'


@extension('.prefix')
@before('coyaml')
def create_full_config(self, node):
    yfile = node.change_ext('.yaml')
    self.create_task('concat_config', node, yfile)
    self.source.append(yfile)


def build(bld):
    import coyaml.waf
    bld(
        features     = ['c', 'cprogram', 'coyaml'],
        source       = [
            'src/paperjam.prefix',
            'src/main.c',
            'src/handle_zmq.c',
            'src/handle_xs.c',
            ],
        target       = 'paperjam',
        includes     = ['src'],
        cflags       = ['-std=gnu99', '-Wall'],
        lib          = ['coyaml', 'yaml'],
        defines      = [
            'CONFIG_HEADER="paperjam.h"',
            ],
        use          = ['XS', 'ZMQ'],
        )
    bld(
        features     = ['c', 'cprogram', 'coyaml'],
        source       = [
            'src/pjmon.prefix',
            'src/pjmonitor.c',
            'src/handle_zmq.c',
            'src/handle_xs.c',
            ],
        target       = 'pjmonitor',
        includes     = ['src'],
        cflags       = ['-std=gnu99', '-Wall'],
        lib          = ['coyaml', 'yaml'],
        defines      = [
            'CONFIG_HEADER="pjmon.h"',
            ],
        use          = ['XS', 'ZMQ'],
        )
    bld(
        features     = ['c', 'cprogram'],
        source       = ['src/pjutil.c']
            + (['src/xs_cli.c'] if 'LIB_XS' in bld.env else [])
            + (['src/zmq_cli.c'] if 'LIB_ZMQ' in bld.env else [])
            ,
        target       = 'pjutil',
        includes     = ['src'],
        cflags       = ['-std=gnu99', '-Wall'],
        use          = ['XS', 'ZMQ'],
        )
    if bld.options.symlinks:
        symlinks = []
        if 'LIB_ZMQ' in bld.env:
            symlinks += [
                'zmqpush',
                'zmqpull',
                'zmqpub',
                'zmqsub',
                'zmqreq',
                'zmqrep',
                ]
        if 'LIB_XS' in bld.env:
            symlinks += [
                'xspush',
                'xspull',
                'xspub',
                'xssub',
                'xsreq',
                'xsrep',
                ]
        for sym in symlinks:
            bld.symlink_as('${PREFIX}/bin/'+sym, 'pjutil')

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

class test(BuildContext):
    cmd = 'test'
    fun = 'run_tests'

def run_tests(bld):
    build(bld)
    bld.add_group()
    libxs = 'yes' if bld.env['LIB_XS'] else ''
    libzmq = 'yes' if bld.env['LIB_ZMQ'] else ''
    bld(rule='cd ${SRC[0].parent.parent.abspath()};'
        ' PAPERJAM=${SRC[1].abspath()} '
        ' PJMONITOR=${SRC[2].abspath()} '
        ' PJUTIL=${SRC[3].abspath()} '
        ' CONFIGDIR=${SRC[0].parent.abspath()} '
        ' LIB_XS='+libxs+
        ' LIB_ZMQ='+libzmq+
        ' python3 -m unittest discover -v',
        source=['test/base.py', 'paperjam', 'pjmonitor', 'pjutil'],
        always=True)

