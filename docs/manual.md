% libuca -- A Unified Camera Access Interface
% Matthias Vogelgesang [matthias.vogelgesang@kit.edu]

libuca is a light-weight camera abstraction library, focused on scientific
cameras used at the ANKA synchrotron.

# Quickstart

## Installation

Before installing `libuca` itself, you should install any drivers and SDKs
needed to access the cameras you want to access through `libuca`. Now you have
two options: install pre-built packages or build from source.

### Installing packages

Packages for the core library and all plugins are currently provided for
openSUSE. To install them run `zypper`:

    sudo zypper in libuca-x.y.z-x86_64.rpm
    sudo zypper in uca-plugin-*.rpm

To install development files such as headers, you have to install the
`libuca-x.y.z-devel.rpm` package.

### Building from source

Building the library and installing from source is simple and straightforward.
Make sure you have

* CMake,
* a C compiler,
* GLib and GObject development libraries and
* necessary camera SDKs

installed.

For the base system, install

    [Debian] sudo apt-get install libglib2.0 cmake gcc
    [openSUSE] sudo zypper in glib2-devel cmake gcc

In case you want to use the graphical user interface you also need the Gtk+
development libraries:

    [Debian] sudo apt-get install libgtk+2.0-dev
    [openSUSE] sudo zypper in gtk2-devel

To generate bindings for third-party languages, you have to install

    [Debian] sudo apt-get install gobject-introspection
    [openSUSE] sudo zypper in gobject-introspection-devel


#### Fetching the sources

Untar the distribution

    untar xfz libuca-x.y.z.tar.gz

or clone the repository

    git clone http://ufo.kit.edu/git/libuca

and create a new, empty build directory inside:

    cd libuca/
    mkdir build


#### Configuring and building

Now you need to create the Makefile with CMake. Go into the build directory and
point CMake to the `libuca` top-level directory:

    cd build/
    cmake ..

As long as the last line reads "Build files have been written to", the
configuration stage is successful. In this case you can build `libuca` with

    make

and install with

    sudo make install

If an _essential_ dependency could not be found, the configuration stage will stop
and build files will not be written. If a _non-essential_ dependency (such as a
certain camera SDK) is not found, the configuration stage will continue but that
particular camera support not built.

If you want to customize the build process you can pass several variables to
CMake:

    cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DLIB_SUFFIX=64

The former tells CMake to install into `/usr` instead of `/usr/local` and the
latter that 64 should be appended to any library paths. This is necessary on
Linux distributions that expect 64-bit libraries in `/usr[/local]/lib64`.

It is also highly recommended to set the the install prefix to `/usr` when
using the language bindings because GObject introspection will only look for
type libraries in `/usr/lib/girepository-1.0`. If you want to install in another
directory, you can also set the `GI_TYPELIB_PATH` environment variable to the
path were the `Ufo-x.y.typelib` is located.


#### Building this manual

Make sure you have [Pandoc][] installed. With Debian/Ubuntu this can be achieved
with

    sudo apt-get install pandoc

Once done, go into `docs/` and type

    make [all|pdf|html]

[Pandoc]: http://johnmacfarlane.net/pandoc/


## First look at the API

The API for accessing cameras is straightforward. First you need to include the
necessary header files:

~~~ {.c}
#include <glib-object.h>
#include <uca-plugin-manager.h>
#include <uca-camera.h>
~~~

Then you need to setup the type system:

~~~ {.c}
int
main (int argc, char *argv[])
{
    UcaPluginManager *manager;
    UcaCamera *camera;
    GError *error = NULL; /* this _must_ be set to NULL */

    g_type_init ();
~~~

Now you can instantiate new camera _objects_. Each camera is identified by a
human-readable string, in this case we want to access any pco camera that is
supported by [libpco][]. To instantiate a camera we have to create a plugin
manager first:

~~~ {.c}
    manager = uca_plugin_manager_new ();
    camera = uca_plugin_manager_get_camera (manager, "pco", &error);
~~~

Errors are indicated with a returned value `NULL` and `error` set to a value
other than `NULL`:

~~~ {.c}
    if (camera == NULL) {
        g_error ("Initialization: %s", error->message);
        return 1;
    }
~~~

You should always remove the [reference][gobject-references] from the camera
object when not using it in order to free all associated resources:

~~~ {.c}
    g_object_unref (camera);
    return 0;
}
~~~

Compile this program with

    cc `pkg-config --cflags --libs libuca glib-2.0` foo.c -o foo

Now, run `foo` and verify that no errors occur.


