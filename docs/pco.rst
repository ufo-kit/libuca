
pco
===

string **name**
    Name of the camera

    | *Default:* 

unsigned int **sensor-width**
    Width of the sensor in pixels

    | *Default:* 1
    | *Range:* [1, 4294967295]

unsigned int **sensor-height**
    Height of the sensor in pixels

    | *Default:* 1
    | *Range:* [1, 4294967295]

double **sensor-pixel-width**
    Width of sensor pixel in meters

    | *Default:* 1e-05
    | *Range:* [2.22507385851e-308, 1.79769313486e+308]

double **sensor-pixel-height**
    Height of sensor pixel in meters

    | *Default:* 1e-05
    | *Range:* [2.22507385851e-308, 1.79769313486e+308]

unsigned int **sensor-bitdepth**
    Number of bits per pixel

    | *Default:* 1
    | *Range:* [1, 32]

unsigned int **sensor-horizontal-binning**
    Number of sensor ADCs that are combined to one pixel in horizontal direction

    | *Default:* 1
    | *Range:* [1, 4294967295]

None **sensor-horizontal-binnings**
    Array of possible binnings in horizontal direction

    | *Default:* None

unsigned int **sensor-vertical-binning**
    Number of sensor ADCs that are combined to one pixel in vertical direction

    | *Default:* 1
    | *Range:* [1, 4294967295]

None **sensor-vertical-binnings**
    Array of possible binnings in vertical direction

    | *Default:* None

None **trigger-mode**
    Trigger mode

    | *Default:* <enum UCA_CAMERA_TRIGGER_AUTO of type UcaCameraTrigger>

double **exposure-time**
    Exposure time in seconds

    | *Default:* 1.0
    | *Range:* [0.0, 1.79769313486e+308]

double **frames-per-second**
    Frames per second

    | *Default:* 1.0
    | *Range:* [2.22507385851e-308, 1.79769313486e+308]

unsigned int **roi-x0**
    Horizontal coordinate

    | *Default:* 0
    | *Range:* [0, 4294967295]

unsigned int **roi-y0**
    Vertical coordinate

    | *Default:* 0
    | *Range:* [0, 4294967295]

unsigned int **roi-width**
    Width of the region of interest

    | *Default:* 1
    | *Range:* [1, 4294967295]

unsigned int **roi-height**
    Height of the region of interest

    | *Default:* 1
    | *Range:* [1, 4294967295]

unsigned int **roi-width-multiplier**
    Minimum possible step size of horizontal ROI

    | *Default:* 1
    | *Range:* [1, 4294967295]

unsigned int **roi-height-multiplier**
    Minimum possible step size of vertical ROI

    | *Default:* 1
    | *Range:* [1, 4294967295]

bool **has-streaming**
    Is the camera able to stream the data

    | *Default:* True

bool **has-camram-recording**
    Is the camera able to record the data in-camera

    | *Default:* False

unsigned int **recorded-frames**
    Number of frames recorded into internal camera memory

    | *Default:* 0
    | *Range:* [0, 4294967295]

bool **transfer-asynchronously**
    Specify whether data should be transfered asynchronously using a specified callback

    | *Default:* False

bool **is-recording**
    Is the camera currently recording

    | *Default:* False

bool **is-readout**
    Is camera in readout mode

    | *Default:* False

bool **buffered**
    TRUE if libuca should buffer frames

    | *Default:* False

unsigned int **num-buffers**
    Number of frame buffers in the ring buffer 

    | *Default:* 4
    | *Range:* [0, 4294967295]

bool **sensor-extended**
    Use extended sensor format

    | *Default:* False

unsigned int **sensor-width-extended**
    Width of the extended sensor in pixels

    | *Default:* 1
    | *Range:* [1, 4294967295]

unsigned int **sensor-height-extended**
    Height of the extended sensor in pixels

    | *Default:* 1
    | *Range:* [1, 4294967295]

double **sensor-temperature**
    Temperature of the sensor in degree Celsius

    | *Default:* 0.0
    | *Range:* [-1.79769313486e+308, 1.79769313486e+308]

None **sensor-pixelrates**
    Array of possible sensor pixel rates

    | *Default:* None

unsigned int **sensor-pixelrate**
    Pixel rate

    | *Default:* 1
    | *Range:* [1, 4294967295]

unsigned int **sensor-adcs**
    Number of ADCs to use

    | *Default:* 1
    | *Range:* [1, 2]

unsigned int **sensor-max-adcs**
    Maximum number of ADCs that can be set with "sensor-adcs"

    | *Default:* 1
    | *Range:* [1, 4294967295]

bool **has-double-image-mode**
    Is double image mode supported by this model

    | *Default:* False

bool **double-image-mode**
    Use double image mode

    | *Default:* False

bool **offset-mode**
    Use offset mode

    | *Default:* False

None **record-mode**
    Record mode

    | *Default:* <enum UCA_PCO_CAMERA_RECORD_MODE_SEQUENCE of type UcaPcoCameraRecordMode>

None **storage-mode**
    Storage mode

    | *Default:* <enum UCA_PCO_CAMERA_STORAGE_MODE_FIFO_BUFFER of type UcaPcoCameraStorageMode>

None **acquire-mode**
    Acquire mode

    | *Default:* <enum UCA_PCO_CAMERA_ACQUIRE_MODE_AUTO of type UcaPcoCameraAcquireMode>

bool **fast-scan**
    Use fast scan mode with less dynamic range

    | *Default:* False

int **cooling-point**
    Cooling point of the camera in degree celsius

    | *Default:* 5
    | *Range:* [0, 10]

int **cooling-point-min**
    Minimum cooling point in degree celsius

    | *Default:* 0
    | *Range:* [-2147483648, 2147483647]

int **cooling-point-max**
    Maximum cooling point in degree celsius

    | *Default:* 0
    | *Range:* [-2147483648, 2147483647]

int **cooling-point-default**
    Default cooling point in degree celsius

    | *Default:* 0
    | *Range:* [-2147483648, 2147483647]

bool **noise-filter**
    Noise filter

    | *Default:* False

None **timestamp-mode**
    Timestamp mode

    | *Default:* <enum UCA_PCO_CAMERA_TIMESTAMP_NONE of type UcaPcoCameraTimestamp>

string **version**
    Camera version given as `serial number, hardware major.minor, firmware major.minor'

    | *Default:* 0, 0.0, 0.0

bool **global-shutter**
    Global shutter enabled

    | *Default:* False