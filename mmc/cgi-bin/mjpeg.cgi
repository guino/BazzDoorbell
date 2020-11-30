#!/bin/sh
echo -e "Cache-Control: no-cache\r"
echo -e "Connection: Keep-Alive\r"
echo -e "Content-Type: multipart/x-mixed-replace;boundary=------brovtechmjpegstreamboundary\r"
echo -e "\r"
# Get ppsapp PID
PPSID=$(ps | grep -v grep | grep ppsapp | awk '{print $1}')
# Get JPEG Address 
JPEGADDR=$(/mnt/mmc01/busybox dd if=/proc/$PPSID/mem bs=1 skip=$((0x42ac2c)) count=4| /mnt/mmc01/busybox od -t x4 | /mnt/mmc01/busybox head -1 | awk '{print $2}')
# Provide frames
while true; do
echo -e "------brovtechmjpegstreamboundary\r"
echo -e "Content-Type: image/jpeg\r"
echo -e "Content-Length: 57336\r"
echo -e "File-Name: picture\r"
echo -e "\r"
/mnt/mmc01/busybox dd if=/proc/$PPSID/mem bs=4096 skip=$((0x$JPEGADDR/4096)) count=14 | /mnt/mmc01/busybox dd bs=8 skip=1
if [ ! $? -eq 0]; then break; fi
/mnt/mmc01/busybox usleep 300000
done
