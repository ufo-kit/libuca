Python Tango server
===================

``libuca/tango`` is a Python-based Tango server.

Installation
------------

In order to install ``libuca/tango`` you need

- `PyTango`_ and
- `tifffile`_

.. _PyTango: http://www.esrf.eu/computing/cs/tango/tango_doc/kernel_doc/pytango/latest/index.html
.. _tifffile: https://pypi.python.org/pypi/tifffile

Go to the ``libuca`` directory and install the server script with::

    $ cd tango
    $ sudo python setup.py install

and create a new TANGO server ``Uca/xyz`` with a class named ``Camera``.


Usage
-----

Before starting the server, you have to create a new device property ``camera``
which specifies which camera to use. If not set, the ``mock`` camera will be used
by default.

Start the device server with::

    $ Uca device-property

You should be able to manipulate camera attributes like ``exposure_time`` and to store frames using a ``Start``, ``Store``, ``Stop`` cycle::

    import PyTango

    camera = PyTango.DeviceProxy("foo/Camera/mock")
    camera.exposure_time = 0.1
    camera.Start()
    camera.Store('foo.tif')
    camera.Stop()
