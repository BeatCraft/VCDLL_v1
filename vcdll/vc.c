
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "../VCDLL.h"



int main(int argc, char** argv)
{
	bool bRet;
	DevObject* dev = NULL;
	
	// Initialize VC Library
	Dev_Initialize();
	
	long devices = Dev_EnumDevice();
	printf("Dev_EnumDevice: devices = %lu\n", devices);
	
	dev = Dev_NewObject(0);
	if (!dev) {
		goto terminate;
	}
	
	bRet = Dev_Start(dev);
	
	sleep(5);
	
	Dev_Stop(dev);
	
dealloc:	
	Dev_Dealloc(dev);
	
terminate:
	// Terminate VC Library
	Dev_Terminate();
	return 0;
}