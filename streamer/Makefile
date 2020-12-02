PACKAGE=streamer
VERSION=0.0.1

CC = gcc
GCC = ../../bazz/armv7-eabihf--uclibc--stable-2020.08-1/bin/arm-buildroot-linux-uclibcgnueabihf-gcc

all: clean $(PACKAGE)-i386 $(PACKAGE)-arm

$(PACKAGE)-i386:
	$(CC) -m32 $(PACKAGE).c -o $(PACKAGE)-i386

$(PACKAGE)-arm:
	$(GCC) $(PACKAGE).c -o $(PACKAGE)-arm

clean:
	rm -f $(PACKAGE)-i386 $(PACKAGE)-arm
