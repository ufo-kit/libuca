import argparse
import PyTango
import sys
import time
import numpy as np
try:
    import progressbar
    HAVE_PROGRESSBAR = True
except ImportError:
    HAVE_PROGRESSBAR = False


if __name__ == '__main__':
    parser = argparse.ArgumentParser()

    parser.add_argument('device', type=str,
                        help='TANGO device address')
    parser.add_argument('-n', '--number', type=int, default=1000,
                        help='Number of frames to acquire')

    args = parser.parse_args()

    camera = PyTango.DeviceProxy(args.device)
    camera.exposure_time = 0.0001
    camera.Start()

    start = time.time()
    size = 0

    if HAVE_PROGRESSBAR:
        progress = progressbar.ProgressBar(max_value=args.number, redirect_stdout=True)
    else:
        progress = lambda x: x
        print("Recording {} frames ...".format(args.number))

    for i in progress(range(args.number)):
        try:
            frame = camera.image
        except:
            camera.Stop()
            print("Failure after {} frames".format(i))
            sys.exit(0)

        size += frame.nbytes

        if HAVE_PROGRESSBAR:
            progress.update(i)

    end = time.time()
    camera.Stop()

    print("Bandwidth: {:.3f} MB/s".format(size / (end - start) / 1024. / 1024.))
