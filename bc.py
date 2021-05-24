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
    device_list = [0];  # 0 - 1
    ld_list = [0, 1, 2];  # 0 - 2
    sensor_list = [0, 1, 2, 3]; # 0 - 3

def BCPreparation(vc):
    pass

def BCCaptureTest():
    print("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@")
    start = time.time()

    duration = 10000;
    current = 30000;
    exposure = 4500
    gain = 0 # 0 - 31
    buff_list = []
    
    dl = BCDeviceList();
    vc = VCDLL.VidepCapture()
    vc.initialize()

    print("# start ########################################################");
    for dev_id in dl.device_list:
        print("### device:",dev_id);
        for sensor_id in dl.sensor_list:
            vc.select_sensor(dev_id , sensor_id)
            vc.set_exposure(dev_id, exposure)
            vc.set_gain(dev_id, gain)
        #
        vc.start_device(dev_id);
        for sensor_id in dl.sensor_list:
            print("### sensor:", sensor_id);
            vc.select_sensor(dev_id , sensor_id);
            #vc.set_exposure(dev_id, exposure);
            #vc.set_gain(dev_id, gain);
            buff_list = []
            for ld_id in dl.ld_list:
                print("### ld:",ld_id);
                res = vc.select_laser(dev_id, ld_id):
                res = vc.set_current_laser_setting(dev_id, current, duration)
                vc.set_laser_onoff(dev_id , sensor_id, 1)
                buff = vc.get_buffer(dev_id, sensor_id, 10000)
                vc.set_laser_onoff(dev_id , sensor_id, 0)
                print(type(buff));
                buff_list.append(buff)
            #
            k = 0
            for ld_id in dl.ld_list:
                fname = "cap_" + str(dev_id) + "_" + str(sensor_id) +"_" + str(ld_id) + ".raw";
                print("### fname:",fname);
                image = ctypes.string_at(buff_list[k] , 5664 * 4248 * 2)
                with open(fname, 'wb') as f:
                    f.write(image)
                #pname = fname + ".png"
                #pil = Image.frombytes(mode = 'L', size = (5664, 4248), data = image)
                #pil.save(pname)
                k = k + 1
            #
        # for sensor_id
        vc.stop_device(dev_id);
    # for dev_id
    vc.terminate()
    #
    elapsed_time = time.time() - start
    print ("elapsed_time:{0}".format(elapsed_time) + "[sec]")
#
if __name__=='__main__':
    sts = BCCaptureTest();
    sys.exit(sts)
#


