Application Programming Interface
=================================

.. highlight:: c

In the introduction we had a quick glance over the basic API used to communicate
with a camera. Now we will go into more detail and explain required background
to understand the execution model.


Instantiating cameras
---------------------

We have already seen how to instantiate a camera object from a name. If
you have more than one camera connected to a machine, you will most
likely want the user decide which to use. To do so, you can enumerate
all camera strings with ``uca_plugin_manager_get_available_cameras``::

        GList *types;

        types = uca_plugin_manager_get_available_cameras (manager);

        for (GList *it = g_list_first (types); it != NULL; it = g_list_next (it))
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

``libuca`` supports three trigger sources through the "trigger-source"
property:

1. ``UCA_CAMERA_TRIGGER_SOURCE_AUTO``: Exposure is triggered by the camera
   itself.
2. ``UCA_CAMERA_TRIGGER_SOURCE_SOFTWARE``: Exposure is triggered via software.
3. ``UCA_CAMERA_TRIGGER_SOURCE_EXTERNAL``: Exposure is triggered by an external
   hardware mechanism.

With ``UCA_CAMERA_TRIGGER_SOURCE_SOFTWARE`` you have to trigger with
``uca_camera_trigger``::

        /* thread A */
        g_object_set (G_OBJECT (camera),
                      "trigger-source", UCA_CAMERA_TRIGGER_SOURCE_SOFTWARE,
                      NULL);

        uca_camera_start_recording (camera, NULL);
        uca_camera_grab (camera, buffer, NULL);
        uca_camera_stop_recording (camera, NULL);

        /* thread B */
        uca_camera_trigger (camera, NULL);

Moreover, the "trigger-type" property specifies if the exposure should be
triggered at the rising edge or during the level signal.


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
--------

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
-----------------------

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
