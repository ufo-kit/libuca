Tools
=====

Several tools are available to ensure ``libuca`` works as expected. All
of them are located in ``build/test/`` and some of them are installed
with ``make installed``.


uca-camera-control -- simple graphical user interface
-----------------------------------------------------

Records and shows frames. Moreover, you can change the camera properties in a
side pane:

.. image:: uca-gui.png


uca-grab -- grabbing frames
---------------------------

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


uca-benchmark -- check bandwidth
--------------------------------

Measure the memory bandwidth by taking subsequent frames and averaging
the grabbing time::

    $ ./benchmark mock
    # --- General information ---
    # Sensor size: 640x480
    # ROI size: 640x480
    # Exposure time: 0.000010s
    # type      n_frames  n_runs    frames/s        MiB/s
      sync      100       3         29848.98        8744.82
      async     100       3         15739.43        4611.16
