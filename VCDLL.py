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
DEBUG = 1
#
NUM_DEVICE = 2
NUM_SENSOR = 16
NUM_LD = 16
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
        self._sensor_list = []
        # keep valus for all LDs
        self._ld_current_list = []
        self._ld_durration_list = []
        self._selected_sensor_id = 0

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
        # void Dev_Initialize(void);
        # void Dev_Terminate(void);
        #long Dev_EnumDevice(void);
        self._vcdll.Dev_EnumDevice.restype = ctypes.c_long
        # bool Dev_GetDeviceNameByIndex(int index, SensorDeviceName* name);
        # DevObject* Dev_NewObject(int index);
        self._vcdll.Dev_NewObject.restype = ctypes.c_void_p
        self._vcdll.Dev_NewObject.argtypes = [ctypes.c_int,]
        # void Dev_Dealloc(DevObject* self);
        # long Dev_FormatCount(DevObject* self);
        # Dev_GetFormatbyIndex(DevObject* self, int index, CapFormat* info);
        # bool Dev_SetFormatIndex(DevObject* self, int index);
        self._vcdll.Dev_SetFormatIndex.restype = ctypes.c_bool
        self._vcdll.Dev_SetFormatIndex.argtypes = [ctypes.c_void_p, ctypes.c_int,]
        # bool Dev_GetCurrentFormatIndex(DevObject* self, int* index);
        self._vcdll.Dev_GetCurrentFormatIndex.restype = ctypes.c_bool
        self._vcdll.Dev_GetCurrentFormatIndex.argtypes = [ctypes.c_void_p, ctypes.c_void_p,]
        # bool Dev_IsSupportStillCapture(DevObject* self);
        # long Dev_StillFormatCount(DevObject* self);
        # bool Dev_GetStillFormatbyIndex(DevObject* self, int index, CapFormat* info);
        # bool Dev_Start(DevObject* self);
        self._vcdll.Dev_Start.restype = ctypes.c_bool
        self._vcdll.Dev_Start.argtypes = [ctypes.c_void_p,]
        # bool Dev_Stop(DevObject* self);
        self._vcdll.Dev_Stop.restype = ctypes.c_bool
        self._vcdll.Dev_Stop.argtypes = [ctypes.c_void_p,]
        # void* Dev_GetBuffer(DevObject* self, int milsec);
        self._vcdll.Dev_GetBuffer.restype = ctypes.c_void_p
        self._vcdll.Dev_GetBuffer.argtypes = [ctypes.c_void_p, ctypes.c_int,]
        #void* Dev_GetBufferRaw8(DevObject* self, int milsec);
        #bool Dev_StillTrigger(DevObject* self);
        self._vcdll.Dev_StillTrigger.restype = ctypes.c_bool
        self._vcdll.Dev_StillTrigger.argtypes = [ctypes.c_void_p,]
        # void* Dev_GetStillBuffer(DevObject* self, int milsec);
        self._vcdll.Dev_GetStillBuffer.restype = ctypes.c_void_p
        self._vcdll.Dev_GetStillBuffer.argtypes = [ctypes.c_void_p, ctypes.c_int,]
        # bool Dev_GetExposureRange(DevObject* self, long* min, long* max, long* step, long* def);
        # bool Dev_GetExposure(DevObject* self, long* exposure);
        # bool Dev_SetExposure(DevObject* self, long exposure);
        self._vcdll.Dev_SetExposure.restype = ctypes.c_bool
        self._vcdll.Dev_SetExposure.argtypes = [ctypes.c_void_p, ctypes.c_long,]
        # bool Dev_GetGainRange(DevObject* self, long* min, long* max, long* step, long* def);
        # bool Dev_GetGain(DevObject* self, long* gain);
        # bool Dev_SetGain(DevObject* self, long gain);
        self._vcdll.Dev_SetGain.restype = ctypes.c_bool
        self._vcdll.Dev_SetGain.argtypes = [ctypes.c_void_p, ctypes.c_long,]
        # bool Dev_GetCurrentLaserNumber(DevObject* self, long* number);
        # bool Dev_SetCurrentLaserNumber(DevObject* self, long number);
        self._vcdll.Dev_SetCurrentLaserNumber.restype = ctypes.c_bool
        self._vcdll.Dev_SetCurrentLaserNumber.argtypes = [ctypes.c_void_p, ctypes.c_long,]
        # bool Dev_GetSensorReadoutDelay(DevObject* self, long* delay);
        # bool Dev_SetSensorReadoutDelay(DevObject* self, long delay);
        # bool Dev_GetSensorFlip(DevObject* self, long* horizontalMirror, long* verticalFlip);
        # bool Dev_SetSensorFlip(DevObject* self, long horizontalMirror, long verticalFlip);
        self._vcdll.Dev_SetSensorFlip.restype = ctypes.c_bool
        self._vcdll.Dev_SetSensorFlip.argtypes = [ctypes.c_void_p, ctypes.c_long, ctypes.c_long,]
        # bool Dev_GetCanStillCapture(DevObject* self, long* canStillCapture);
        # bool Dev_GetSensorPower(DevObject* self, long* onOff);
        # bool Dev_SetSensorPower(DevObject* self, long onOff);
        # bool Dev_GetSensorDetected(DevObject* self, long* detected);
        self._vcdll.Dev_GetSensorDetected.restype = ctypes.c_bool
        self._vcdll.Dev_GetSensorDetected.argtypes = [ctypes.c_void_p, ctypes.c_void_p,]
        # Dev_GetCurrentSensorNumber(DevObject* self, long* number);
        # bool Dev_SetCurrentSensorNumber(DevObject* self, long number);
        self._vcdll.Dev_SetCurrentSensorNumber.restype = ctypes.c_bool
        self._vcdll.Dev_SetCurrentSensorNumber.argtypes = [ctypes.c_void_p, ctypes.c_long,]
        # bool Dev_GetCurrentLaserSetting(DevObject* self, long* current, long* duration);
        # bool Dev_SetCurrentLaserSetting(DevObject* self, long current, long duration);
        self._vcdll.Dev_SetCurrentLaserSetting.restype = ctypes.c_bool
        self._vcdll.Dev_SetCurrentLaserSetting.argtypes = [ctypes.c_void_p, ctypes.c_long, ctypes.c_long,]
        # bool Dev_GetLaserOnOff(DevObject* self, long* onOff);
        # bool Dev_SetLaserOnOff(DevObject* self, long onOff);
        self._vcdll.Dev_SetLaserOnOff.restype = ctypes.c_bool
        self._vcdll.Dev_SetLaserOnOff.argtypes = [ctypes.c_void_p, ctypes.c_long,]
        # bool Dev_GetSensorRegister(DevObject* self, unsigned short addr, unsigned short length, unsigned short* value);
        # bool Dev_SetSensorRegister(DevObject* self, unsigned short addr, unsigned short length, unsigned short value);
        # bool Dev_GetSerialNumber(DevObject* self, void* buff, long length);
        self._vcdll.Dev_GetSerialNumber.restype = ctypes.c_bool
        self._vcdll.Dev_GetSerialNumber.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_long,]
        #bool Dev_SetSerialNumber(DevObject* self, void* buff, long length);
        self._vcdll.Dev_SetSerialNumber.restype = ctypes.c_bool
        self._vcdll.Dev_SetSerialNumber.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_long,]
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
        self._vcdll.Dev_Initialize()
        num_device = self.enumerate_devices()
        if DEBUG:
            print("num_device=%d" % (num_device))
        #
        for i in range(num_device):
            buf = ctypes.create_string_buffer(10)
            obj = ctypes.c_void_p(self._vcdll.Dev_NewObject(i))
            self._vcdll.Dev_SetSensorPower(obj, 1);
            #
            self._vcdll.Dev_GetSerialNumber(obj, buf, 8)
            line = "%s" % buf.value.decode()
            sn = (obj, line[:1], line[1:6], line[6:8])
            self._dev_list.append(sn)
            #
        #
        d_id = 0
        obj = self._dev_list[d_id][0]
        detected = ctypes.c_long(1)
        f = self.get_sensor_detected(d_id)
        f = detected.value
        for i in range(NUM_SENSOR):
            if f & (1<<i):
                self._sensor_list.append(1)
            else:
                self._sensor_list.append(0)
            #
        #
        for j in range(NUM_SENSOR):
            if self.select_sensor(d_id , j)==0:
                self.set_gain(d_id, 0)
                self.set_exposure(d_id, 4500)
                self._vcdll.Dev_SetSensorFlip(obj, ctypes.c_long(1), ctypes.c_long(1))
                #
                self.set_current_laser_number(d_id, j)
                self.set_current_laser_setting(d_id, 0, 10000)
            #
        #
        return 0

    def enumerate_devices(self):
        if DEBUG:
            print("VidepCapture::enumerate_devices()")
        #
        if self._vcdll is None:
            return 1
        #
        return self._vcdll.Dev_EnumDevice()

    def get_sensor_detected(self, d_id):
        if DEBUG:
            print("VidepCapture::get_sensor_detected(%d)" % d_id)
        #
        d_id = 0
        obj = self._dev_list[d_id][0]
        if obj:
            pass
        else:
            return 1
        #
        detected = ctypes.c_long(0)
        self._vcdll.Dev_GetSensorDetected(obj, ctypes.byref(detected))
        return detected.value

    # d_id is an uint (we assume 0 7 in current generation devices),
    # returns 0 if success
    def start_device(self, d_id):
        if DEBUG:
            print("VidepCapture::start_device(%d)" % (d_id))
        #
        if self._vcdll is None:
            return 1
        #
        d_id = 0
        if self._selected_sensor_id>7:
            d_id = 1
        #
        obj = self._dev_list[d_id][0]
        if obj:
            pass
        else:
            return 1
        #
        ret = self._vcdll.Dev_Start(obj)
        if ret:
            return 0
        #
        return 1

    def get_device_sn(self, d_id):
        if DEBUG:
            print("VidepCapture::get_device_sn(%d)" % (d_id))
        #
        if self._vcdll is None:
            return 1
        #
        sn = "0" + "12345" + "67" # dummy
        return sn

    def set_device_sn(self, d_id , sn):
        if DEBUG:
            print("VidepCapture::set_device_sn(%d, %s)" % (d_id, sn))
        #
        if self._vcdll is None:
            return 1
        #
        return 0 # dummy
    
    # all sensors on the device have the same gain setting
    def set_gain(self, d_id, gain):
        if DEBUG:
            print("VidepCapture::set_gain(%d, %d)" % (d_id, gain))
        #
        if self._vcdll is None:
            return 1
        #
        d_id = 0
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
        d_id = 0
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
            print("VidepCapture::set_current_laser_setting(%d, %d, %d)" % (d_id, current, duration))
        #
        if self._vcdll is None:
            return 1
        #
        d_id = 0
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
            print("VidepCapture::set_current_laser_number(%d, %d)" % (d_id, l_id))
        #
        if self._vcdll is None:
            return 1
        #
        d_id = 0
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
            print("VidepCapture::select_laser(%d, %d)" % (d_id, l_id))
        #
        return self.set_current_laser_number(d_id, l_id)

    def set_laser_setting(self, d_id, l_id, current, duration):
        if DEBUG:
            print("VidepCapture::set_laser_setting(%d, %d, %d, %d)" % (d_id, l_id, current, duration))
        #
        if self._vcdll is None:
            return 1
        #
        d_id = 0
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
        d_id = 0
        obj = self._dev_list[d_id][0]
        if self._sensor_list[s_id]:
            self._vcdll.Dev_SetCurrentSensorNumber(obj, s_id)
            self._selected_sensor_id = s_id
            return 0
        #
        return 1

    def set_laser_onoff(self, d_id, sw):
        if DEBUG:
            print("VidepCapture::set_laser_onoff(%d, %d)" % (d_id, sw))
        #
        if self._vcdll is None:
            return 1
        #
        d_id = 0
        obj = self._dev_list[d_id][0]
        self._vcdll.Dev_SetLaserOnOff(obj, ctypes.c_long(sw))
        return 0
        
    def trigger(self, d_id , s_id):
        if DEBUG:
            print("VidepCapture::trigger(%d, %d) : not implemented" % (d_id, s_id))
        #
        # not in use for V1
        return 1
    
    def get_buffer(self, d_id, timeout=10000):
        if DEBUG:
            print("VidepCapture::get_buffer(%d, %d)" % (d_id, timeout))
        #
        buffer = ctypes.c_void_p(None)
        if self._vcdll is None:
            return buffer
        #
        d_id = 0
        if self._selected_sensor_id>7:
            d_id = 1
        #
        obj = self._dev_list[d_id][0]
        buffer = self._vcdll.Dev_GetBuffer(obj, ctypes.c_int(timeout))
        return buffer

    # stop_device(), then terminate()
    def stop_device(self, d_id):
        if DEBUG:
            print("VidepCapture::stop_device(%d)" % (d_id))
        #
        if self._vcdll is None:
            return 1
        #
        d_id = 0
        if self._selected_sensor_id>7:
            d_id = 1
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
    d_id = 0 # device id : 0 or 1 but 1 is dummy device
    s_id = 0 # sensor id : 0 - 15
    l_id = 0 # laser id  : 0 - 15 (4 LS and a LS suppots 4 LDs but only 3 LDs implemented on the V1.)
    current = 30000
    duration = 10000
    #
    dll_path = os.getcwd() + "/vcdll/libVCDLL.so"
    vc = VidepCapture(dll_path)
    vc = VidepCapture()
    #
    vc.initialize()
    vc.select_laser(d_id, l_id)
    vc.set_current_laser_setting(d_id, current, duration)
    if vc.select_sensor(d_id , s_id):
        vc.terminate()
        return 0
    #
    vc.start_device(d_id)
#    vc.set_laser_onoff(d_id, 1)
    buf = vc.get_buffer(d_id, 10000)
#    vc.set_laser_onoff(d_id, 0)
    vc.stop_device(d_id)
    # save
    fname = "cap_" + str(d_id) + "_" + str(s_id) +"_" + str(l_id) + ".raw";
    print("### fname:",fname);
    image = ctypes.string_at(buf , 5664 * 4248 * 2)
    with open(fname, 'wb') as f:
        f.write(image)
    #
    vc.terminate()
    return 0
#
#
#
if __name__=='__main__':
    sts = main()
    sys.exit(sts)
#
