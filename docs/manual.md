% libuca -- A Unified Camera Access Interface
% Matthias Vogelgesang [matthias.vogelgesang@kit.edu]

libuca is a light-weight camera abstraction library, focused on scientific
cameras used at the ANKA synchrotron.

# Quickstart

## Installation

Before installing `libuca` itself, you should install any drivers and SDKs
needed to access the cameras you want to access through `libuca`.

## Building from source

Building the library and installing from source is simple and straightforward.
Make sure you have

* CMake,
* a C compiler,
* GLib and GObject development libraries and
* necessary camera SDKs

installed. With Debian/Ubuntu this should be enough:

    sudo apt-get install libglib2.0 cmake gcc

In case you want to use the graphical user interface you also need the Gtk+
development libraries:

    sudo apt-get install libgtk+2.0-dev

If you want to build the most recent version fresh from the [Git
repository][repo], you also need Git:

    sudo apt-get install git

[repo]: http://ufo.kit.edu/repos/libuca.git/

### Fetching the sources

Untar the distribution

    untar xfz libuca-x.y.z.tar.gz

or clone the repository

    git clone http://ufo.kit.edu/git/libuca

and create a new, empty build directory inside:

    cd libuca/
    mkdir build


### Configuring and building

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


### Building this manual

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
#include <uca-camera.h>
~~~

Then you need to setup the type system:

~~~ {.c}
int
main (int argc, char *argv[])
{
    UcaCamera *camera;
    GError *error = NULL; /* this _must_ be set to NULL */

    g_type_init ();
~~~

Now you can instantiate new camera _objects_. Each camera is identified by a
human-readable string, in this case we want to access any pco camera that is
supported by [libpco][]:

~~~ {.c}
    camera = uca_camera_new ("pco", &error);
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

[libpco]: http://ufo.kit.edu/repos/libpco.git/
[gobject-references]: http://developer.gnome.org/gobject/stable/gobject-memory.html#gobject-memory-refcount

### Grabbing frames

To synchronously grab frames, first start the camera:

~~~ {.c}
    uca_camera_start_recording (camera, &error);
    g_assert_no_error (error);
~~~

Now you have two options with regard to memory buffers. If you already have a
suitable sized buffer, just pass it to `uca_camera_grab`. Otherwise pass a
pointer pointing to `NULL` (this is different from a `NULL` pointer!). In this
case memory will be allocated for you:

~~~ {.c}
    gpointer buffer_1 = NULL;   /* A pointer pointing to NULL */
    gpointer buffer_2 = g_malloc0 (640 * 480 * 2);

    /* Memory will be allocated. Remember to free it! */
    uca_camera_grab (camera, &buffer_1, &error);

    /* Memory buffer will be used */
    uca_camera_grab (camera, &buffer_2, &error);
~~~


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

    g_object_set (G_OBJECT(camera),
                  "roi-width", roi_width,
                  "exposure-time", exposure_time,
                  NULL);
~~~

Several essential camera parameters _must_ be implemented by all cameras. To get
a list of them consult the API reference for `UcaCamera`. For camera specific
parameters you need to consult the corresponding API reference for
`UfoFooCamera`.


# Tools

Several tools are available to ensure `libuca` works as expected. All of them
are located in `build/test/` and some of them are installed with `make
installed`.

## `grab` -- grabbing frames

Grab with frames with

    $ ./grab camera-model

store them on disk as `frame-00000.raw`, `frame-000001.raw` ... and measure the
time to take them. The raw format is not format but a memory dump of the
buffers, so you might want to use [ImageJ][] to view them.

[ImageJ]: http://rsbweb.nih.gov/ij/

## `control` -- simple graphical user interface

Shows the frames and displays them on screen. Moreover, you can change the
camera properties in a side pane.

## `benchmark` -- check bandwidth

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

# Integrating a new camera
