
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "../VCDLL.h"

void save_raw_image(char* path, void* buf, unsigned long size)
{
	printf("save_raw_image %s\n", path);
	
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC,
				   S_IRWXU | S_IRGRP | S_IROTH);
				   
	if (fd < 0) {
		printf("can't create %s (error=%d)\n", path, errno);
		return;
	}
	
	write(fd, buf, size);
	close(fd);
}

void laser_test(DevObject* dev)
{
	long i;
	long no;
	long lcurrent, duration, onoff;
	
	printf("Laser control test start!\n");
	
	Dev_SetLaserOnOff(dev, 0);
	
	// laser setting
	for (i = 0; i < 16; i++) {
		Dev_SetCurrentLaserNumber(dev, i);
		Dev_GetCurrentLaserNumber(dev, &no);
		printf("Dev_GetCurrentLaserNumber: %ld\n", no);
		
		lcurrent = (5000 * i) + 10000;
		
		Dev_SetCurrentLaserSetting(dev, lcurrent, 10000);
		Dev_GetCurrentLaserSetting(dev, &lcurrent, &duration);
		printf("Dev_GetCurrentLaserSetting: %ld %ld\n", lcurrent, duration);
	}
	
	for (i = 0; i < 16; i++) {
		printf("Dev_SetCurrentSensorNumber: %ld\n", i);
		Dev_SetCurrentLaserNumber(dev, i);
		
		Dev_SetLaserOnOff(dev, 1);
		Dev_GetLaserOnOff(dev, &onoff);
		printf("Dev_GetLaserOnOff: %ld\n", onoff);
		
		//sleep(1);
		
		Dev_SetLaserOnOff(dev, 0);
		Dev_GetLaserOnOff(dev, &onoff);
		printf("Dev_GetLaserOnOff: %ld\n", onoff);
	}

	Dev_SetCurrentLaserNumber(dev, 0);
	printf("Laser control test done.\n");	
}

void sensor_setup(DevObject* dev, unsigned long sensor_no) 
{
	long gain, exposure;
	long mirror, flip;
	long no;
	/* センサーの選択 */
	Dev_SetCurrentSensorNumber(dev, sensor_no);
	Dev_GetCurrentSensorNumber(dev, &no);
	printf("Dev_GetCurrentSensorNumber: %ld\n", no);
	/* ゲイン */
	Dev_SetGain(dev, 1);
	Dev_GetGain(dev, &gain);
	printf("Dev_GetGain: %ld\n", gain);
	/* 露出 */
	Dev_SetExposure(dev, 4500);
	Dev_GetExposure(dev, &exposure);
	printf("Dev_GetExposure: %ld\n", exposure);
	/* 画像反転 */
	Dev_SetSensorFlip(dev, 1, 1);
	Dev_GetSensorFlip(dev, &mirror, &flip);
	printf("Dev_GetSensorFlip: %ld %ld\n", mirror, flip);
}

