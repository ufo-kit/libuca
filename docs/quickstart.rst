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


Usage
-----

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