[libpco]: http://ufo.kit.edu/repos/libpco.git/
[gobject-references]: http://developer.gnome.org/gobject/stable/gobject-memory.html#gobject-memory-refcount


### Grabbing frames

To synchronously grab frames, first start the camera:

~~~ {.c}
    uca_camera_start_recording (camera, &error);
    g_assert_no_error (error);
~~~

Now, you have to allocate a suitably sized buffer and pass it to
`uca_camera_grab`.

~~~ {.c}
    gpointer buffer = g_malloc0 (640 * 480 * 2);

    uca_camera_grab (camera, buffer, &error);
~~~

You have to make sure that the buffer is large enough by querying the size of
the region of interest and the number of bits that are transferred.


### Getting and setting camera parameters

Because camera parameters vary tremendously between different vendors and
products, they are realized with so-called GObject _properties_, a mechanism
that maps string keys to typed and access restricted values. To get a value, you
use the `g_object_get` function and provide memory where the result is stored:

~~~ {.c}
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
~~~

In a similar way, properties are set with `g_object_set`:

~~~ {.c}
    guint roi_width = 512;
    gdouble exposure_time = 0.001;

    g_object_set (G_OBJECT (camera),
                  "roi-width", roi_width,
                  "exposure-time", exposure_time,
                  NULL);
~~~

Each property can be associated with a physical unit. To query for the unit call
`uca_camera_get_unit` and pass a property name. The function will then return a
value from the `UcaUnit` enum.

Several essential camera parameters _must_ be implemented by all cameras. To get
a list of them consult the API reference for [`UcaCamera`][ucacam-ref]. For
camera specific parameters you need to consult the corresponding API reference
for `UfoFooCamera`. The latest nightly built reference can be found
[here][libuca-reference].

[ucacam-ref]: http://ufo.kit.edu/extra/libuca/reference/UcaCamera.html#UcaCamera.properties
[libuca-reference]: http://ufo.kit.edu/extra/libuca/reference/


# Supported cameras

The following cameras are supported:

* pco.edge, pco.dimax, pco.4000 (all CameraLink) via [libpco][]. You need to
  have the SiliconSoftware frame grabber SDK with the `menable` kernel module
  installed.
* PhotonFocus
* Pylon
* UFO Camera developed at KIT/IPE.

## Property documentation

* [mock][mock-doc]
* [pco][pco-doc]
* [PhotonFocus][pf-doc]
* [Ufo Camera][ufo-doc]

[mock-doc]: mock.html
[pco-doc]: pco.html
[pf-doc]: pf.html
[ufo-doc]: ufo.html


# More API

In the [last section][], we had a quick glance over the basic API used to
communicate with the camera. Now we will go into more detail.

## Instantiating cameras

We have already seen how to instantiate a camera object from a name. If you have
more than one camera connected to a machine, you will most likely want the user
decide which to use. To do so, you can enumerate all camera strings with
`uca_plugin_manager_get_available_cameras`:

~~~ {.c}
    GList *types;

    types = uca_camera_get_available_cameras (manager);

    for (GList *it = g_list_first; it != NULL; it = g_list_next (it))
        g_print ("%s\n", (gchar *) it->data);

    /* free the strings and the list */
    g_list_foreach (types, (GFunc) g_free, NULL);
    g_list_free (types);
~~~

[last section]: #first-look-at-the-api


## Errors

All public API functions take a location of a pointer to a `GError` structure as
a last argument. You can pass in a `NULL` value, in which case you cannot be
notified about exceptional behavior. On the other hand, if you pass in a
pointer to a `GError`, it must be initialized with `NULL` so that you do not
accidentally overwrite and miss an error occurred earlier.

Read more about `GError`s in the official GLib
[documentation][GError].

[GError]: http://developer.gnome.org/glib/stable/glib-Error-Reporting.html


## Recording

Recording frames is independent of actually grabbing them and is started with
`uca_camera_start_recording`. You should always stop the recording with
`ufo_camera_stop_recording` when you finished. When the recording has started,
you can grab frames synchronously as described earlier. In this mode, a block to
`uca_camera_grab` blocks until a frame is read from the camera. Grabbing might
block indefinitely, when the camera is not functioning correctly or it is not
triggered automatically.


## Triggering

`libuca` supports three trigger modes through the "trigger-mode" property:

1. `UCA_CAMERA_TRIGGER_AUTO`: Exposure is triggered by the camera itself.
2. `UCA_CAMERA_TRIGGER_INTERNAL`: Exposure is triggered via software.
3. `UCA_CAMERA_TRIGGER_EXTERNAL`: Exposure is triggered by an external hardware
   mechanism.

