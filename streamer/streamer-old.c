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

#define BUFFER_LENGTH 128*1024
#define MAX_NAL_LENGTH 16*1024+8
#define MAX_NAL_LIST 4096
#define DEBUG 1
#define HEADER_SIZE 16

// NAL Tracking structure (address + 4 byte header)
struct NAL_TRACK {
	ulong addr;
	char header[HEADER_SIZE];
};

// Globals for simplicity
int fd;							// File descriptor
char *buff;						// Search buffer

// Find NAL in specified buffer, return NAL offset if found or size if not found
ulong findNAL(char* buf, ulong size) {
	//  Search for start of NAL
	int i = 0;
	while (buf[i]!=0 || buf[i+1]!=0 || ( buf[i+2]!=1 && (buf[i+2]!=0 || buf[i+3]!=1) ) ) {
		i++;
		if (i+4 >= size)
			return size;
	}
	// While we have NALs (multiple NAL prevention)
	while (buf[i]==0 && buf[i+1]==0 && ( buf[i+2]==1 || (buf[i+2]==0 && buf[i+3]==1) ) ) {
		i+= (buf[i+2]==0 ? 4 : 3);
		if(i+4 >= size)
			break;
	}
	// Return offset of NAL (past 00 00 [00] 01)
	return i;
}

// Puts a NAL address and header into specified list (and increment length)
void lstAdd(struct NAL_TRACK* lst, int* index, ulong nalAddr, void* nalData) {
	lst[*index].addr = nalAddr;
	memmove(&lst[*index].header, nalData, HEADER_SIZE);
	(*index)++;
	// Always mark end of list
	lst[*index].addr = 0;
}

// Searches specified list starting from the specified address up to specified length
void buildList(struct NAL_TRACK* lst, int* lstLength, ulong address, ulong length) {
	// Set to initial address to read from
	lseek(fd, address, SEEK_SET);

	ulong readSize = 0; 		// How much did we get to read ?
	ulong dataSize = 0;			// Data size available to search (based on reading)
	ulong off = address;		// Current offset of buff within file/memory
	void* pos = buff;			// Position to start searching in buffer
	ulong nalStart;				// NAL Start offset (in buffer)

	// While we have data to read and we haven't reached the maximum length yet
	while((readSize = read(fd, buff + dataSize, BUFFER_LENGTH - dataSize)) > 0 && off<address+length) {
		// Adjust available data size
		dataSize += readSize;

		// While there are NALs in the buffer
		while((nalStart = findNAL(pos, dataSize)) < dataSize && off<address+length) {
			// Adjust position to NAL start
			pos += nalStart;
			off += nalStart;
			dataSize -= nalStart;

			// If this NAL is past our buffer limit, we're done
			if(off>=address+length)
				break;

			// Save NAL address and header to current list
			//if(DEBUG)
			//	fprintf(stderr, "NAL at %06X header=%08X\n", off, ((ulong*) pos)[0] );
			lstAdd(lst, lstLength, off, pos );
		}

		// if no NALs found in buffer, discard it
		if(pos == buff) {
			pos = buff + dataSize;
			off += pos - ((void *) buff);
			dataSize = 0;
		}

		// Shift buffer, adjust offset and reset position
		memmove(buff, pos, dataSize);
		pos = buff;
	}

}

// Write output to standard output, return 0 (false) on success or 1 (true) in case it fails
int output(ulong startAddr, ulong endAddr) {
	fprintf(stderr, "Outputting data from %06X to %06X\n", startAddr, endAddr);
	off_t off = startAddr;
	ulong size = (endAddr-startAddr)+4;
	char sizeStr[12];
	sprintf(sizeStr, "%x\r\n", size);
	write(STDOUT_FILENO, sizeStr, strlen(sizeStr));
	// Write NAL marker if not present (likely on start of output)
	if(write(STDOUT_FILENO, "\x00\x00\x00\x01", 4)<4)
		return 1;
	// Use system call to copy data from the memory/file descriptor to standard output directly (without reading buffer again)
	if(sendfile(STDOUT_FILENO, fd, &off, endAddr-startAddr)<endAddr-startAddr)
		return 1;
	if(write(STDOUT_FILENO, "\r\n", 2)<2)
		return 1;
	return 0;
}

