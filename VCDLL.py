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
#
#
#
DEBUG = 0
#
NUM_DEVICE = 8
NUM_SENSOR = 4
NUM_LD = 4
#
# device : CX3 based USB Capture device
#           (dev file index, dev_object ptr, sn_gen, sn_module, sn_position)
# sensor : OVT CMOS Camera Sensor
#
class VidepCapture():
    def __init__(self, path=None):
        if DEBUG:
            print("VidepCapture::__init__()")
        #
        self._vcdll = None
        self._dev_list = []
        # keep valus for all LDs
        self._ld_current_list = []
        self._ld_durration_list = []

        if path is None:
            path = "/home/root/VCDLL_v1/vcdll/libVCDLL.so"
        #
        if sys.platform.startswith('linux'):
            if DEBUG:
                print("linux")
            #
        else:
            print("error : platform is not suppoerted")
            return None
        #
        try:
            self._vcdll = ctypes.cdll.LoadLibrary(path)
        except:
            print("error : DLL path")
            self._vcdll = None
            return None
        #
        #
        #
        self._vcdll.Dev_NewObject.restype = ctypes.c_long
        self._vcdll.Dev_EnumDevice.restype = ctypes.c_long
        self._vcdll.Dev_GetDeviceNameByIndex.restype = ctypes.c_bool
        self._vcdll.Dev_FormatCount.restype = ctypes.c_long
        self._vcdll.Dev_GetFormatbyIndex.restype = ctypes.c_bool
        self._vcdll.Dev_SetFormatIndex.restype = ctypes.c_bool
        self._vcdll.Dev_GetCurrentFormatIndex.restype = ctypes.c_bool
        self._vcdll.Dev_IsSupportStillCapture.restype = ctypes.c_bool
        self._vcdll.Dev_StillFormatCount.restype = ctypes.c_long
        self._vcdll.Dev_GetStillFormatbyIndex.restype = ctypes.c_bool
        self._vcdll.Dev_SetStillFormatIndex.restype = ctypes.c_bool
        self._vcdll.Dev_Start.restype = ctypes.c_bool
        self._vcdll.Dev_Stop.restype = ctypes.c_bool
        self._vcdll.Dev_GetExposureRange.restype = ctypes.c_bool
        self._vcdll.Dev_GetExposure.restype = ctypes.c_bool
        self._vcdll.Dev_SetExposure.restype = ctypes.c_bool
        self._vcdll.Dev_GetGainRange.restype = ctypes.c_bool
        self._vcdll.Dev_GetGain.restype = ctypes.c_bool
        self._vcdll.Dev_SetGain.restype = ctypes.c_bool
        self._vcdll.Dev_GetCurrentLaserNumber.restype = ctypes.c_bool
        self._vcdll.Dev_SetCurrentLaserNumber.restype = ctypes.c_bool
        #self._vcdll.Dev_GetSensorPower.restype = ctypes.c_bool
        #self._vcdll.Dev_SetSensorPower.restype = ctypes.c_bool
        self._vcdll.Dev_GetSensorReadoutDelay.restype = ctypes.c_bool
        self._vcdll.Dev_SetSensorReadoutDelay.restype = ctypes.c_bool
        self._vcdll.Dev_GetSensorFlip.restype = ctypes.c_bool
        self._vcdll.Dev_SetSensorFlip.restype = ctypes.c_bool
        self._vcdll.Dev_GetCanStillCapture.restype = ctypes.c_bool
        #self._vcdll.Dev_GetStillCaptureCount.restype = ctypes.c_bool
        #self._vcdll.Dev_SetStillCaptureCount.restype = ctypes.c_bool
        self._vcdll.Dev_GetSerialNumber.restype = ctypes.c_bool
        self._vcdll.Dev_SetSerialNumber.restype = ctypes.c_bool
        self._vcdll.Dev_GetSensorDetected.restype = ctypes.c_bool
        self._vcdll.Dev_GetCurrentSensorNumber.restype = ctypes.c_bool
        self._vcdll.Dev_SetCurrentSensorNumber.restype = ctypes.c_bool
        #self._vcdll.Dev_GetCurrentLaserSetting.restype = ctypes.c_bool
        self._vcdll.Dev_SetCurrentLaserSetting.restype = ctypes.c_bool
        #self._vcdll.Dev_GetLaserOnOff.restype = ctypes.c_bool
        #self._vcdll.Dev_SetLaserOnOff.restype = ctypes.c_bool
        self._vcdll.Dev_StillTrigger.restype = ctypes.c_bool
        self._vcdll.Dev_GetStillBuffer.restype  = ctypes.c_long
        self._vcdll.Dev_GetBuffer.restype = ctypes.c_long
    #
    
    def __del__(self):
        if DEBUG:
            print("VidepCapture::__del__()")
        #
        if self._vcdll is None:
            pass
        else:
            self._vcdll.Dev_Terminate()
            del self._vcdll
            self._vcdll = None
        #
    
    def initialize(self):
        if DEBUG:
            print("VidepCapture::initialize()")
        #
        if self._vcdll is None:
            return 1
        #
        self._vcdll.Dev_Initialize() # a ligacy from Win32
        # allocate objects for USB devices
        num_device = self.enumerate_devices()
        for i in range(num_device):
            buf = ctypes.create_string_buffer(10)
            obj = ctypes.c_void_p(self._vcdll.Dev_NewObject(i))
            self._vcdll.Dev_GetSerialNumber(obj, buf, 8)
            line = "%s" % buf.value.decode()
            sn = (obj, line[:1], line[1:6], line[6:8])
            self._dev_list.append(sn)
        #
        # sort objects by SN (indexing devices depends on power-up timing)
        self._dev_list.sort(key=itemgetter(3))
        
        current = 0 #50
        duration = 0 #10000
        exposure = 0
        gain = 0
        for i in range(num_device):
            self._ld_current_list.append([current]*NUM_LD)
            self._ld_durration_list.append([duration]*NUM_LD)
            #
            num_sensor = self.get_sensor_detected(i)
            #self._ld_exposure_list.append([duration]*num_sensor)
            #self._ld_gain_list.append([gain]*num_sensor)
        #
        return 0

    def enumerate_devices(self):
        if DEBUG:
            print("VidepCapture::enumerate_devices()")
        if self._vcdll is None:
            return 1
        #
        return self._vcdll.Dev_EnumDevice()

    def get_sensor_detected(self, d_id):
        if DEBUG:
            print("VidepCapture::get_sensor_detected(%d)" % d_id)
        #
        obj = self._dev_list[d_id][0]
        if obj:
            pass
        else:
            return 1
        #
        detected = ctypes.c_long(0)
        self._vcdll.Dev_GetSensorDetected(obj, ctypes.pointer(detected))
        v = detected.value
        if v==0b0001:
            return 1
        elif v==0b0011:
            return 2
        elif v==0b0111:
            return 3
        elif v==0b1111:
            return 4
        #
        return 0

    # d_id is an uint (we assume 0 7 in current generation devices),
    # returns 0 if success
    def start_device(self, d_id):
        if DEBUG:
            print("VidepCapture::start_device()")
        #
        if self._vcdll is None:
            return 1
        #
        obj = self._dev_list[d_id][0]
        if obj:
            pass
        else:
            return 1
        #
        self._vcdll.Dev_SetFormatIndex(obj, 0)
        self._vcdll.Dev_SetStillFormatIndex(obj, 0)
        self._vcdll.Dev_Start(obj)
        return 0

    def get_device_sn(self, d_id):
        if DEBUG:
            print("VidepCapture::get_device_sn(%d)" % (d_id))
        #
        if self._vcdll is None:
            return 1
        #
        buf = ctypes.create_string_buffer(10)
        obj = self._dev_list[d_id][0]
        self._vcdll.Dev_GetSerialNumber(obj, buf, 8)
        sn = "%s" % buf.value.decode()
        return sn

    def set_device_sn(self, d_id , sn):
        if DEBUG:
            print("VidepCapture::set_device_sn(%d, %s)" % (d_id, sn))
        #
        if self._vcdll is None:
            return 1
        #
        if len(sn)>8:
            return 1
        #
        buf = ctypes.create_string_buffer(sn)
        obj = self._dev_list[d_id][0]
        self._vcdll.Dev_SetSerialNumber(obj, buf, 8)

        return 0
    
    # all sensors on the device have the same gain setting
    def set_gain(self, d_id, gain):
        if DEBUG:
            print("VidepCapture::set_gain(%d, %d)" % (d_id, gain))
        #
        if self._vcdll is None:
            return 1
        #
        obj = self._dev_list[d_id][0]
        if obj:
            pass
        else:
            return 1
        #
        self._vcdll.Dev_SetGain(obj, ctypes.c_long(gain))
        return 0 # success

    def set_exposure(self, d_id, exposure):
        if DEBUG:
            print("VidepCapture::set_exposure(%d, %d)" % (d_id, exposure))
        #
        if self._vcdll is None:
            return 1
        #
        obj = self._dev_list[d_id][0]
        if obj:
            pass
        else:
            return 1
        #
        self._vcdll.Dev_SetExposure(obj, ctypes.c_long(exposure))
        return 0 # success
        
    def get_laser_current(self, d_id, l_id):
        if DEBUG:
            print("VidepCapture::get_laser_current(%d, %d)" % (d_id, l_id))
        #
        print("get_laser_current() is obsolute. use get_laser_setting() instead.")
        return 0
    
    def set_laser_current(self, d_id, l_id, current):
        if DEBUG:
            print("VidepCapture::set_laser_current(%d, %d, %d)" % (d_id, l_id, current))
        #
        print("set_laser_current() is obsolute. use set_laser_setting() instead.")
        return 1

    # 'current' means selected.
    # arg:current is electric current
    def set_current_laser_setting(self, d_id, current, duration):
        if DEBUG:
            print("VidepCapture::set_current_laser_setting(%d, %d, %d)" % (d_id, l_id, current, duration))
        #
        #print("set_current_laser_setting(%d, %d, %d)" % (d_id, current, duration))
        if self._vcdll is None:
            return 1
        #
        obj = self._dev_list[d_id][0]
        if obj:
            pass
        else:
            return 1
        #
        self._vcdll.Dev_SetCurrentLaserSetting(obj, ctypes.c_long(current), ctypes.c_long(duration))
        return 0

    def set_current_laser_number(self, d_id, l_id):
        if DEBUG:
            print("VidepCapture::set_current_laser_number(%d, %d, %d)" % (d_id, l_id))
        #
        #print("set_current_laser_number(%d, %d)" % (d_id, l_id))
        if self._vcdll is None:
            return 1
        #
        obj = self._dev_list[d_id][0]
        if obj:
            pass
        else:
            return 1
        #
        self._vcdll.Dev_SetCurrentLaserNumber(obj, ctypes.c_long(l_id))
        return 0
        
    def select_laser(self, d_id, l_id):
        if DEBUG:
            print("VidepCapture::select_laser(%d, %d, %d)" % (d_id, l_id))
        #
        return self.set_current_laser_number(d_id, l_id)

    def set_laser_setting(self, d_id, l_id, current, duration):
        if DEBUG:
            print("VidepCapture::set_laser_setting(%d, %d, %d, %d)" % (d_id, l_id, current, duration))
        #
        if self._vcdll is None:
            return 1
        #
        obj = self._dev_list[d_id][0]
        if obj:
            pass
        else:
            return 1
        #
        ret = self.set_current_laser_number(d_id, l_id)
        if ret>0:
            return 1
        #
        return self.set_current_laser_setting(d_id, current, duration)

    def select_sensor(self, d_id , s_id):
        if DEBUG:
            print("VidepCapture::select_sensor(%d, %d)" % (d_id, s_id))
        #
        if self._vcdll is None:
            return 1
        #
        obj = self._dev_list[d_id][0]
        # select a sensor to capture
        self._vcdll.Dev_SetCurrentSensorNumber(obj, s_id)
        
        return 0

    # Acquisition:
    #   triggers a sequence of acquisitions
    #   on the s_id given the laser channel configuration
    def trigger(self, d_id , s_id):
        if DEBUG:
            print("VidepCapture::trigger(%d, %d)" % (d_id, s_id))
        #
        if self._vcdll is None:
            return 1
        #
        obj = self._dev_list[d_id][0]
        # select a sensor to capture
        #self._vcdll.Dev_SetCurrentSensorNumber(obj, s_id)
        # capture
        self._vcdll.Dev_StillTrigger(obj)
        return 0 # success
        
    def get_buffer(self, d_id, s_id):
        if DEBUG:
            print("VidepCapture::get_buffer(%d, %d)" % (d_id, s_id))
        #
        buffer = ctypes.c_void_p(None)
        if self._vcdll is None:
            return buffer
        #
        timeout = 2000 # dummy
        to = ctypes.c_int(timeout)
        #
        obj = self._dev_list[d_id][0]
        p = self._vcdll.Dev_GetStillBuffer(obj, to)
        if p:
            buffer = p
        #
        return buffer
        # returns 0 if success, …,
        # buffer pointer point to an array in RAM of size 4 x 5664 x 4248 x 2 bytes
        #       NOTE: each sensor needs this size buffer,
        #               i.e. 4x 4x 5664x4248x2 bytes are needed per camera)
        #       NOTE: it’s the responsibility of the ctrl app to read out only the new (valid) data
        #             and ignore the old data in the buffer,
        #               i.e. the app has to make sure it strictly follows “trigger getbuffer” sequences
        #               on any sensor or returns 0
        #return 0

    # stop_device(), then terminate()
    def stop_device(self, d_id):
        if DEBUG:
            print("VidepCapture::stop_device(%d)" % (d_id))
        #
        if self._vcdll is None:
            return 1
        #
        obj = self._dev_list[d_id][0];
        self._vcdll.Dev_Stop(obj);
        return 0 # success

    def terminate(self):
        if DEBUG:
            print("VidepCapture::terminate()")
        #
        if self._vcdll is None:
            return 1
        #
        self._vcdll.Dev_Terminate()
        del self._vcdll
        self._vcdll = None
        return 0 # success
