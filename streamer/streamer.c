/*
 * streamer.c
 *
 *  Created on: Nov. 26, 2020
 *      Author: wagner oliveira (wbbo@hotmail.com)
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/sendfile.h>

#define DEBUG 1
#define COMPARE_SIZE 16
#define DIVIDER 1000

// Globals for simplicity
int fd;							// File descriptor
char *buff;						// Search buffer

// Main sub
int main(int ac, char **av) {

	// If not enough arguments, show usage and leave
	if(ac < 4) {
		fprintf(stderr, "usage: %s /proc/NNNN/mem address length\n", av[0]);
		exit(1);
	}

	// Open memory of process
	if((fd = open(av[1], O_RDONLY)) < 0) {
		fprintf(stderr, "Can't access %s", av[1]);
		perror(":");
		exit(1);
	}

	// Set address and length of buffer
	ulong searchAddress = strtoul(av[2], 0, 0);
	ulong searchLength = strtoul(av[3], 0, 0);
	int step=searchLength/DIVIDER;

	// Debug
	if(DEBUG)
		fprintf(stderr, "Opened %s, search address=%06X, search length=%06X step=%d\n", av[1],  searchAddress, searchLength, step);

	// Create buffers
	buff = malloc(searchLength);
	if(!buff) {
		fprintf(stderr, "Could not allocate buffers.");
		exit(1);
	}

	// Copy initial data
	lseek(fd, searchAddress, SEEK_SET);
	if(read(fd, buff, searchLength)<searchLength) {
		fprintf(stderr, "Could not read whole buffer");
		exit(1);
	}

	char cmpBuff[COMPARE_SIZE];
	char skipped[step+COMPARE_SIZE];
	int lastSame = -1;
	int lastChange = -1;
	int chkPos = 4;
	while(1) {
		// Wait for next frame( 0.1s)
		usleep(100000);

		// Find latest change (from start of buffer) within 1000 steps
		int chkCount = 0;
		while(1) {
			// Read data for comparison
			lseek(fd, searchAddress+chkPos, SEEK_SET);
			read(fd, cmpBuff, COMPARE_SIZE);
			// If we found a change
			if(memcmp(buff+chkPos, cmpBuff, COMPARE_SIZE)) {
				fprintf(stderr, "Change detected %06X\n", chkPos);
				// Read skipped data for comparison
				lseek(fd, searchAddress+lastSame, SEEK_SET);
				read(fd, skipped, step+COMPARE_SIZE);
				// Find the first different position since the last match
				int i=0;
				for(i=0;i<step+COMPARE_SIZE;i++) {
					if(skipped[i]!=buff[lastSame+i]) {
						break;
					}
				}
				fprintf(stderr, "Exact change address is %06X i=%d\n", lastSame+i, i);
				// If this isn't the first loop
				if(lastChange>-1) {
					// Change is after previous change
					if(lastSame+i > lastChange) {
						fprintf(stderr, "Output: %06X to %06X\n", lastChange, lastSame+i-1);
						write(STDOUT_FILENO, buff+lastChange, (lastSame+i-1)-lastChange);
					} else {
						while(lastChange<searchLength) {
							// Read skipped data for comparison
							lseek(fd, searchAddress+lastChange, SEEK_SET);
							read(fd, skipped, step+COMPARE_SIZE);
							// Find the first same position at end
							int e=0;
							for(e=0;e<step+COMPARE_SIZE;e++) {
								if(lastChange+e>searchLength) {
									lastChange = searchLength;  // so it gets out of while too
									break;
								}
								// If we found a match
								if(skipped[e]==buff[lastChange+e]) {
									e--;
									fprintf(stderr, "Output1: %06X, size=%06X\n", lastChange, (searchLength-lastChange)+e);
									sendfile(STDOUT_FILENO, fd, (off_t*) (searchAddress+lastChange), (searchLength-lastChange)+e);
									//write(STDOUT_FILENO, buff+lastChange, (searchLength-lastChange)+e);
									lastChange = searchLength; // so it gets out of while too
									break;
								}
							}
							lastChange+=step;
						}
						fprintf(stderr, "Oputput2: 0 to %06X\n", lastSame+i-1);
						sendfile(STDOUT_FILENO, fd, (off_t *) searchAddress, lastSame+i);
						//write(STDOUT_FILENO, buff, lastSame+i-1);
					}
				}
				// Save address of last change
				lastChange = lastSame+i;
				break;
			}
			// Save last position where buffer matched
			lastSame = chkPos;
			// We'll check after the next 'step' bytes
			chkPos+=step;
			// Loop when we reach the end
			if(chkPos>searchLength)
				chkPos = 4;
			// Track how many check was required to find change
			chkCount++;
		}

		// Copy initial data
		lseek(fd, searchAddress, SEEK_SET);
		if(read(fd, buff, searchLength)<searchLength) {
			fprintf(stderr, "Could not read whole buffer");
			exit(1);
		}
	}

	fprintf(stderr, "Stdout closed\n");
	free(buff);
	return 0;
}


