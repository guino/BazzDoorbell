#!/bin/sh
if [ ! -e /tmp/customrun ]; then
 echo custom > /tmp/customrun
 cp /mnt/mmc01/passwd /etc/passwd
 /mnt/mmc01/busybox telnetd
 /mnt/mmc01/busybox httpd -c /mnt/mmc01/httpd.conf -h /mnt/mmc01 -p 8080
 if [ -e /mnt/mmc01/ppsapp ]; then
  PPSID=$(ps | grep -v grep | grep ppsapp | awk '{print $1}')
  kill $PPSID
  #/mnt/mmc01/set record_enable 0
  #/mnt/mmc01/set enable_event_record 1
  #/mnt/mmc01/set onvif_enable 1
  /mnt/mmc01/ppsapp &
 fi
 #/mnt/mmc01/offline.sh &
fi
if [ ! -e /tmp/cleanup`date +%Y%m%d` ]; then
 rm -rf /tmp/cleanup*
 touch /tmp/cleanup`date +%Y%m%d`
 /mnt/mmc01/cgi-bin/cleanup.cgi > /tmp/cleanup.log
fi
