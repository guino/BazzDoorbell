#!/bin/sh
echo "Content-type: video/H264"
#echo "Content-type: octet/stream"
#echo "Transfer-Encoding: chunked"
echo ""
# Get ppsapp PID
PPSID=$(ps | grep -v grep | grep ppsapp | awk '{print $1}')
# Get VIDEO address
ADDR=$(/mnt/mmc01/busybox dd if=/proc/$PPSID/mem bs=1 skip=$((0x41a44c)) count=4| /mnt/mmc01/busybox od -t x4 | /mnt/mmc01/busybox head -1 | awk '{print $2}')
/mnt/mmc01/streamer-arm /proc/$PPSID/mem $((0x$ADDR)) $((128*1536*6)) 2> /tmp/log