// Main sub
int main(int ac, char **av) {
	char *lastNALBuff;				// Last NAL buffer from PID memory
	void *lstA;						// List A of NALs (4 byte address + header)
	void *lstB;						// List B of NALs (4 byte address + header)
	struct NAL_TRACK *oldLst;		// Points to the old list (A/B)
	int oldLstLen = 0;				// Old list length
	struct NAL_TRACK *newLst;		// Points to the new list (A/B)
	int newLstLen = 0;				// New list length

	// If not enough arguments, show usage and leave
	if(ac < 4) {
		fprintf(stderr, "usage: %s file address length\n", av[0]);
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

	// Debug
	if(DEBUG)
		fprintf(stderr, "Opened %s, search address=%06X, search length=%06X \n", av[1],  searchAddress, searchLength);

	// Create buffers
	buff = malloc(BUFFER_LENGTH);
	lastNALBuff = malloc(MAX_NAL_LENGTH);
	lstA = malloc(MAX_NAL_LIST*HEADER_SIZE);
	lstB = malloc(MAX_NAL_LIST*HEADER_SIZE);
	if(!buff || !lastNALBuff || !lstA || !lstB) {
		fprintf(stderr, "Could not allocate buffers.");
		exit(1);
	}

	// Set initial values
	oldLst = lstA;
	newLst = lstB;

	// Build initial list of NALs
	buildList(oldLst, &oldLstLen, searchAddress, searchLength);

	// Copy lastNAL
	lseek(fd, (searchAddress+searchLength)-MAX_NAL_LENGTH, SEEK_SET);
	read(fd, lastNALBuff, MAX_NAL_LENGTH);

	int writeFailed = 0;
	ulong lastSentAddr = 0;
	while(!writeFailed) {
		// Wait for next frame (15fps)
		usleep(67000);

		// Build new list of NALs
		buildList(newLst, &newLstLen, searchAddress, searchLength);

		int maxIndex = oldLstLen>newLstLen ? oldLstLen : newLstLen;
		int chgNALx = -1;
		for(int x=0;x<maxIndex;x++) {
			// If address or header for this address changed
			if(oldLst[x].addr!=newLst[x].addr || memcmp(oldLst[x].header,newLst[x].header,HEADER_SIZE)!=0 ) {
				chgNALx = x;
				break;
			}
		}

		// By here we should have chgNALx and matchNALx set
		fprintf(stderr, "chgNALx=%d chgAddr=%08X lastSentAddr=%08X\n", chgNALx, newLst[chgNALx].addr, lastSentAddr );

		if(lastSentAddr>0) {
			if(chgNALx>0 && lastSentAddr<newLst[chgNALx-1].addr) {
				output(lastSentAddr, newLst[chgNALx-1].addr-4);
				lastSentAddr = newLst[chgNALx-1].addr;
			} else {
				// Read last NAL to buff
				lseek(fd, (searchAddress+searchLength)-MAX_NAL_LENGTH, SEEK_SET);
				read(fd, buff, MAX_NAL_LENGTH);
				// Compare buff with lastNALBuff
				ulong endAddr = MAX_NAL_LENGTH-4;
				while(buff[endAddr]==lastNALBuff[endAddr] && buff[endAddr+1]==lastNALBuff[endAddr+1] && buff[endAddr+2]==lastNALBuff[endAddr+2] && buff[endAddr+3]==lastNALBuff[endAddr+3]) {
					endAddr--;
				}
				fprintf(stderr, "endAddr=%08X\n",endAddr);
				endAddr += searchAddress+searchLength-MAX_NAL_LENGTH;
				if(endAddr>lastSentAddr)
					output(lastSentAddr, endAddr);
				lastSentAddr = searchAddress+4;
			}
		} else {
			lastSentAddr = newLst[chgNALx-1].addr;
		}

		// Swap lists for next round
		void* tmp = oldLst;
		oldLst = newLst;
		newLst = tmp;
		oldLstLen = newLstLen;
		newLstLen = 0;

	}

	free(lstA);
	free(lstB);
	free(buff);
	free(lastNALBuff);
	return 0;
}


