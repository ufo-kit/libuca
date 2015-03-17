## Installation

Install the server script with

    $ python setup.py install

and create a new TANGO server `Uca/xyz` with a class named `Camera`.


## Usage

Before starting the server, you have to create a new device property `camera`
which specifies which camera to use. If not set, the `mock` camera will be used
by default.

Start the device server with

    $ Uca foo

You should be able to manipulate camera attributes like `exposure_time` and the
like and store frames using a `Start`, `Store`, `Stop` cycle.

```python
import PyTango

camera = PyTango.DeviceProxy("foo/Camera/mock")
camera.exposure_time = 0.1
camera.Start()
camera.Store('foo.tif')
camera.Stop()
```
