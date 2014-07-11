Welcome
=======

libuca is a light-weight camera abstraction library, focused on scientific
cameras used at the ANKA synchrotron.


Quickstart
==========

Installation
------------

Before installing ``libuca`` itself, you should install any drivers and
SDKs needed to access the cameras you want to access through ``libuca``.
Now you have two options: install pre-built packages or build from
source.


Installing packages
~~~~~~~~~~~~~~~~~~~

Packages for the core library and all plugins are currently provided for
openSUSE. To install them run ``zypper``:

::

    sudo zypper in libuca-x.y.z-x86_64.rpm
    sudo zypper in uca-plugin-*.rpm

To install development files such as headers, you have to install the
``libuca-x.y.z-devel.rpm`` package.


Building from source
~~~~~~~~~~~~~~~~~~~~

Building the library and installing from source is simple and
straightforward. Make sure you have

-  CMake,
-  a C compiler,
-  GLib and GObject development libraries and
-  necessary camera SDKs

installed.

For the base system, install ::

    [Debian] sudo apt-get install libglib2.0 cmake gcc
    [openSUSE] sudo zypper in glib2-devel cmake gcc

In case you want to use the graphical user interface you also need the
Gtk+ development libraries::

    [Debian] sudo apt-get install libgtk+2.0-dev
    [openSUSE] sudo zypper in gtk2-devel

To generate bindings for third-party languages, you have to install ::

    [Debian] sudo apt-get install gobject-introspection
    [openSUSE] sudo zypper in gobject-introspection-devel


Fetching the sources
^^^^^^^^^^^^^^^^^^^^

Untar the distribution ::

    untar xfz libuca-x.y.z.tar.gz

or clone the repository ::

    git clone http://ufo.kit.edu/git/libuca

and create a new, empty build directory inside::

    cd libuca/
    mkdir build


Configuring and building
^^^^^^^^^^^^^^^^^^^^^^^^

Now you need to create the Makefile with CMake. Go into the build
directory and point CMake to the ``libuca`` top-level directory::

    cd build/
    cmake ..

As long as the last line reads "Build files have been written to", the
configuration stage is successful. In this case you can build ``libuca``
with ::

    make

and install with ::

    sudo make install

If an *essential* dependency could not be found, the configuration stage
will stop and build files will not be written. If a *non-essential*
dependency (such as a certain camera SDK) is not found, the
configuration stage will continue but that particular camera support not
built.

If you want to customize the build process you can pass several
variables to CMake::

    cmake .. -DPREFIX=/usr -DLIBDIR=/usr/lib64

The former tells CMake to install into ``/usr`` instead of
``/usr/local`` and the latter that we want to install the libraries and
plugins into the ``lib64`` subdir instead of the default ``lib`` subdir
as it is common on SUSE systems.


First look at the API
---------------------

.. highlight:: c

The API for accessing cameras is straightforward. First you need to
include the necessary header files::

    #include <glib-object.h>
    #include <uca/uca-plugin-manager.h>
    #include <uca/uca-camera.h>

Then you need to setup the type system::

    int
    main (int argc, char *argv[])
    {
        UcaPluginManager *manager;
        UcaCamera *camera;
        GError *error = NULL; /* this _must_ be set to NULL */

        g_type_init ();

Now you can instantiate new camera *objects*. Each camera is identified
by a human-readable string, in this case we want to access any pco
camera that is supported by
`libpco <http://ufo.kit.edu/repos/libpco.git/>`__. To instantiate a
camera we have to create a plugin manager first::

        manager = uca_plugin_manager_new ();
        camera = uca_plugin_manager_get_camera (manager, "pco", &error);

Errors are indicated with a returned value ``NULL`` and ``error`` set to
a value other than ``NULL``::

        if (camera == NULL) {
            g_error ("Initialization: %s", error->message);
            return 1;
        }

You should always remove the
`reference <http://developer.gnome.org/gobject/stable/gobject-memory.html#gobject-memory-refcount>`__
from the camera object when not using it in order to free all associated
resources::

        g_object_unref (camera);
        return 0;
    }

