TCP-based network bridge camera
===============================

`uca-net`_ is a transparent TCP-based network bridge camera for remote access of ``libuca``
cameras.

.. _uca-net: https://github.com/ufo-kit/uca-net


Installation
------------

The only dependency is libuca itself and any camera you wish to access.

Clone the repository::

    $ git clone https://github.com/ufo-kit/uca-net.git

and create a new build directory inside::

    $ cd uca-net/
    $ mkdir build

The installation process is the same as by ``libuca``::

    $ cd build/
    $ cmake ..
    $ make
    $ sudo make install


Usage
-----

You can start a server on a remote machine with::

    $ ucad camera-model

and connect to it from any other machine, for example::

    $ uca-grab -p host=foo.bar.com:4567 -n 10 net   // grab ten frames
    $ uca-camera-control -c net                     // control graphically
