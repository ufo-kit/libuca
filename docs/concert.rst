Concert
=======

`Concert`_ is a light-weight control system interface, which can also control ``libuca`` cameras.

.. _Concert: https://github.com/ufo-kit/concert


Installation
------------

In the `official documentation`_ you can read `how to install`_ ``Concert``.

.. _official documentation: https://concert.readthedocs.io/en/latest/
.. _how to install: https://concert.readthedocs.io/en/latest/user/install.html


Usage
-----

``Concert`` can be used from a session and within an integrated ``IPython`` shell or as a library.

In order to create a concert session you should first initialize the session and then start the editor::

    $ concert init session
    $ concert edit session

You can simply add your camera, for example the ``mock`` camera with::    

    from concert.devices.cameras.uca import Camera

    camera = Camera("mock")

and start the session with::

    $ concert start session

The function ``ddoc()`` will give you an overview of all defined devices in the session::

    session > ddoc()
    # ------------------------------------------------------------------------------------------------
    #  Name     Description              Parameters                                                                       
    # ------------------------------------------------------------------------------------------------
    #  camera   libuca-based camera.     Name            Unit     Description                                  
    #                                    buffered        None     info    TRUE if libuca should buffer        
    #           All properties that are                           frames                                       
    #           exported by the                                   locked  no                                  
    #           underlying camera are    exposure_time   second   info    Exposure time in seconds            
    #           also visible.                                     locked  no                                  
    #                                                             lower   -inf second                         
    #                                                             upper   inf second                 
    ...


Getting and setting camera parameters
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can get an overview of the camera parameters by calling the ``dstate()`` function::

    session > dstate()     
    # ---------------------------------------------------------
    #  Name     Parameters                                    
    # ---------------------------------------------------------
    #  camera    buffered                   False             
    #            exposure_time              0.05 second       
    #            fill_data                  True              
    #            frame_rate                 20.0 1 / second   
    #            has_camram_recording       False             
    #            has_streaming              True  
    ...

set the value of a parameter with::

    session > camera.exposure_time = 0.01 * q.s

and check the set value with::

    session > camera.exposure_time
    # <Quantity(0.01, 'second')>

or you can use the ``get()`` and ``set()`` methods::

    session > exposure_time = camera["exposure_time"]
    session > exposure_time.set(0.1 * q.s)
    session > exposure_time.get().result()
    # <Quantity(0.1, 'second')>  

In order to set the trigger source property you can use ``trigger_sources.AUTO``, ``trigger_sources.SOFTWARE`` or ``trigger_sources.EXTERNAL``::

    session > camera.trigger_source = camera.trigger_sources.AUTO


Grabbing frames
~~~~~~~~~~~~~~~

To grab a frame, first start the camera, use the ``grab()`` function and stop the camera afterwards::

    session > camera.start_recording()
    session > frame = camera.grab()
    session > camera.stop_recording()

You get the frame as an array::

    session > frame
    # array([[  0,   0,   0, ...,   0,   0,   0],
    #        [  0,   0,   0, ...,   0,   0,   0],
    #        [  0,   0, 255, ...,   0,   0,   0],
    #        ..., 
    #        [  0,   0,   0, ...,   0,   0,   0],
    #        [  0,   0,   0, ...,   0,   0,   0],
    #        [  0,   0,   0, ...,   0,   0,   0]], dtype=uint8)


Saving state and locking parameters
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can store the current state of your camera with::

    session > camera.stash()
    # <Future at 0x2b8ab10 state=running>

And go back to it again with::

    session > camera.restore()
    # <Future at 0x299f550 state=running>

In case you want to prevent a parameter or all the parameters from being written you can use the ``lock()`` method::

    session > camera["exposure_time"].lock()
    session > camera["exposure_time"].set(1 * q.s)
    # <Future at 0x2bb3d90 state=finished raised LockError>

    # lock all parameters of the camera device
    session > camera.lock()

and to unlock them again, just use the ``unlock()`` method::

    session > camera.unlock()


Concert as a library - more examples
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can also use ``Concert`` as a library.

For example test the bit depth consistency with::

    import numpy as np
    from concert.quantities import q
    from concert.devices.cameras.uca import Camera


    def acquire_frame(camera):
        camera.start_recording()
        frame = camera.grab()
        camera.stop_recording()
        return frame

    def test_bit_depth_consistency(camera):
        camera.exposure_time = 1 * q.s
        frame = acquire_frame(camera)

        bits = camera.sensor_bitdepth
        success = np.mean(frame) < 2**bits.magnitude
        print "success" if success else "higher values than possible"

    camera = Camera("mock")
    test_bit_depth_consistency(camera)

or the exposure time consistency with::

    def test_exposure_time_consistency(camera):
        camera.exposure_time = 1 * q.ms
        first = acquire_frame(camera)

        camera.exposure_time = 100 * q.ms
        second = acquire_frame(camera)

        success = np.mean(first) < np.mean(second)
        print "success" if success else "mean image value is lower than expected"


Official Documentation
~~~~~~~~~~~~~~~~~~~~~~

If you have more questions or just want to know more about ``Concert``, please take a look at the very detailed `official documentation`_.

.. _official documentation: https://concert.readthedocs.io/en/latest/
