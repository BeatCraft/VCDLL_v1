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
    ld_list = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15];  # 0 - 15
    sensor_list = [0, 1, 2, 3, 4, 5, 6, 7]; # 0 - 8
    ld_table =[ [[0, 1, 2], [0, 1, 2], [0, 1, 2], [0, 1, 2],
                 [4, 5, 6], [4, 5, 6], [4, 5, 6], [4, 5, 6]],
                [[8, 9, 10], [8, 9, 10], [8, 9, 10], [8, 9, 10],
                 [12, 13, 14], [12, 13, 14], [12, 13, 14], [12, 13, 14]]
                ]

def BCPreparation(vc):
    pass

def BCCaptureTest():
    print("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@")
    start = time.time()

    # default values are set in VidepCapture::initialize()
    duration = 10000;
    current = 30000;
    exposure = 4500
    gain = 0 # 0 - 31
    
    buff_list = []
    dl = BCDeviceList();
    vc = VCDLL.VidepCapture()
    vc.initialize()

    print("# start ########################################################");
    print("# config")
    for dev_id in dl.device_list:
        for sensor_id in dl.sensor_list:
            vc.select_sensor(dev_id , sensor_id)
            vc.set_exposure(dev_id, exposure)
            vc.set_gain(dev_id, gain)
        #
        for ld_id in dl.ld_list:
            res = vc.select_laser(dev_id, ld_id)
            vc.set_laser_onoff(dev_id, 1)
            res = vc.set_current_laser_setting(dev_id, current, duration)
            vc.set_laser_onoff(dev_id, 0)
        #
    #
    print("# capture")
    for dev_id in dl.device_list:
        print("### device:", dev_id);
        vc.start_device(dev_id);
        for sensor_id in dl.sensor_list:
            print("### sensor:", sensor_id)
            vc.select_sensor(dev_id , sensor_id)
            #
            buff_list = []
            for ld_id in dl.ld_table[dev_id][sensor_id]:
                print("### ld:", ld_id);
                res = vc.select_laser(dev_id, ld_id)
                vc.set_laser_onoff(dev_id, 1)
                buff = vc.get_buffer(dev_id, 10000) # args changes since v0
                vc.set_laser_onoff(dev_id, 0)
                print(type(buff));
                buff_list.append(buff)
            #
            k = 0
            for ld_id in dl.ld_table[dev_id][sensor_id]:
                fname = "cap_" + str(dev_id) + "_" + str(sensor_id) +"_" + str(ld_id) + ".raw";
                print("### fname:",fname);
                image = ctypes.string_at(buff_list[k] , 5664 * 4248 * 2)
                with open(fname, 'wb') as f:
                    f.write(image)
                #
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