With `UCA_CAMERA_TRIGGER_INTERNAL` you have to trigger with
`uca_camera_trigger`:

~~~ {.c}
    /* thread A */
    g_object_set (G_OBJECT (camera),
                  "trigger-mode", UCA_CAMERA_TRIGGER_INTERNAL,
                  NULL);

    uca_camera_start_recording (camera, NULL);
    uca_camera_grab (camera, &buffer, NULL);
    uca_camera_stop_recording (camera, NULL);

    /* thread B */
    uca_camera_trigger (camera, NULL);
~~~


## Grabbing frames asynchronously

In some applications, it might make sense to setup asynchronous frame
acquisition, for which you will not be blocked by a call to `libuca`:

~~~ {.c}
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
~~~


# Bindings

Since version 1.1, libuca generates GObject introspection meta data if
`g-ir-scanner` and `g-ir-compiler` can be found. When the XML description
`Uca-x.y.gir` and the typelib `Uca-x.y.typelib` are installed, GI-aware
languages can access libuca and create and modify cameras, for example in
Python:

~~~ {.python}
from gi.repository import Uca

pm = Uca.PluginManager()

# List all cameras
print(pm.get_available_cameras())

# Load a camera
cam = pm.get_camerav('pco', [])

# You can read and write properties in two ways
cam.set_properties(exposure_time=0.05)
cam.props.roi_width = 1024
~~~

Note, that the naming of classes and properties depends on the GI implementation
of the target language. For example with Python, the namespace prefix `uca_`
becomes the module name `Uca` and dashes separating property names become
underscores.

Integration with Numpy is relatively straightforward. The most important thing
is to get the data pointer from a Numpy array to pass it to `uca_camera_grab`:

~~~ {.python}
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
~~~


# Integrating new cameras

A new camera is integrated by [sub-classing][] `UcaCamera` and implement all
virtual methods. The simplest way is to take the `mock` camera and
rename all occurences. Note, that if you class is going to be called `FooBar`,
the upper case variant is `FOO_BAR` and the lower case variant is `foo_bar`.

In order to fully implement a camera, you need to override at least the
following virtual methods:

* `start_recording`: Take suitable actions so that a subsequent call to
  `grab` delivers an image or blocks until one is exposed.
* `stop_recording`: Stop recording so that subsequent calls to `grab`
  fail.
* `grab`: Return an image from the camera or block until one is ready.

## Asynchronous operation

When the camera supports asynchronous acquisition and announces it with a true
boolean value for `"transfer-asynchronously"`, a mechanism must be setup up
during `start_recording` so that for each new frame the grab func callback is
called.

## Cameras with internal memory

Cameras such as the pco.dimax record into their own on-board memory rather than
streaming directly to the host PC. In this case, both `start_recording` and
`stop_recording` initiate and end acquisition to the on-board memory. To
initiate a data transfer, the host calls `start_readout` which must be suitably
implemented. The actual data transfer happens either with `grab` or
asynchronously.


[sub-classing]: http://developer.gnome.org/gobject/stable/howto-gobject.html


# Tools

Several tools are available to ensure `libuca` works as expected. All of them
are located in `build/test/` and some of them are installed with `make
installed`.

## `uca-grab` -- grabbing frames

Grab with frames with

    $ uca-grab --num-frames=10 camera-model

store them on disk as `frames.tif` if `libtiff` is installed, otherwise as
`frame-00000.raw`, `frame-000001.raw`. The raw format is a memory dump of the
frames, so you might want to use [ImageJ][] to view them. You can also specify
the output filename or filename prefix with the ``-o/--output`` option:

    $ uca-grab -n 10 --output=foobar.tif camera-model

Instead of reading exactly _n_ frames, you can also specify a duration in
fractions of seconds:

    $ uca-grab --duration=0.25 camera-model

[ImageJ]: http://rsbweb.nih.gov/ij/


## `uca-camera-control` -- simple graphical user interface

Shows the frames and displays them on screen. Moreover, you can change the
camera properties in a side pane.

## `uca-benchmark` -- check bandwidth

Measure the memory bandwidth by taking subsequent frames and averaging the
grabbing time:

    $ ./benchmark mock
    # --- General information ---
    # Sensor size: 640x480
    # ROI size: 640x480
    # Exposure time: 0.000010s
    # type      n_frames  n_runs    frames/s        MiB/s
      sync      100       3         29848.98        8744.82
      async     100       3         15739.43        4611.16


# The GObject Tango device

[TODO: Get more information from Volker Kaiser and/or Mihael Koep]