Compile this program with ::

    cc `pkg-config --cflags --libs libuca glib-2.0` foo.c -o foo

Now, run ``foo`` and verify that no errors occur.


Grabbing frames
~~~~~~~~~~~~~~~

To synchronously grab frames, first start the camera::

        uca_camera_start_recording (camera, &error);
        g_assert_no_error (error);

Now, you have to allocate a suitably sized buffer and pass it to
``uca_camera_grab``::

        gpointer buffer = g_malloc0 (640 * 480 * 2);

        uca_camera_grab (camera, buffer, &error);

You have to make sure that the buffer is large enough by querying the
size of the region of interest and the number of bits that are
transferred.


Getting and setting camera parameters
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Because camera parameters vary tremendously between different vendors
and products, they are realized with so-called GObject *properties*, a
mechanism that maps string keys to typed and access restricted values.
To get a value, you use the ``g_object_get`` function and provide memory
where the result is stored::

        guint roi_width;
        gdouble exposure_time;

        g_object_get (G_OBJECT(camera),
                      "roi-width", &roi_width,
                      "exposure-time", &exposure_time,
                      /* The NULL marks the end! */
                      NULL
                      );

        g_print ("Width of the region of interest: %d\n", roi_width);
        g_print ("Exposure time: %3.5s\n", exposure_time);

In a similar way, properties are set with ``g_object_set``::

        guint roi_width = 512;
        gdouble exposure_time = 0.001;

        g_object_set (G_OBJECT (camera),
                      "roi-width", roi_width,
                      "exposure-time", exposure_time,
                      NULL);

Each property can be associated with a physical unit. To query for the
unit call ``uca_camera_get_unit`` and pass a property name. The function
will then return a value from the ``UcaUnit`` enum.

Several essential camera parameters *must* be implemented by all
cameras. To get a list of them consult the API reference for
```UcaCamera`` <http://ufo.kit.edu/extra/libuca/reference/UcaCamera.html#UcaCamera.properties>`__.
For camera specific parameters you need to consult the corresponding API
reference for ``UfoFooCamera``. The latest nightly built reference can
be found `here <http://ufo.kit.edu/extra/libuca/reference/>`__.


Supported cameras
=================

The following cameras are supported:

-  pco.edge, pco.dimax, pco.4000 (all CameraLink) via
   `libpco <http://ufo.kit.edu/repos/libpco.git/>`__. You need to have
   the SiliconSoftware frame grabber SDK with the ``menable`` kernel
   module installed.
-  PhotonFocus
-  Pylon
-  UFO Camera developed at KIT/IPE.


Property documentation
----------------------

-  `mock <mock.html>`__
-  `pco <pco.html>`__
-  `PhotonFocus <pf.html>`__
-  `Ufo Camera <ufo.html>`__


More API
========

In the `last section <#first-look-at-the-api>`__, we had a quick glance
over the basic API used to communicate with the camera. Now we will go
into more detail.


Instantiating cameras
---------------------

We have already seen how to instantiate a camera object from a name. If
you have more than one camera connected to a machine, you will most
likely want the user decide which to use. To do so, you can enumerate
all camera strings with ``uca_plugin_manager_get_available_cameras``::

        GList *types;

        types = uca_camera_get_available_cameras (manager);

        for (GList *it = g_list_first; it != NULL; it = g_list_next (it))
            g_print ("%s\n", (gchar *) it->data);

        /* free the strings and the list */
        g_list_foreach (types, (GFunc) g_free, NULL);
        g_list_free (types);


Errors
------

All public API functions take a location of a pointer to a ``GError``
structure as a last argument. You can pass in a ``NULL`` value, in which
case you cannot be notified about exceptional behavior. On the other
hand, if you pass in a pointer to a ``GError``, it must be initialized
with ``NULL`` so that you do not accidentally overwrite and miss an
error occurred earlier.

Read more about ``GError``\ s in the official GLib
`documentation <http://developer.gnome.org/glib/stable/glib-Error-Reporting.html>`__.


Recording
---------

