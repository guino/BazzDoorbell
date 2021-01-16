#!/bin/sh
echo -e "Content-type: text/plain\r"
echo -e "\r"
SERIAL=123456789
DAYS=90
YEAR=$(date +%Y)
/mnt/mmc01/busybox find /mnt/mmc01/SDT/$SERIAL/record/$YEAR/ -type d -mtime +$DAYS -exec echo rm -rf {} \; -exec rm -rf {} \;
YEAR=$((YEAR-1))
if [ -e /mnt/mmc01/SDT/$SERIAL/record/$YEAR/ ]; then
 /mnt/mmc01/busybox find /mnt/mmc01/SDT/$SERIAL/record/$YEAR/ -type d -mtime +$DAYS -exec echo rm -rf {} \; -exec rm -rf {} \;
fi
