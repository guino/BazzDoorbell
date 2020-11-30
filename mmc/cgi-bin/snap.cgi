#!/bin/sh
echo -e "Content-type: image/jpeg\r"
echo -e "\r"
# Delay so my apps don't reload too fast
/mnt/mmc01/busybox usleep 300000
# Get ppsapp PID
PPSID=$(ps | grep -v grep | grep ppsapp | awk '{print $1}')
# Get JPEG address
JPEGADDR=$(/mnt/mmc01/busybox dd if=/proc/$PPSID/mem bs=1 skip=$((0x42ac2c)) count=4| /mnt/mmc01/busybox od -t x4 | /mnt/mmc01/busybox head -1 | awk '{print $2}')
/mnt/mmc01/busybox dd if=/proc/$PPSID/mem bs=4096 skip=$((0x$JPEGADDR/4096)) count=14 | /mnt/mmc01/busybox dd bs=8 skip=1
