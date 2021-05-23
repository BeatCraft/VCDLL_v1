all: vc

vc: vc.c
	gcc -g -Wall -pthread -L.. -lVCDLL -o $@ $<

test:
	sh test-vc

clean:
	rm -f vc