#
#
#
def main():
    argvs = sys.argv
    argc = len(argvs)
    #
    d_id = 0 # decice id : 0 - 7
    s_id = 0 # sensor id : 0 - 3
    l_id = 0 # laser id : 0 - 3 (LDC suppots 4 channels but LS has only 3 LDs.)
    current = 50000
    duration = 10000
    exposure = 100
    gain = 0
    
    # buf0 : size = 5664 x 4248 x 2 x 4
    #   sensor_0+LD0, sensor_0+LD1, sensor_0+LD2, sensor_0+LD3 @device_0
    # buf1 : size = 5664 x 4248 x 2 x 4
    #   sensor_1+LD0, sensor_1+LD1, sensor_1+LD2, sensor_1+LD3 @device_0
    # buf2 : size = 5664 x 4248 x 2 x 4
    #   sensor_2+LD0, sensor_2+LD1, sensor_2+LD2, sensor_2+LD3 @device_0
    # buf3 : size = 5664 x 4248 x 2 x 4
    #   sensor_3+LD0, sensor_3+LD1, sensor_3+LD2, sensor_3+LD3 @device_0
    buf0 = ctypes.c_void_p(None)
    buf1 = ctypes.c_void_p(None)
    buf2 = ctypes.c_void_p(None)
    buf3 = ctypes.c_void_p(None)
    
    dll_path = os.getcwd() + "/libVC96.so"
    vc = VidepCapture(dll_path)
    vc = VidepCapture()
    # objects for all connected decives are allocated.
    vc.initialize()
    #print vc.get_device_sn(0)
    #print vc.set_device_sn(0, "20000000")
    #print vc.get_device_sn(0)

    return 0
    
    
    # laser setting
    vc.set_laser_setting(d_id , l_id , current, duration)
    l_id = 1
    vc.set_laser_setting(d_id , l_id , current, duration)
    l_id = 2
    vc.set_laser_setting(d_id , l_id , current, duration)
    l_id = 3
    vc.set_laser_setting(d_id , l_id , 0, 0)
    l_id = 0
    vc.select_laser(d_id, l_id)
    #
    vc.start_device(d_id)
    #
    s_id = 0
    vc.select_sensor(d_id , s_id)
    vc.set_exposure(d_id, exposure)
    vc.set_gain(d_id, gain)
    #
    vc.select_laser(d_id, 0)
    vc.trigger(d_id, s_id) # capture images with different LDs sequencialy.
    vc.get_buffer(d_id, s_id, buf0)

    vc.select_laser(d_id, 1)
    vc.trigger(d_id, s_id) # capture images with different LDs sequencialy.
    vc.get_buffer(d_id, s_id, buf0)

    vc.select_laser(d_id, 2)
    vc.trigger(d_id, s_id) # capture images with different LDs sequencialy.
    vc.get_buffer(d_id, s_id, buf0)
    #
    #
    #
    vc.select_laser(d_id, 0)
    #
    s_id = 1
    vc.select_sensor(d_id , s_id)
    vc.set_exposure(d_id, exposure)
    vc.set_gain(d_id, gain)
    vc.trigger(d_id, s_id)
    vc.get_buffer(d_id, s_id, buf1)
    #
    s_id = 2
    vc.select_sensor(d_id , s_id)
    vc.set_exposure(d_id, exposure)
    vc.set_gain(d_id, gain)
    vc.trigger(d_id, s_id)
    vc.get_buffer(d_id, s_id, buf2)
    #
    s_id = 3
    vc.select_sensor(d_id , s_id)
    vc.set_exposure(d_id, exposure)
    vc.set_gain(d_id, gain)
    vc.trigger(d_id, s_id)
    vc.get_buffer(d_id, s_id, buf3)
    #
    vc.stop_device(d_id)
    vc.terminate()
    #
    return 0
#
#
#
if __name__=='__main__':
    sts = main()
    sys.exit(sts)
#
