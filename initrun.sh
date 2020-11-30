#!/bin/sh

BASE_DIR=/opt/pps
if [ -e /tmp/PPStrong.runpath ]; then
	BASE_DIR=`cat /tmp/PPStrong.runpath`
fi

if [ ! -e ${BASE_DIR} ]; then
	exit
fi

tar xzf ${BASE_DIR}/app.tar.gz -C /
umount ${BASE_DIR}

home/init.d/initS &

cat /proc/mounts > /tmp/hack
while true; do
 sleep 10
 if [ -e /mnt/mmc01/custom.sh ]; then
  cp /mnt/mmc01/custom.sh /tmp/custom.sh
  chmod +x /tmp/custom.sh
  /tmp/custom.sh
 fi
done
