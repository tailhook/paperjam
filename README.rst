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

Configuring
-----------

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

