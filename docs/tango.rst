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