Recording frames is independent of actually grabbing them and is started
with ``uca_camera_start_recording``. You should always stop the
recording with ``ufo_camera_stop_recording`` when you finished. When the
recording has started, you can grab frames synchronously as described
earlier. In this mode, a block to ``uca_camera_grab`` blocks until a
frame is read from the camera. Grabbing might block indefinitely, when
the camera is not functioning correctly or it is not triggered
automatically.


Triggering
----------

``libuca`` supports three trigger modes through the "trigger-mode"
property:

1. ``UCA_CAMERA_TRIGGER_AUTO``: Exposure is triggered by the camera
   itself.
2. ``UCA_CAMERA_TRIGGER_INTERNAL``: Exposure is triggered via software.
3. ``UCA_CAMERA_TRIGGER_EXTERNAL``: Exposure is triggered by an external
   hardware mechanism.

With ``UCA_CAMERA_TRIGGER_INTERNAL`` you have to trigger with
``uca_camera_trigger``::

        /* thread A */
        g_object_set (G_OBJECT (camera),
                      "trigger-mode", UCA_CAMERA_TRIGGER_INTERNAL,
                      NULL);

        uca_camera_start_recording (camera, NULL);
        uca_camera_grab (camera, &buffer, NULL);
        uca_camera_stop_recording (camera, NULL);

        /* thread B */
        uca_camera_trigger (camera, NULL);


Grabbing frames asynchronously
------------------------------

In some applications, it might make sense to setup asynchronous frame
acquisition, for which you will not be blocked by a call to ``libuca``::

    static void
    callback (gpointer buffer, gpointer user_data)
    {
        /*
         * Do something useful with the buffer and the string we have got.
         */
    }

    static void
    setup_async (UcaCamera *camera)
    {
        gchar *s = g_strdup ("lorem ipsum");

        g_object_set (G_OBJECT (camera),
                      "transfer-asynchronously", TRUE,
                      NULL);

        uca_camera_set_grab_func (camera, callback, s);
        uca_camera_start_recording (camera, NULL);

        /*
         * We will return here and `callback` will be called for each newo
         * new frame.
         */
    }


Bindings
========

.. highlight:: python

Since version 1.1, libuca generates GObject introspection meta data if
``g-ir-scanner`` and ``g-ir-compiler`` can be found. When the XML
description ``Uca-x.y.gir`` and the typelib ``Uca-x.y.typelib`` are
installed, GI-aware languages can access libuca and create and modify
cameras, for example in Python::

    from gi.repository import Uca

    pm = Uca.PluginManager()

    # List all cameras
    print(pm.get_available_cameras())

    # Load a camera
    cam = pm.get_camerav('pco', [])

    # You can read and write properties in two ways
    cam.set_properties(exposure_time=0.05)
    cam.props.roi_width = 1024

Note, that the naming of classes and properties depends on the GI
implementation of the target language. For example with Python, the
namespace prefix ``uca_`` becomes the module name ``Uca`` and dashes
separating property names become underscores.

Integration with Numpy is relatively straightforward. The most important
thing is to get the data pointer from a Numpy array to pass it to
``uca_camera_grab``::

    import numpy as np

    def create_array_from(camera):
        """Create a suitably sized Numpy array and return it together with the
        arrays data pointer"""
        bits = camera.props.sensor_bitdepth
        dtype = np.uint16 if bits > 8 else np.uint8
        a = np.zeros((cam.props.roi_height, cam.props.roi_width), dtype=dtype)
        return a, a.__array_interface__['data'][0]

    # Suppose 'camera' is a already available, you would get the camera data like
    # this:
    a, buf = create_array_from(camera)
    camera.start_recording()
    camera.grab(buf)

    # Now data is in 'a' and we can use Numpy functions on it
    print(np.mean(a))

    camera.stop_recording()


Integrating new cameras
=======================

A new camera is integrated by
`sub-classing <http://developer.gnome.org/gobject/stable/howto-gobject.html>`__
``UcaCamera`` and implement all virtual methods. The simplest way is to
take the ``mock`` camera and rename all occurences. Note, that if you
class is going to be called ``FooBar``, the upper case variant is
``FOO_BAR`` and the lower case variant is ``foo_bar``.

In order to fully implement a camera, you need to override at least the
following virtual methods:

