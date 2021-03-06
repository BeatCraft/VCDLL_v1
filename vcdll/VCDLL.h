/**
   @file VCDLL.h
   カメラデバイス制御ライブラリ
 */
#ifndef __VCDLL_H
#define __VCDLL_H

#include <time.h>		// 2018/07/31
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <malloc.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>

#include <linux/videodev2.h>
#include <linux/uvcvideo.h>
#include <linux/usb/video.h>

#ifdef HAVE_BYTESWAP_H
#include <byteswap.h>
#endif

#define LFI4_FMT_MAX_WIDTH		5664
#define LFI4_FMT_MAX_HEIGHT 	4248
#define LFI4_FMT_SRC_SIZE		((LFI4_FMT_MAX_WIDTH + (LFI4_FMT_MAX_WIDTH / 4)) * LFI4_FMT_MAX_HEIGHT)


struct laser_param {
	unsigned long lcurrent;
	unsigned long lduration;
};

struct io_sensor_register {
	unsigned short addr;
	unsigned short length;
	unsigned short value;
};

#define LFI4_VIDIOC_SET_EXPO _IOWR('V', BASE_VIDIOC_PRIVATE, unsigned long)
#define LFI4_VIDIOC_GET_EXPO _IOWR('V', BASE_VIDIOC_PRIVATE + 1, unsigned long)
#define LFI4_VIDIOC_SET_GAIN _IOWR('V', BASE_VIDIOC_PRIVATE + 2, unsigned long)
#define LFI4_VIDIOC_GET_GAIN _IOWR('V', BASE_VIDIOC_PRIVATE + 3, unsigned long)
#define LFI4_VIDIOC_SET_ORIENT _IOWR('V', BASE_VIDIOC_PRIVATE + 4, unsigned long)
#define LFI4_VIDIOC_GET_ORIENT _IOWR('V', BASE_VIDIOC_PRIVATE + 5, unsigned long)

#define LFI4_VIDIOC_SET_SENSOR _IOWR('V', BASE_VIDIOC_PRIVATE + 6, unsigned long)
#define LFI4_VIDIOC_GET_SENSOR _IOWR('V', BASE_VIDIOC_PRIVATE + 7, unsigned long)

#define LFI4_VIDIOC_SET_LASER	_IOWR('V', BASE_VIDIOC_PRIVATE + 8, unsigned long)
#define LFI4_VIDIOC_GET_LASER	_IOWR('V', BASE_VIDIOC_PRIVATE + 9, unsigned long)
#define LFI4_VIDIOC_SET_SENSOR_POWER	_IOWR('V', BASE_VIDIOC_PRIVATE + 10, unsigned long)
#define LFI4_VIDIOC_GET_SENSOR_POWER	_IOWR('V', BASE_VIDIOC_PRIVATE + 11, unsigned long)

#define LFI4_VIDIOC_GET_CAN_CAPTURE		_IOWR('V', BASE_VIDIOC_PRIVATE + 12, unsigned long)
#define LFI4_VIDIOC_SET_CAPTURE_COUNT	_IOWR('V', BASE_VIDIOC_PRIVATE + 13, unsigned long)
#define LFI4_VIDIOC_GET_CAPTURE_COUNT	_IOWR('V', BASE_VIDIOC_PRIVATE + 14, unsigned long)
#define LFI4_VIDIOC_GET_SENSOR_DETECT	_IOWR('V', BASE_VIDIOC_PRIVATE + 15, unsigned long)

#define LFI4_VIDIOC_SET_LASER_PARAM		_IOWR('V', BASE_VIDIOC_PRIVATE + 16, struct laser_param)
#define LFI4_VIDIOC_GET_LASER_PARAM		_IOWR('V', BASE_VIDIOC_PRIVATE + 17, struct laser_param)

#define LFI4_VIDIOC_SET_LASER_ONOFF		_IOWR('V', BASE_VIDIOC_PRIVATE + 18, unsigned long)
#define LFI4_VIDIOC_GET_LASER_ONOFF		_IOWR('V', BASE_VIDIOC_PRIVATE + 19, unsigned long)

#define LFI4_VIDIOC_SET_SERIAL		_IOWR('V', BASE_VIDIOC_PRIVATE + 20, unsigned long)
#define LFI4_VIDIOC_GET_SERIAL		_IOWR('V', BASE_VIDIOC_PRIVATE + 21, unsigned long)

