all: vc

vc: vc.c
	gcc -g -Wall -pthread -L.. -o $@ $< -lVCDLL

test:
	sh test-vc

clean:
	rm -f vc