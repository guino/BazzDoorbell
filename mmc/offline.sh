#!/bin/sh

# If no hosts file, leave
if [ ! -e /mnt/mmc01/hosts ]; then
 exit 0
fi

# Wait for date to be corrected
while [ `date +%s` -lt 1645543474 ]; do
 date >> /tmp/offline.log
 sleep 1
done
# Wait for connection to be made
while [ `netstat -n | grep ':8883' | grep -c ESTABLISHED` -lt 1 ]; do
 echo "no conn" >> /tmp/offline.log
 sleep 1
done
# Block internet access to tuya servers
cp /mnt/mmc01/hosts /etc
echo "blocked hosts" >> /tmp/offline.log
# Wait for connection to be dropped
while [ `netstat -n | grep -v 127.0.0.1 | grep -v ':23' | grep -c ESTABLISHED` -gt 0 ]; do
 # For each non-telnet established IP
 for ip in `netstat -n 2>&1 | grep -v 127.0.0.1 | grep -v :23 | grep ESTABLISHED | awk '{print $5}' | awk -F: '{print $1}'`; do
  echo "checking $ip" >> /tmp/offline.log
  if [ "`route -n | grep -c $ip`" == "0" ]; then
   route add -net $ip netmask 255.255.255.255 gw 127.0.0.1
   echo "blocked $ip" >> /tmp/offline.log
  fi
 done
 # Bring down wifi
 ifconfig wlan0 down
 # Brief wait
 sleep 1
done
# Restore wifi connection
ifconfig wlan0 up

