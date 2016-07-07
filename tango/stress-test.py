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
    parser.add_argument('-s', '--software-trigger', action='store_true',
                        default=False,
                        help='Use software trigger instead of auto')

    args = parser.parse_args()

    camera = PyTango.DeviceProxy(args.device)
    camera.exposure_time = 0.0001
    camera.trigger_source = 1 if args.software_trigger else 0
    camera.Start()

    start = time.time()
    size = 0
    acquired = 0

    if HAVE_PROGRESSBAR:
        progress = progressbar.ProgressBar(max_value=args.number, redirect_stdout=True)
    else:
        progress = lambda x: x
        print("Recording {} frames ...".format(args.number))

    for i in progress(range(args.number)):
        try:
            if args.software_trigger:
                camera.Trigger()

            frame = camera.image
            size += frame.nbytes
            acquired += 1
        except:
            pass


        if HAVE_PROGRESSBAR:
            progress.update(i)

    end = time.time()
    camera.Stop()

    print("Frames acquired: {}/{} ({:3.2f} %)".format(acquired, args.number, float(acquired) / args.number * 100))
    print("Bandwidth: {:.3f} MB/s".format(size / (end - start) / 1024. / 1024.))
