PACKAGE=jpeg
VERSION=0.0.1

CC = gcc
GCC = ../../armv5-eabi--uclibc--stable-2020.08-1/bin/arm-buildroot-linux-uclibcgnueabi-gcc

all: clean $(PACKAGE)-i386 $(PACKAGE)-arm

$(PACKAGE)-i386:
	$(CC) -m32 $(PACKAGE).c -o $(PACKAGE)-i386

$(PACKAGE)-arm:
	$(GCC) $(PACKAGE).c -o $(PACKAGE)-arm
	cp -f $(PACKAGE)-arm /srv/tftp/
clean:
	rm -f $(PACKAGE)-i386 $(PACKAGE)-arm