int main(int argc, char** argv)
{
	int i, j;
	//bool bRet;
	long no, on, detected;
	char path[256];
	DevObject* dev = NULL;
	DevObject* dev_dummy = NULL;
	
	// Initialize VC Library
	Dev_Initialize();
	
	long devices = Dev_EnumDevice();
	printf("Dev_EnumDevice: devices = %lu\n", devices);
#if 1
	// メインドライバ /dev/video0
	dev = Dev_NewObject(0);
	if (!dev) {
		goto terminate;
	}
	//Dev_Dealloc(dev);
	//Dev_Terminate();
	//return 0;
#endif
#if 1
	// ダミードライバ /dev/video1
	dev_dummy = Dev_NewObject(1);
	if (!dev_dummy) {
		goto dealloc;
	}
	//Dev_Dealloc(dev_dummy);
	//Dev_Terminate();
	//return 0;
#endif	
#define SENSOR_CNT	16
#define CAPTURE_CNT 1

	/*
		V4Lのシーケンスでは最初のストリーム開始時にセンサーの電源オンのコールバックが
		呼ばれるため予め16個分のセンサーのオン・設定を先に行っておく
	*/
	Dev_SetSensorPower(dev, 1);
	Dev_GetSensorPower(dev, &on);
	printf("Dev_GetSensorPower: %ld\n", on);
	
	/* センサーの接続確認  メインドライバ /dev/video0 に対して行う */
	Dev_GetSensorDetected(dev, &detected);
	/* センサーの設定  メインドライバ /dev/video0 に対して行う */
	printf("Dev_GetSensorDetected: %lx\n", detected);
	for (i = 0; i < SENSOR_CNT; i++) {
		if (detected & (1 << i)) {
			sensor_setup(dev, i);
		}
	}
	
#if 1
	/* ストリーム開始 MIPI0 センサー 0〜7 */
	/* センサーの選択  メインドライバ /dev/video0 に対して行う */
	Dev_SetCurrentSensorNumber(dev, 0);
	/* ストリーム開始 MIPI0 センサー 0〜7 */
	Dev_Start(dev);
	
	for (i = 0; i <= 3; i++) {
		if (!(detected & (1 << i))) {
			continue;
		}
	
		/* センサーの選択 */
		Dev_SetCurrentSensorNumber(dev, i);
		Dev_GetCurrentSensorNumber(dev, &no);
		printf("Dev_GetCurrentSensorNumber: %ld\n", no);
		
		for (j = 0; j < CAPTURE_CNT; j++) {
			/* レーザーの選択 */
			//Dev_SetCurrentLaserNumber(dev, j);
			/* レーザーの設定 30mA durationは未対応 */
			//Dev_SetCurrentLaserSetting(dev, 30000, 10000);
			/* レーザーオン */
			//Dev_SetLaserOnOff(dev, 1);
			/* キャプチャー */
			unsigned char* buffer = Dev_GetBuffer(dev, 10000);
			printf("Dev_GetBuffer: 0x%p\n", buffer);
			/* レーザーオフ */
			Dev_SetLaserOnOff(dev, 0);
			if (buffer) {
				/* ファイルへ保存 */
				sprintf(path, "raw1016bit_cam%d_%d.raw", i, j);
				save_raw_image(path, buffer, LFI4_FMT_MAX_WIDTH*LFI4_FMT_MAX_HEIGHT*2);
			}
		}
	}
	
	/* ストリーム終了 */
	Dev_Stop(dev);
#endif
#if 1
	/* ストリーム開始 MIPI1 センサー 8〜15 */
	/* センサーの選択  メインドライバ /dev/video0 に対して行う */
	Dev_SetCurrentSensorNumber(dev, 12);
	/* ストリーム開始 MIPI1 センサー 8〜15 */
	Dev_Start(dev_dummy);
	/* センサー(8〜15)のストリーム開始  メインドライバ /dev/video0 に対して行う */
	Dev_SetEnableSensorStream(dev, 1);
	
	for (i = 12; i <= 15; i++) {
		if (!(detected & (1 << i))) {
			continue;
		}
	
		/* センサーの選択 */
		Dev_SetCurrentSensorNumber(dev, i);
		Dev_GetCurrentSensorNumber(dev, &no);
		printf("Dev_GetCurrentSensorNumber: %ld\n", no);
		
		for (j = 0; j < CAPTURE_CNT; j++) {
			/* レーザーの選択 */
			Dev_SetCurrentLaserNumber(dev, j);
			/* レーザーの設定 30mA durationは未対応 */
//			Dev_SetCurrentLaserSetting(dev, 30000, 10000);
			/* レーザーオン */
//			Dev_SetLaserOnOff(dev, 1);
			/* キャプチャー */
			unsigned char* buffer = Dev_GetBuffer(dev_dummy, 10000);
			printf("Dev_GetBuffer: 0x%p\n", buffer);
			/* レーザーオフ */
//			Dev_SetLaserOnOff(dev, 0);
			if (buffer) {
				/* ファイルへ保存 */
				sprintf(path, "raw1016bit_cam%d_%d.raw", i, j);
				save_raw_image(path, buffer, LFI4_FMT_MAX_WIDTH*LFI4_FMT_MAX_HEIGHT*2);
			}
		}
	}
	
	/* センサー(8〜15)のストリーム終了  メインドライバ /dev/video0 に対して行う */
	Dev_SetEnableSensorStream(dev, 0);
	/* ストリーム終了 */
	Dev_Stop(dev_dummy);
#endif	
	
	Dev_Dealloc(dev_dummy);
	
dealloc:	
	Dev_Dealloc(dev);
	;
	
terminate:
	// Terminate VC Library
	Dev_Terminate();
	return 0;
}