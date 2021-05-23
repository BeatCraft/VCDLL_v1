# VCDLLコンパイル用
CFLAGS =  -ludev -D_XOPEN_SOURCE=500  `pkg-config --libs --cflags libusb-1.0`

libVCDLL.so: VCDLL.c VCDLL.h
	gcc -fPIC -shared VCDLL.c `pkg-config --libs --cflags libusb-1.0` -o libVCDLL.so $(CFLAGS) 

rm:
	rm *.raw *.png
	
clean:
	rm -f *.o *.so
