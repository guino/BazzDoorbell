#!/bin/sh
if [ ! -e /tmp/customrun ]; then
 echo custom > /tmp/customrun
 cp /mnt/mmc01/passwd /etc/passwd
 /mnt/mmc01/busybox telnetd
 /mnt/mmc01/busybox httpd -c /mnt/mmc01/httpd.conf -h /mnt/mmc01 -p 8080
fi
