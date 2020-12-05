# Bazz Wi-Fi Doorbell/Camera Penetration test and customization
##### Introduction
Following the work I did with another wifi camera (https://github.com/guino/WR301CH1KW) I decided to tackle another 'closed' camera I already had installed this year.
This is a 'TUYA' camera which is known to be protective of how their devices can be accessed and used (no 'PC' apps for instance), but I still bought the device for the price, features and the fact it works with the existing wiring/chime of the house.
The main goals as usual are to try to 'backup' the video contents of the device to my NAS (hopefully without using SD cards) and in this case access the camera feed from outer applications/PC.
##### Hardware
The hardware model is listed as WFDBELL1 (website, etc) but I am pretty sure is just an OEM branding from the actual manufacturer (https://www.mearitek.com/products-bell-5s/) which I think also makes the doorbells for 'geeni' and 'merkury' (and probably others).

[![Doorbell](https://raw.githubusercontent.com/guino/BazzDoorbell/master/img/bazz.jpg)](https://www.amazon.com/BAZZ-WFDBELL1-Doorbell-Compatible-Required/dp/B07ZQLBDHC)

###### Penetration test
The first thing was to check if there was anything published on this type of camera online: turned out with some people insterested but not a lot of progress (https://github.com/AMoo-Miki/homebridge-tuya-lan/issues/4), but I already know there are a number of other cameras in the market with the same 'hisilicon' boards in them.
So we start with a quick check for open ports:
```
nmap 10.10.10.7
Starting Nmap 7.80 ( https://nmap.org ) at 2020-11-30 16:45 EST
Nmap scan report for 060010869 (10.10.10.7)
Host is up (0.0099s latency).
Not shown: 996 closed ports
PORT     STATE SERVICE
80/tcp   open  http
6668/tcp open  irc
```
No ssh/telnet port open so my only option without tearing the device appart (to find a serial port) is to check for anything I can use in the web server.
A quick check reveals the web server requests a user/password which some people in the thread above already had discovered (saving me the time), so lets fuzz this thing:
```
$ wfuzz -w common.txt --hc=404 --basic admin:056565099 http://10.10.10.7/FUZZ
********************************************************
* Wfuzz 3.1.0 - The Web Fuzzer                         *
********************************************************

Target: http://10.10.10.7/FUZZ
Total requests: 951

=====================================================================
ID           Response   Lines    Word       Chars       Payload                                                                                                                                                                                       
=====================================================================

Total time: 0
Processed Requests: 0
Filtered Requests: 0
Requests/sec.: 0

/.local/lib/python3.8/site-packages/wfuzz/wfuzz.py:77: UserWarning:Fatal exception: Pycurl error 28: Operation timed out after 90000 milliseconds with 0 bytes received
```
That was a waste... seems like the web server just doesn't respond when you provide a url it doesn't understand even with the valid user/password provided.
I tried a few more options/filters without much success (only found /search ), however one of the threads above had a few working URLs that they found by dumping the firmware (/devices/deviceinfo, /sys/reboot, etc), but the one that caught my attention was /proc/<something>.
I went ahead and was able to get /proc/cmdline, /proc/mounts and other locations but most importantly it could get me anywhere in the file system with a trick such as: /proc/self/root/etc/passw.
That said: it seems it will only provide ascii printable characters (and closes the connection if it finds non-printable characters) and has a limit of 4096kb size (which is plenty for most text/script files).
I call that progress but nothing that allows me to do something to it.

Without any remote options left I decided to open the device and try the UART route... so I opened the device and found out the SoC chip is in fact hiSilicon (as suspected):

![SoC](https://raw.githubusercontent.com/guino/BazzDoorbell/master/img/soc.jpg)

Also found/identified the flash chip (PDF is in the project):

![Flash](https://raw.githubusercontent.com/guino/BazzDoorbell/master/img/flash.jpg)

And for the next part: found and identified the UART pins, measured the level (3.3V) and probed potential GND/RX/TX pins:

![UART](https://raw.githubusercontent.com/guino/BazzDoorbell/master/img/uart.jpg)

I was hoping that like many other cameras we'd have a standard uboot hopefully configured in a way I could interrupt/change settings before loading kernel but I was surprised:
```
hisi-sdhci: 0
PPS:Jul 22 2019 00:22:28 meari_c5    0 

please input password::
```
That doesn't look like u-boot, but anyway I tried different passwords without success and after searching found someone also looking for a way around it (https://github.com/DanTLehman/orion_sc008ha).
I did play a little bit with it and that ppsMmcTool.txt file (see https://github.com/DanTLehman/orion_sc008ha/issues/1) but figured it was time to get more serious... time to pull out the programmer (available for around $15 just about everywhere):

![Programmer](https://images-na.ssl-images-amazon.com/images/I/41xB3WRorzL._AC_SY355_.jpg)

I had read (probably from one of the mentioned links) that the only way they were getting to read/write these chips was by removing it from the board, but gave it a try anyway without success.
Since removing/soldering the chip is time consumming and kind of a pain (not to say you can damage the board) I figured I'd try disconnecting only one (or some) pins to see if I could get it to work.
After a few tries, I got lucky: it was only required to lift pin 6 (CLK) in order to be able to both read/write the chip using the 'clip':

![pin6](https://raw.githubusercontent.com/guino/BazzDoorbell/master/img/pin6.jpg)

I call this good progress, now that I have a dump of the flash memory we can run it thru binwalk and see what we get:
```
# binwalk -e -M ../bazz.bin 
DECIMAL       HEXADECIMAL     DESCRIPTION
--------------------------------------------------------------------------------
18304         0x4780          gzip compressed data, has original file name: "u-boot.bin", from Unix, last modified: 2020-03-19 02:07:20
393216        0x60000         uImage header, header size: 64 bytes, header CRC: 0xCAFBC6AB, created: 2020-03-21 04:08:30, image size: 3205330 bytes, Data Address: 0x40008000, Entry Point: 0x40008000, data CRC: 0x4D094A1B, OS: Linux, CPU: ARM, image type: OS Kernel Image, compression type: none, image name: "Linux-4.9.37"
393280        0x60040         Linux kernel ARM boot executable zImage (little-endian)
395744        0x609E0         device tree image (dtb)
409432        0x63F58         device tree image (dtb)
415388        0x6569C         device tree image (dtb)
419604        0x66714         gzip compressed data, maximum compression, from Unix, last modified: 1970-01-01 00:00:00 (null date)
3584472       0x36B1D8        device tree image (dtb)
3604480       0x370000        CramFS filesystem, little endian, size: 3805184, version 2, sorted_dirs, CRC 0x8C6D7F38, edition 1, 929 blocks, 3 files
8060940       0x7B000C        JFFS2 filesystem, little endian
8066032       0x7B13F0        JFFS2 filesystem, little endian
8066520       0x7B15D8        JFFS2 filesystem, little endian
8067980       0x7B1B8C        JFFS2 filesystem, little endian
8068468       0x7B1D74        JFFS2 filesystem, little endian
8069928       0x7B2328        JFFS2 filesystem, little endian
8070416       0x7B2510        JFFS2 filesystem, little endian
8196048       0x7D0FD0        Zlib compressed data, compressed
8196232       0x7D1088        Zlib compressed data, compressed
8196868       0x7D1304        Zlib compressed data, compressed
8197048       0x7D13B8        Zlib compressed data, compressed
8197752       0x7D1678        Zlib compressed data, compressed
8198256       0x7D1870        Zlib compressed data, compressed
8198916       0x7D1B04        Zlib compressed data, compressed
8199416       0x7D1CF8        Zlib compressed data, compressed
8199920       0x7D1EF0        Zlib compressed data, compressed
8200424       0x7D20E8        Zlib compressed data, compressed
8200860       0x7D229C        Zlib compressed data, compressed
8201564       0x7D255C        Zlib compressed data, compressed
8202068       0x7D2754        Zlib compressed data, compressed
8202572       0x7D294C        Zlib compressed data, compressed
8203080       0x7D2B48        Zlib compressed data, compressed
8203584       0x7D2D40        Zlib compressed data, compressed
8204092       0x7D2F3C        Zlib compressed data, compressed
8204600       0x7D3138        Zlib compressed data, compressed
8205108       0x7D3334        Zlib compressed data, compressed
8205616       0x7D3530        Zlib compressed data, compressed
8206124       0x7D372C        Zlib compressed data, compressed
8206632       0x7D3928        Zlib compressed data, compressed
8207140       0x7D3B24        Zlib compressed data, compressed
8207648       0x7D3D20        Zlib compressed data, compressed
8208156       0x7D3F1C        Zlib compressed data, compressed
8208664       0x7D4118        Zlib compressed data, compressed
8209172       0x7D4314        Zlib compressed data, compressed
8209680       0x7D4510        Zlib compressed data, compressed
8210184       0x7D4708        Zlib compressed data, compressed
8210688       0x7D4900        Zlib compressed data, compressed
8211192       0x7D4AF8        Zlib compressed data, compressed
8211628       0x7D4CAC        Zlib compressed data, compressed
8212332       0x7D4F6C        Zlib compressed data, compressed
8212836       0x7D5164        Zlib compressed data, compressed
8213340       0x7D535C        Zlib compressed data, compressed
8213844       0x7D5554        Zlib compressed data, compressed
8214348       0x7D574C        Zlib compressed data, compressed
8214852       0x7D5944        Zlib compressed data, compressed
8215356       0x7D5B3C        Zlib compressed data, compressed
8215860       0x7D5D34        Zlib compressed data, compressed
8216364       0x7D5F2C        Zlib compressed data, compressed
8216868       0x7D6124        Zlib compressed data, compressed
8217372       0x7D631C        Zlib compressed data, compressed
8217876       0x7D6514        Zlib compressed data, compressed
8218312       0x7D66C8        Zlib compressed data, compressed
8219012       0x7D6984        Zlib compressed data, compressed
8219516       0x7D6B7C        Zlib compressed data, compressed
8219952       0x7D6D30        Zlib compressed data, compressed
8220656       0x7D6FF0        Zlib compressed data, compressed
8221240       0x7D7238        Zlib compressed data, compressed
8221796       0x7D7464        Zlib compressed data, compressed
8222232       0x7D7618        Zlib compressed data, compressed
8222720       0x7D7800        JFFS2 filesystem, little endian
8229020       0x7D909C        Zlib compressed data, compressed
8229504       0x7D9280        Zlib compressed data, compressed
8229992       0x7D9468        Zlib compressed data, compressed
8230480       0x7D9650        Zlib compressed data, compressed
8230664       0x7D9708        Zlib compressed data, compressed
8231148       0x7D98EC        Zlib compressed data, compressed
8231636       0x7D9AD4        Zlib compressed data, compressed
8232124       0x7D9CBC        Zlib compressed data, compressed
8232308       0x7D9D74        Zlib compressed data, compressed
8232488       0x7D9E28        Zlib compressed data, compressed
8232672       0x7D9EE0        Zlib compressed data, compressed
8232856       0x7D9F98        Zlib compressed data, compressed
8233040       0x7DA050        Zlib compressed data, compressed
8233524       0x7DA234        Zlib compressed data, compressed
8234012       0x7DA41C        Zlib compressed data, compressed
8234500       0x7DA604        Zlib compressed data, compressed
8234988       0x7DA7EC        Zlib compressed data, compressed
8235472       0x7DA9D0        Zlib compressed data, compressed
8235960       0x7DABB8        Zlib compressed data, compressed
8236448       0x7DADA0        Zlib compressed data, compressed
8236936       0x7DAF88        Zlib compressed data, compressed
8237420       0x7DB16C        Zlib compressed data, compressed
8237908       0x7DB354        Zlib compressed data, compressed
8238092       0x7DB40C        Zlib compressed data, compressed
8238580       0x7DB5F4        Zlib compressed data, compressed
8238760       0x7DB6A8        Zlib compressed data, compressed
8238944       0x7DB760        Zlib compressed data, compressed
8239128       0x7DB818        Zlib compressed data, compressed
8239312       0x7DB8D0        Zlib compressed data, compressed
8239492       0x7DB984        Zlib compressed data, compressed
8239980       0x7DBB6C        Zlib compressed data, compressed
8240164       0x7DBC24        Zlib compressed data, compressed
8240652       0x7DBE0C        Zlib compressed data, compressed
8240832       0x7DBEC0        Zlib compressed data, compressed
8241320       0x7DC0A8        Zlib compressed data, compressed
8241808       0x7DC290        Zlib compressed data, compressed
8242296       0x7DC478        Zlib compressed data, compressed
8242780       0x7DC65C        Zlib compressed data, compressed
8243268       0x7DC844        Zlib compressed data, compressed
8243756       0x7DCA2C        Zlib compressed data, compressed
8244244       0x7DCC14        Zlib compressed data, compressed
8244728       0x7DCDF8        Zlib compressed data, compressed
8245216       0x7DCFE0        Zlib compressed data, compressed
8245704       0x7DD1C8        Zlib compressed data, compressed
8245888       0x7DD280        Zlib compressed data, compressed
8246372       0x7DD464        Zlib compressed data, compressed
8246860       0x7DD64C        Zlib compressed data, compressed
8247348       0x7DD834        Zlib compressed data, compressed
8247836       0x7DDA1C        JFFS2 filesystem, little endian
8248316       0x7DDBFC        JFFS2 filesystem, little endian
8249140       0x7DDF34        JFFS2 filesystem, little endian
8250600       0x7DE4E8        JFFS2 filesystem, little endian
8251088       0x7DE6D0        JFFS2 filesystem, little endian
8252548       0x7DEC84        JFFS2 filesystem, little endian
8253036       0x7DEE6C        JFFS2 filesystem, little endian
8254496       0x7DF420        JFFS2 filesystem, little endian
8254984       0x7DF608        JFFS2 filesystem, little endian
8256444       0x7DFBBC        JFFS2 filesystem, little endian
8256932       0x7DFDA4        JFFS2 filesystem, little endian
(recursive extraction ommitted)
```
After a quick look around I did not find any scripts or references to running/reading anything from the mmc card, that is, in order to get it to execute a command/script and gain access.
I also did not see any mention of telnetd for me to run but I was happy to see an unencrypted cramfs partition, which included a startup script and the main doorbell/camera application itself.

So the next step is to try to gain remote access, the initrun.sh script in the cramfs partition seems like the perfect place, so I modified it (see initrun.sh in github) and prepared my own firmware file, after determining the parameters for the cramfs (by reviewing the original one):
```
mkfs.cramfs -b 4096 -e 1 -N little -n ppsapp mycramfs-root/ my.cramfs
cp bazz.bin mybazz.bin
dd conv=notrunc if=my.cramfs of=mybazz.bin bs=1 seek=3604480
```
I know my script was going to cause a little overhead by running every 10s but I wanted to know I could re-run something without rebooting by simply removing/editing/inserting the script on mmc and also wanted to be sure it would run on reboot after the mmc was mounted (however long that may take).
So I flashed my modified bin file, and verified the device booted normally (by just holding pin 6 down down touching the board) and verified my script executed before soldering pin 6 back in place.

With my script being called on the mmc, I wrote a small script to start passwordless telnetd (final version of custom.sh is in github), and finally had a way into the OS:
```
# telnet 10.10.10.7
Trying 10.10.10.7...
Connected to 10.10.10.7.
Escape character is '^]'.

BusyBox v1.26.2 (2019-11-03 17:33:40 PST) built-in shell (ash)

/ #
```

##### Customization
The objective with the process above was not to expose vulnerabilities or make it possible to 'hack' cameras, in fact the process above required me to physically access the flash chip of the camera -- the whole point is to be able to customize the device to my specific needs, specifically to store/copy the video files on my NFS share.
With root access, the first thing to do is to secure any remote access to the camera so that it can't be hacked with known username/password combinations, so I went ahead and edited /etc/passwd to have a username and password (hash) only known to me.
Unlike other cameras I have seen in the market this one actually uses a read-write memory mount which allows us to just modify and copy files to the built-in memory but those changes are lost on reboot (so my script has to re-apply them on every boot), no big deal.
Once I verified my new telnet user/pass worked I went ahead and changed the passwordless telnet acces for a standard (password protected) telnet on my script.

Now to see if I can mount my NFS share from the camera.... checking /proc/filesystems I knew nfs kernel modules were not loaded (or even available)... bummer.

Tried copying nfs modules from another camera on same kernel version but they would not load on this kernel.. bummer again. 
The only way I could get NFS to work here is to make my own kernel with NFS support (and modules) but then I'd probably have to recompile all custom kernel drivers for the camera -- there's no way in hell I'm spending the time to do all that, so at this point I'm dead set with always using the mmc card.
The silver lining is that I already needed the mmc card to bootstrap my script anyway, so that saves me time from researching a way to run my script over network and such.

So how can I backup the SD card videos to my NAS? The simplest solution is usually the best: run httpd (from same busybox in mmc card) with a user/password so I can browse/download the files from a script on a raspberry pi I already use for automation around the house.
That's where the httpd.conf, index.html and cgi-bin/main.cgi come in (credit to https://github.com/HeGanjie/busybox-httpd-directory-listing). I have included the the cron script that downloads the files from the doorbell to my NAS/NFS share, you just have to adjust the parameters/paths to use it with your device (NFS/share/script paths, user, password, IP and serial).

Now to the hard part... get remote viewing of the doorbell/camera.

I had already started looking at ppsapp with ghidra looking for any potential exploits to enable telnet and even found a URL that runs a command to toggle telnet access (but as previously said telnetd is not in the image), so I opened it up looking at the RTSP related code used for chromecast/ecoshow and found the code that initializes the camera buffers (and named them):

![ghidra](https://raw.githubusercontent.com/guino/BazzDoorbell/master/img/ghidra.jpg)

From the ppsapp code it looks like the feed to outside devices may be encrypted and I didn't want to go down that route since I had a ready-source of unencrypted data in the memory of the applicatuon.

I downloaded the armv7l toolchain for uclibc and wrote a 'streamer' application which would read the video from the circular buffer in ppsapp, monitor it and stream out any new data which works for the most part.
I have added the streamer code attempts I made to github, and the most current version successfully makes files (just redirect its output to a file) which can be properly played in VLC, HOWEVER: it seems that without the RTP/RTSP encapsulation the playback cuts off every few seconds (despite a file with the same exact data playing correctly). I assume this has to do with the timing of the NAL units or it may require that each frame be transmitted in the same 'chunk' of data (which I didn't do). Nevertheless see the 'final update' section at the bottom for full solution.
~~The problem with the streamer application is that I didn't want it to use a lot of memory so I made a smaller buffer to parse the data from the ppsapp buffer and monitor/stream it that way but because of the nature of the updates it seems I am always reading data while it's being updated in memory (which corrupts the stream).
With that the only 'good' solution would be to read the ppsapp circular buffer in one-shot (as quick as possible) to minimize the chances of it happening while the buffer is being updated (minimizing the chances of it being corrupt).
This means I'd have to copy the full buffer (1.2Mb for 1080P) which I am not sure is worth the cost. I may end up doing it and using the SD buffer (400Kb) which would be ok.
If/when I do complete the streamer application I intend to post it on here.~~

For now what I have done is come up with an alternate solution:
I tracked down the address in the ppsapp which stores the buffer address for the JPEG encoding of the camera feed in real time (used for screenshots/alerts and such), so I wrote a snap.cgi script (which is in github) which does the following:
* Finds the process ID for the running ppsapp
* Reads the buffer address from the ppsapp memory
* Reads the JPG buffer itself and sends it out as a response

The result is I am able to get a 'snapshot' URL of the camera feed under /cgi-bin/snap.cgi:

![snapshot](https://raw.githubusercontent.com/guino/BazzDoorbell/master/img/snap-cgi.png)

It is a low resolution snapshot but it definitely works for alerts/previews/etc (plenty for my ios app, domoticz etc which crunch down a bunch of camera images together).

Last (for now) I also put together a 'mjpeg' stream (mjpeg.cgi in github), which basically does the same as the snap.cgi but in a loop (with the proper headers) so that I can actually see a 'moving feed' of the camera from app/browser/etc.

I will likely still work on the 'streamer' application using a full buffer -- it's more of a challenge right now than a 'need'.

I hope the above helps someone in a similar situation and if by any chance you get to the point where you have access to the firmware, feel free to use the contents of this page (including the ppsapp wchich will work with the scripts provided).

##### Final update

I didn't want to spend more time (than I already have) coding a solution to view a high-resolution stream from the doorbell camera so I went into the ppsapp code in ghidra again to see if I could find the code that streams the video to their app, I was thinking of just disabling the encryption so I would be able to view it directly, but in that review process I noticed that the "echo show" streaming support did not seem to be encrypted and in fact it seemed like that feature was just disabled (by hard-coded setting), so without hesitation I went ahead and modified the code/file to always start the "echo show" support. I expected I would have to make a few more changes to get it to work but no: a single byte change in the file and it baiscally started up a RTSP server on port 8554 (rtsp://<ip>:8554) with VLC and any other streaming app working perfectly **in HD with audio**.
I have made the 'rtsp' version of ppsapp available in the project so you can use it in your device. I am not concerned in 'protecting' the stream with a user/password as it is inside my local/internal network (firewalled) and it's a camera the faces outside the house, so use it at your own risk. If I am ever concerned I can just add a cgi script in httpd that enables/disables access to the RTSP stream (with iptables or even just stopping the rtsp version of ppsapp and starting the non-rtsp version).

With the video recordings being saved (downloaded) onto my NFS share and having the ability to to view the live feed in HD (RTSP) and in SD (MJPEG) from any standard application I call this a resounding success and don't have any more future plans for this project.

##### SD Option

See this link for a No-Programmer, No-UART, No-Open SD card-only solution:
https://github.com/guino/BazzDoorbell/issues/2