-  ``start_recording``: Take suitable actions so that a subsequent call
   to ``grab`` delivers an image or blocks until one is exposed.
-  ``stop_recording``: Stop recording so that subsequent calls to
   ``grab`` fail.
-  ``grab``: Return an image from the camera or block until one is
   ready.


Asynchronous operation
----------------------

When the camera supports asynchronous acquisition and announces it with
a true boolean value for ``"transfer-asynchronously"``, a mechanism must
be setup up during ``start_recording`` so that for each new frame the
grab func callback is called.


Cameras with internal memory
----------------------------

Cameras such as the pco.dimax record into their own on-board memory
rather than streaming directly to the host PC. In this case, both
``start_recording`` and ``stop_recording`` initiate and end acquisition
to the on-board memory. To initiate a data transfer, the host calls
``start_readout`` which must be suitably implemented. The actual data
transfer happens either with ``grab`` or asynchronously.


Tools
=====

Several tools are available to ensure ``libuca`` works as expected. All
of them are located in ``build/test/`` and some of them are installed
with ``make installed``.


``uca-grab`` -- grabbing frames
-------------------------------

Grab with frames with ::

    $ uca-grab --num-frames=10 camera-model

store them on disk as ``frames.tif`` if ``libtiff`` is installed,
otherwise as ``frame-00000.raw``, ``frame-000001.raw``. The raw format
is a memory dump of the frames, so you might want to use
`ImageJ <http://rsbweb.nih.gov/ij/>`__ to view them. You can also
specify the output filename or filename prefix with the ``-o/--output``
option::

    $ uca-grab -n 10 --output=foobar.tif camera-model

Instead of reading exactly *n* frames, you can also specify a duration
in fractions of seconds::

    $ uca-grab --duration=0.25 camera-model


``uca-camera-control`` -- simple graphical user interface
---------------------------------------------------------

Shows the frames and displays them on screen. Moreover, you can change
the camera properties in a side pane.


``uca-benchmark`` -- check bandwidth
------------------------------------

Measure the memory bandwidth by taking subsequent frames and averaging
the grabbing time:

::

    $ ./benchmark mock
    # --- General information ---
    # Sensor size: 640x480
    # ROI size: 640x480
    # Exposure time: 0.000010s
    # type      n_frames  n_runs    frames/s        MiB/s
      sync      100       3         29848.98        8744.82
      async     100       3         15739.43        4611.16


The GObject Tango device
========================

UcaDevice is a generic Tango Device that wraps ``libuca`` in order to
make libuca controlled cameras available as Tango devices.


Architecture
------------

UcaDevice is derived from GObjectDevice and adds libuca specific features like
start/stop recording etc.  The most important feature is *acquisition control*.
UcaDevice is responsible for controlling acquisition of images from libuca. The
last aquired image can be accessed by reading attribute ``SingleImage``.
UcaDevice is most useful together with ImageDevice. If used together, UcaDevice
sends each aquired image to ImageDevice, which in turn does configured
post-processing like flipping, rotating or writing the image to disk.


Attributes
~~~~~~~~~~

Besides the dynamic attributes translated from libuca properties
UcaDevice has the following attributes:

-  ``NumberOfImages (Tango::DevLong)``: how many images should be
   acquired? A value of -1 means unlimited *(read/write)*
-  ``ImageCounter (Tango::DevULong)``: current number of acquired images
   *(read-only)*
-  ``CameraName (Tango::DevString)``: name of libuca object type
   *(read-only)*
-  ``SingleImage (Tango::DevUChar)``: holds the last acquired image


Acquisition Control
~~~~~~~~~~~~~~~~~~~

In UcaDevice acquisition control is mostly implemented by two
``yat4tango::DeviceTasks``: *AcquisitionTask* and *GrabTask*.
*GrabTask*'s only responsibility is to call ``grab`` on ``libuca``
synchronously and post the data on to AcquisitionTask.