#define LFI4_VIDIOC_SET_SENSOR_REGADDR	_IOWR('V', BASE_VIDIOC_PRIVATE + 22, struct io_sensor_register*)
#define LFI4_VIDIOC_GET_SENSOR_REGADDR	_IOWR('V', BASE_VIDIOC_PRIVATE + 23, struct io_sensor_register*)
#define LFI4_VIDIOC_SET_SENSOR_REGVALUE	_IOWR('V', BASE_VIDIOC_PRIVATE + 24, struct io_sensor_register*)
#define LFI4_VIDIOC_GET_SENSOR_REGVALUE	_IOWR('V', BASE_VIDIOC_PRIVATE + 25, struct io_sensor_register*)

#define CLEAR(x) memset (&(x), 0, sizeof(x))  /* メモリの明示的な初期化 */

#define MAX_DEVICE_NUM		2
#define MAX_FORMAT_NUM		32

#define DEVICE_NAME_LENGTH	255
#define DEVICE_NAME_PREFIX	"BC LFI"

typedef struct _GUID {
    unsigned long  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[ 8 ];
} GUID;

typedef struct {
	char	deviceName[DEVICE_NAME_LENGTH + 1];
	char	serialNumber[DEVICE_NAME_LENGTH + 1];
	char	devicePath[DEVICE_NAME_LENGTH + 1];
	char	subDevicePath[DEVICE_NAME_LENGTH + 1];
} SensorDevice;

typedef struct {
	long			numDevice;
	SensorDevice	devices[MAX_DEVICE_NUM];
} SensorDeviceInfo;

typedef struct {
	char	deviceName[DEVICE_NAME_LENGTH + 1];
	char	serialNumber[DEVICE_NAME_LENGTH + 1];
} SensorDeviceName;

typedef struct {
	GUID			formatType;
	long			width;
	long			height;
	long			fps;
	long			bitPerPixel;
	long			imageSize;
	long			index;
} CapFormat;

typedef struct {
	long		numFormat;
	long		currentFormatIndex;
	CapFormat	formats[MAX_FORMAT_NUM];
} CapFormatInfo;


typedef struct {
  unsigned char * start;
  size_t          length;
} VideoBuffer;

typedef struct {
	int					devfd;
	int					subdevfd;
	int					currentDeviceIndex;
	
	VideoBuffer*		buffers;
	VideoBuffer*		still_buffers;
	
	CapFormatInfo		videoFormatInfo;
	CapFormatInfo		stillFormatInfo;
	
	
	unsigned long    n_buffers;
	unsigned long    still_n_buffers;
	
	unsigned long    num_captures;
	unsigned long	currentImageSize;
	unsigned long	currentStillImageSize;
	
	//void*           gp; /* capture image 画像データ退避用メモリ */
	//void*           g_stillp;
	
	bool			streaming;
	bool			streamingOn;
	
	bool			waitBuffer;
	
	void*			priv;
	
} DevObject;

void Dev_Initialize(void);												  /* ライブラリの初期化処理 */
void Dev_Terminate(void);												  /* ライブラリの後処理 */
long Dev_EnumDevice(void);												  /*  接続されているデバイスの列挙 */
bool Dev_GetDeviceNameByIndex(int index, SensorDeviceName* name);		  /*  デバイス名の取得 */

DevObject* Dev_NewObject(int index);                  					  /* カメラデバイスのオブジェクトを生成 */
void Dev_Dealloc(DevObject* self);                                        /* カメラデバイスオブジェクトの破棄 */
long Dev_FormatCount(DevObject* self);                                    /* 接続されたカメラがサポートする動画のフォーマットの数を返す */
bool Dev_GetFormatbyIndex(DevObject* self, int index, CapFormat* info);   /* サポートするフォーマットの詳細情報を取得 */
bool Dev_SetFormatIndex(DevObject* self, int index);          		  /* プレビュー(動画)で使用するフォーマットを指定 */
bool Dev_GetCurrentFormatIndex(DevObject* self, int* index);         	  /* 現在の動画フォーマットの設定を取得 */
bool Dev_IsSupportStillCapture(DevObject* self);                          /* センサが静止画キャプチャをサポートしているかを取得 */
long Dev_StillFormatCount(DevObject* self);                               /* センサーがサポートする静止画フォーマットの数を取得 */
bool Dev_GetStillFormatbyIndex(DevObject* self, int index,
                               CapFormat* info);        				  /* 静止画キャプチャのフォマットの詳細情報を取得する */
