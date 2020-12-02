#!/bin/bash

URL="http://user:password@10.10.10.7:8080"
SERIAL="123456789"
LOG=/tmp/doorcopy.log #/dev/stdout

# Make sure 3tb is mounted/ready
if [ ! -e /mnt/3tb/nvr-Door ]; then
	echo Trying to mount 3tb ...
	# Umount+Mount
	umount /mnt/3tb
	mount -t nfs 10.10.10.2:/mnt/3TB /mnt/3tb
	if [ ! -e /mnt/3tb/nvr-Door ]; then
		echo Failed to mount 3tb, leaving ...
		exit 1
	fi
fi

# Clear log
echo -en "" > $LOG

# Get today and lists of files (on camera and local)
TODAY=$1
if [ "$1" == "" ]; then
 TODAY=$(date +%Y%m%d)
fi
YY=${TODAY:0:4}
MM=${TODAY:4:2}
DD=${TODAY:6:2}

# Check if today's path exists locally
cd /mnt/3tb/nvr-Door
# If it doesn't: copy/check yesterday's files one last time
if [ ! -e $TODAY ]; then
	/jffs/doorcopy.sh `date --date="yesterday" +%Y%m%d`
fi

# Simplify paths
mkdir -p $TODAY
cd $TODAY

# Get list of files for today
LST=$(curl -m 2 -s "$URL/SDT/$SERIAL/record/$YY/$MM/$DD/$TODAY.index" | strings | grep '\.data' | tac)

# For each file
for f in $LST; do
	N=$(basename $f)
	# Log file being processed
	echo -n $N >> $LOG
	# If we don't have the file yet
	if [ ! -e $N ]; then
		# Copy it and log it
		echo -n " copying... " >> $LOG
		curl -m 60 -s "$URL${f:10}" > $N
		touch -m -t $TODAY${N:0:2}${N:2:2}.${N:4:2} $N
		echo "done." >> $LOG
	else
		RSIZE=$(curl -m 2 -sI "$URL${f:10}" | grep Content-Length | awk '{print $2}' | sed 's/\r$//')
		LSIZE=$(stat -c %s $N)
		# If remote size is bigger, update local file
		if [ "$RSIZE" -gt "$LSIZE" ]; then
			echo -n " updating... " >> $LOG
			curl -m 60 -s "$URL${f:10}" > $N
			touch -m -t $TODAY${N:0:2}${N:2:2}.${N:4:2} $N
			echo "done." >> $LOG
		else
			# Log we already had it (we found the first one that we already have, so we're done)
			echo " already copied." >> $LOG
		fi
		exit 0
	fi
done
