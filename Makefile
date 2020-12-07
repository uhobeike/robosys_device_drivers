obj-m := 3_led.o

3_led.ko: 3_led.c
	make -C /usr/src/linux-headers-`uname -r` M=`pwd` V=1 modules
clean:
	make -C /usr/src/linux-headers-`uname -r` M=`pwd` V=1 clean