bool Dev_SetStillFormatIndex(DevObject* self, int index);            	  /* 静止画キャプチャのフォーマットを指定 */
bool Dev_Start(DevObject* self);                                          /* 動画の描画を開始 */
bool Dev_Stop(DevObject* self);                                           /* 動画の描画を停止 */
void* Dev_GetBuffer(DevObject* self, int milsec);                		  /* 動画のイメージデータを取得 */
void* Dev_GetBufferRaw8(DevObject* self, int milsec); 					  /* 動画のイメージデータを8bitで取得 */
bool Dev_StillTrigger(DevObject* self);                                   /* 静止画イメージデータ取得のためにセンサにトリガを送信 */
void* Dev_GetStillBuffer(DevObject* self, int milsec);           		  /* 静止画イメージデータを取得 */
bool Dev_GetExposureRange(DevObject* self,
						  long* min, long* max,
                          long* step, long* def);               		  /* センサの露出コントロールのパラメータを取得 */
bool Dev_GetExposure(DevObject* self, long* exposure);               	  /* センサの露出コントロールの現在の値を取得 */
bool Dev_SetExposure(DevObject* self, long exposure);           		  /* センサの露出コントロールの値を設定 */
bool Dev_GetGainRange(DevObject* self,
					  long* min, long* max,
                      long* step, long* def);                   		  /* センサのゲインコントロールのパラメータを取得 */
bool Dev_GetGain(DevObject* self, long* gain);                       	  /* センサのゲインコントロールの値を取得 */
bool Dev_SetGain(DevObject* self, long gain);                   		  /* センサのゲインコントロールの値を取得 */
bool Dev_GetCurrentLaserNumber(DevObject* self, long* number);       	  /* 静止画キャプた時に発行させるレーザー番号を取得 */
bool Dev_SetCurrentLaserNumber(DevObject* self, long number);   		  /* 静止画キャプチャ時に発行させるレーザー番号を設定する */
                                                               		   	  /* bool DevGetSensorPower(unsigned short * onOff); */
                                                               			  /* bool Dev_SetSensorPower(const unsigned short onOff) */
bool Dev_GetSensorReadoutDelay(DevObject* self, long* delay);        	  /* センサの静止画キャプチャ時のReadOutが開始されるまでのディレイ(tRDOUT)の現在の値を取得 */
bool Dev_SetSensorReadoutDelay(DevObject* self, long delay);    		  /* センサの静止画キャプチャ時のReadOutが開始されるまでのディレイ(tRDOUT)を設定 */
bool Dev_GetSensorFlip(DevObject* self,
					  long* horizontalMirror,
                      long* verticalFlip);         						  /* センサのキャプチャイメージの反転状態を取得 */
bool Dev_SetSensorFlip(DevObject* self,
					   long horizontalMirror,
                       long verticalFlip);     							  /* センサのキャプチャイメージの反転状態を設定 */
bool Dev_GetCanStillCapture(DevObject* self,
							long* canStillCapture); 					  /* センサが静止画キャプチャが可能な状態かを問い合わせる */
bool Dev_GetSensorPower(DevObject* self, long* onOff);					  /* センサの電源のOn/Off状態を取得 */
bool Dev_SetSensorPower(DevObject* self, long onOff);					  /* センサの電源のOn/Off状態を設定 */

// 2.2.0
bool Dev_GetSensorDetected(DevObject* self, long* detected);				  		/* センサの接続状態を取得 */
bool Dev_GetCurrentSensorNumber(DevObject* self, long* number);						/* 現在設定されているセンサ番号の取得 */
bool Dev_SetCurrentSensorNumber(DevObject* self, long number);						/* センサ番号の設定 */
bool Dev_GetCurrentLaserSetting(DevObject* self, long* current, long* duration);	/* 現在のレーザー設定の取得 */
bool Dev_SetCurrentLaserSetting(DevObject* self, long current, long duration);		/* レーザの設定 */
bool Dev_GetLaserOnOff(DevObject* self, long* onOff);								/* レーザーのOn/Offの取得 */
bool Dev_SetLaserOnOff(DevObject* self, long onOff);								/* レーザーのOn/Offの設定 */
bool Dev_GetSensorRegister(DevObject* self, unsigned short addr, 
											unsigned short length,
			 								unsigned short* value);					/* センサーのレジスタ値の取得 */
bool Dev_SetSensorRegister(DevObject* self, unsigned short addr, 
											unsigned short length, 
											unsigned short value);					/* センサーのレジスタ値の設定 */
bool Dev_GetSerialNumber(DevObject* self, void* buff, long length);					/* シリアル番号(文字列)の取得 */
bool Dev_SetSerialNumber(DevObject* self, void* buff, long length);					/* シリアル番号(文字列)の設定 */



#endif