*AcquisitionTask* is responsible for starting and stopping GrabTask and
for passing image data on to ``ImageDevice`` (if exisiting) and to
``UcaDevice`` for storage in attribute ``SingleImage``. It counts how
many images have been acquired and checks this number against the
configured ``NumberOfImages``. If the desired number is reached, it
stops GrabTask, calls ``stop_recording`` on ``libuca`` and sets the
Tango state to STANDBY.


Plugins
~~~~~~~

As different cameras have different needs, plugins are used for special
implementations. Plugins also makes UcaDevice and Tango Servers based on
it more flexible and independent of libuca implementation.

* PCO: The Pco plugin implements additional checks when writing ROI values.
* Pylon: The Pylon plugin sets default values for ``roi-width`` and
  ``roi-height`` from libuca properties ``roi-width-default`` and
  ``roi-height-default``.


Installation
------------

Detailed installation depends on the manifestation of UcaDevice. All
manifestations depend on the following libraries:

-  YAT
-  YAT4Tango
-  Tango
-  GObjectDevice
-  ImageDevice


Build
~~~~~

::

    export PKG_CONFIG_PATH=/usr/lib/pkgconfig
    export PYLON_ROOT=/usr/pylon
    export LD_LIBRARY_PATH=$PYLON_ROOT/lib64:$PYLON_ROOT/genicam/bin/Linux64_x64
    git clone git@iss-repo:UcaDevice.git
    cd UcaDevice
    mkdir build
    cd build
    cmake ..
    make


Setup in Tango Database / Jive
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Before ``ds_UcaDevice`` can be started, it has to be registered manually
in the Tango database. With ``Jive`` the following steps are necessary:

1. Register Server Menu *Tools* → Server Wizard Server name → ds\_UcaDevice
   Instance name → my\_server *(name can be chosen freely)* Next Cancel

2.  Register Classes and Instances In tab *Server*: context menu on
    ds\_UcaDevice → my\_server → Add Class Class: UcaDevice Devices:
    ``iss/name1/name2`` Register server same for class ImageDevice

3. Start server ::

    export TANGO_HOST=anka-tango:100xx
    export UCA_DEVICE_PLUGINS_DIR=/usr/lib(64)
    ds_UcaDevice pco my_server

4. Setup properties for UcaDevice context menu on device → Device wizard
   Property StorageDevice: *address of previously registered ImageDevice
   instance*

5. Setup properties for ImageDevice context menu on device → Device wizard
   PixelSize: how many bytes per pixel for the images of this camera?
   GrabbingDevice: *address of previously registered UcaDevice instance*

6. Finish restart ds_UcaDevice

FAQ
---

*UcaDevice refuses to start up...?* Most likely there is no instance
registered for class UcaDevice. Register an instance for this class and
it should work.

*Why does UcaDevice depend on ImageDevice?* UcaDevice pushes each new
Frame to ImageDevice. Polling is not only less efficient but also prone
to errors, e.g. missed/double frames and so on. Perhaps we could use the
Tango-Event-System here!

Open Questions, Missing Features etc.
-------------------------------------

* *Why do we need to specify ``Storage`` for UcaDevice and ``GrabbingDevice``
  for ImageDevice?*

  ImageDevice needs the Tango-Address of UcaDevice to mirror all Attributes and
  Commands and to forward them to it. UcaDevice needs the Tango-Address of
  ImageDevice to push a new frame on reception. A more convenient solution could
  be using conventions for the device names, e.g. of the form
  ``$prefix/$instance_name/uca`` and ``$prefix/$instance_name/image``.  That way
  we could get rid of the two Device-Properties and an easier installation
  without the process of registering the classes and instances in ``Jive``.

* *Why does UcaDevice dynamically link to GObjectDevice?*

  There is no good reason for it. Packaging and installing would be easier if we
  linked statically to ``GObjectDevice`` because we would hide this dependency.
  Having a separate ``GObjectDevice`` is generally a nice feature to make
  ``GObjects`` available in Tango. However, there is currently no GObjectDevice
  in use other than in the context of UcaDevice.

* *Why must the plugin name be given as a command line parameter instead of a
  Device-Property?*

  There is no good reason for it. UcaDevice would be easier to use, if the
  plugin was configured in the Tango database as a Device-Property for the
  respective server instance.
