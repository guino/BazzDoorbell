#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int fd;							// File descriptor
char *buff;						// jpeg buffer

char *snaphdr = "Content-type: image/jpeg\r\n\r\n";

char *mjpeghdr = "Cache-Control: no-cache\r\nConnection: Keep-Alive\r\nContent-Type: multipart/x-mixed-replace;boundary=------guinomjpegboundary\r\n\r\n";
char *mjpegsep = "------guinomjpegboundary\r\nContent-Type: image/jpeg\r\nFile-Name: picture\r\nContent-Length: ";
char mjpeglen[12];

int main( int ac, char *av[] )
{
	// If not enough arguments, show usage and leave
	if(ac < 4) {
		fprintf(stderr, "usage: %s /proc/NNNN/mem address length [mjpeg]\n", av[0]);
		exit(1);
	}

	// Open memory of process
	if((fd = open(av[1], O_RDONLY)) < 0) {
		fprintf(stderr, "Can't access %s\n", av[1]);
		perror(":");
		exit(1);
	}

	// Flag for mjpeg
	int mjpeg = 0;
	if(ac == 5 && strcmp(av[4], "mjpeg")==0)
		mjpeg = 1;

	// Set address and length of buffer
	ulong jpegptraddr = strtoul(av[2], 0, 16);
	ulong jpegaddr = 0;
	ulong len = strtoul(av[3], 0, 0);

	// Read print jpeg address
	lseek(fd, jpegptraddr, SEEK_SET);
	read(fd, &jpegaddr, 4);
	fprintf(stderr, "jpegptraddr=%x jpegaddr=%x\n", jpegptraddr, jpegaddr);

	// Create buffers
	buff = malloc(len*2);
	if(!buff) {
		fprintf(stderr, "Could not allocate buffers.\n");
		exit(1);
	}

	// Initial read pointer position
	char *ptr = buff;

	// If mjpeg, send out initial header
	if(mjpeg)
		write(STDOUT_FILENO, mjpeghdr, strlen(mjpeghdr));

	// Stdout write length
	int outlen = 0;

	do {
		// While buffers are different (i.e. update in progress)
		int match = 0;
		while(match==0) {
			// Copy data to current read pointer
			lseek(fd, jpegaddr, SEEK_SET);
			if(read(fd, ptr, len)<len) {
				fprintf(stderr, "Could not read whole buffer\n");
				break;
			}

			// Set next read pointer
			if(ptr==buff)
				ptr=buff+len;
			else
				ptr=buff;

			// If buffers are the same
			if(memcmp(buff, buff+len, len)==0)
				match=1;

			// Not too fast (10ms beteen checks)
			usleep(10000);
		}

		// If out buffers match, send out data
		if(match) {
			int jlen = len;
			// Based on type
			if(mjpeg) {
				// Find end of JPEG
				for(jlen=0;jlen<len;jlen++) {
					if( (buff[jlen]==0xFF) && (buff[jlen+1]==0xD9) )
						break;
				}
				if(jlen+2<len)
					jlen+=2;
				// Write separator
				write(STDOUT_FILENO, mjpegsep, strlen(mjpegsep));
				// Prepare and write size
				sprintf(mjpeglen, "%d\r\n\r\n", jlen);
				write(STDOUT_FILENO, mjpeglen, strlen(mjpeglen));
			} else {
				write(STDOUT_FILENO, snaphdr, strlen(snaphdr));
			}
			// JPEG buffer
			outlen = write(STDOUT_FILENO, buff, jlen);
		}

		// Prevents re-run of snap/mjpeg too fast
		usleep(100000);

	} while(mjpeg==1 && outlen>0);

	fprintf(stderr, "Stdout closed\n");
	free(buff);

	return 0;
}
