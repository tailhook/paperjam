Paperjam
========

Paperjam is an implementation of "devices" for zeromq and crossroads.


Dependencies
------------

Any or both of the following:

* libzmq
* libxs

Build time dependencies:

* coyaml_
* libyaml_
* python3_

.. _coyaml: http://github.com/tailhook/coyaml
.. _libyaml: http://pyyaml.org/wiki/LibYAML
.. _python3: http://python.org

Compiling
---------

::

    ./waf configure --prefix=/usr
    ./waf build
    ./waf test
    sudo ./waf install

Command-line Tools
------------------

There are three binaries in the project:

paperjam
    Devices implementation, which uses YAMLy config (see below)

pjmonitor
    Monitor for paperjam. It attaches to monitor sockets (if configured),
    and shows everything sent inside device. Useful for debugging.

pjutil
    Utility for sending and receiving messages from the command-line. Basic
    usage is following:

        pjutil --*lib* --*type* --connect *addr*

    Where *lib* is either ``xs`` or ``zmq``. And *type* is one of the
    socket types zeromq or crossroads support. Use ``--help`` to get a
    full list of socket types.

    *addr* can be any zeromq or crossroads address.  For example
    ``tcp://127.0.0.1:1234``. The ``--connect`` option is repeatable and there
    is also ``--bind`` option.

    Every leftover arguments will be sent to the socket, if socket is writable.
    Each argument is sent as a part of single multipart message. For the
    ``rep`` and ``respondent`` sockets, those arguments will be replied for
    each request from the socket. For other writable sockets the message will
    be sent at the start.

    All input messages will be printed one per line, quoted with double quotes
    and space-separated.

    Process with ``req`` socket type exits as soon as response comes.
    ``push`` and ``pub`` processes exit as soon as message is put in OS
    buffers (which basically means message is sent). All other modes are
    left in the indefinite loop of listening and printing messages. You
    can set maximum process running time for any socket type
    with ``--timeout`` option.


zmqpush, zmqpull, zmqreq, zmqrep, zmqpub, zmqsub, xspush, xspull, xsreq, xsrep, xspub, xssub, xssurveyor, xsrespondent
    Aliases (symlinks) to pjutil, which are preconfigured for specific library
    (libxs or libzmq) and socket type. Except skipping aformentioned arguments
    they behave exactly as pjutil. Only supported combinations of library
    and socket type are installed on the system.


Configuring Devices
-------------------

Default configuration file is in ``/etc/paperjam.yaml``. It is normal YAML_
file with support of anchors and other nice things. Devices configured in
``Devices`` section, which is mapping of devices, for example::

    Devices:

        opaque_name_of_device:

            # socket where you should send request to
            frontend: !zmq.Rep
            - !Bind "ipc:///tmp/frontend.sock"

            # socket which will really respond to requests
            backend: !zmq.Rep
            - !Connect "tcp://10.0.0.1:10000"

            # socket which you can introspect traffic from
            # it's optional
            monitor: !zmq.Push
            - !Bind "ipc:///tmp/monitor.sock"

All kinds of sockets and any combination of zmq vs xs sockets supported. We
allow various combinations of patterns, for example to redirect traffic from
'pull' socket to 'pub' one, but disable meaningless ones, like connecting 'rep'
socket to 'pull' one. We also hide all the gory details of ``XREP``/``XREQ``
sockets and use ``Rep``/``Rep`` in config (however, we use ``X``-prefixed
sockets internally to multiplex requests). All kinds of zmq and xs addresses
are supported.

.. _YAML: http://yaml.org
