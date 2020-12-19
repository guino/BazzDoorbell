#!/bin/sh
echo -e "Content-type: text/plain\r"
echo -e "\r"
# Set here the request address for ppsapp
REQADDR=$((0x42d6e4))
# Get ppsapp PID
PPSID=$(ps | grep -v grep | grep ppsapp | awk '{print $1}')
# Get file (from command line or query)
if [ "$1" != "" ]; then
 FNAME=$1
else
 FNAME=$QUERY_STRING
fi
# Check file
if [ ! -e "$FNAME" ]; then
 echo $FNAME does not exist!
 exit 0
fi
# Check if it's already playing something
PLAYING=$(/mnt/mmc01/busybox dd if=/proc/$PPSID/mem bs=1 skip=$REQADDR count=4| /mnt/mmc01/busybox od -t x4 | /mnt/mmc01/busybox head -1 | awk '{print $2}')
if [ "$PLAYING" == "00000000" ]; then
 # Write filename
 echo -en "$FNAME\x00" | /mnt/mmc01/busybox dd of=/proc/$PPSID/mem bs=1 seek=$((REQADDR+8))
 # Request playback
 echo -en "\x01" | /mnt/mmc01/busybox dd of=/proc/$PPSID/mem bs=1 seek=$REQADDR
 echo Playing $FNAME
else
 echo "Can't play right now as something is already playing!"
fi
