#! /usr/bin/python
# -*- coding: utf-8 -*-
#
# naming rule is:
# https://hg.python.org/peps/file/tip/pep-0008.txt
#
import os, sys, time, math
from stat import *
import random
import copy
import struct
import ctypes
from operator import itemgetter
import time

import VCDLL
#from PIL import Image


class BCDeviceList:
    device_list = [0,1,2,3,4,5,6,7]; #[0,1,2,3,4,5,6,7];    # 0 - 7
    ld_list = [0];  # 0 - 3
    sensor_list = [0,1,2,3]; # 0 - 3

def BCPreparation(vc):

    dl = BCDeviceList();

    duration =10000;
    current = 45000;

    for dev_id in dl.device_list:
        vc.start_device(dev_id);
        res = vc.trigger(dev_id, 0);
        buff = vc.get_buffer(dev_id, 0);
        vc.stop_device(dev_id);

def BCCaptureTest():

    print("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@")
    start = time.time()

    dl = BCDeviceList();

    duration = 20000;
    current = 45000;

    exposure = 4280
    gain = 0

    buff_list = []


    vc = VCDLL.VidepCapture();
    vc.initialize() ;

    #return;

    BCPreparation(vc);


    print("# start ########################################################");
    for dev_id in dl.device_list:
        print("### device:",dev_id);
        for sensor_id in dl.sensor_list:
            vc.select_sensor(dev_id , sensor_id)
            vc.set_exposure(dev_id, exposure)
            vc.set_gain(dev_id, gain)

        vc.start_device(dev_id);
        for sensor_id in dl.sensor_list:
            print("### sensor:",sensor_id);
            vc.select_sensor(dev_id , sensor_id);
            vc.set_exposure(dev_id, exposure);
            vc.set_gain(dev_id, gain);

            buff_list = []
            for ld_id in dl.ld_list:
                print("### ld:",ld_id);
                res = vc.set_laser_setting(dev_id , ld_id , current, duration);

                res = vc.select_laser(dev_id, ld_id);
                res = vc.trigger(dev_id, sensor_id);
                buff = vc.get_buffer(dev_id, sensor_id);
                print(type(buff));
                buff_list.append(buff)
#            print("\n");
#            for ld_id in dl.ld_list:
                fname = "cap_" + str(dev_id) + "_" + str(sensor_id) +"_" + str(ld_id) + ".raw";
                print("### fname:",fname);
                image = ctypes.string_at(buff , 5664 * 4248 * 2)
                #f = open (fname, "wb");
                #f.write(image);
                #f.close();
                with open(fname, 'wb') as f:
                    f.write(image)
                #pname = fname + ".png"
                #pil = Image.frombytes(mode = 'L', size = (5664, 4248), data = image)
                #pil.save(pname)

        vc.stop_device(dev_id);
    vc.terminate()

    elapsed_time = time.time() - start
    print ("elapsed_time:{0}".format(elapsed_time) + "[sec]")
#
if __name__=='__main__':
    sts = BCCaptureTest();
    sys.exit(sts)
#


