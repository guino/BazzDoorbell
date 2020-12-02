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
#define MAX_NAL_LENGTH 64*1024+8
#define MAX_NAL_LIST 4096
#define DEBUG 1
#define HEADER_SIZE 8

// NAL Tracking structure (address + 4 byte header)
struct NAL_TRACK {
	ulong addr;
	char header[HEADER_SIZE];
};

// Globals for simplicity
int fd;							// File descriptor
void *buff;						// Search buffer

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
			if(DEBUG)
				fprintf(stderr, "NAL at %06X header=%08X\n", off, ((ulong*) pos)[0] );
			lstAdd(lst, lstLength, off, pos );
		}

		// if no NALs found in buffer, discard it
		if(pos == buff) {
			pos = buff + dataSize;
			off += pos - buff;
			dataSize = 0;
		}

		// Shift buffer, adjust offset and reset position
		memmove(buff, pos, dataSize);
		//off += pos - buff;
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
	void *lastNALBuff;				// Last NAL buffer from PID memory
	void *lstA;						// List A of NALs (4 byte address + 4 first NAL bytes)
	void *lstB;						// List B of NALs (4 byte address + 4 first NAL bytes)
	struct NAL_TRACK *curLst;		// Points to the current list (A/B) we're iterating over
	int curLstLen = 0;				// Current list length
	struct NAL_TRACK *newLst;		// Points to the new list (A/B) we're building while iterating current list
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
	curLst = lstA;
	newLst = lstB;

	// Build initial list of NALs
	buildList(curLst, &curLstLen, searchAddress, searchLength);

	// ---- SYNC ----

	// Header being checked;(basically a small buffer)
	char chkHeader[HEADER_SIZE];
	// Number of retries (prevention in case data is not changing for some reason)
	int retries=0;
	// Index of the 'most current' NAL (the newest unchanged NAL in the memory)
	int curNALx = -1;
	// Wait until we have a valid current NAL
	while(curNALx==-1 && retries++<15) {
		// Give time for circular buffer to change -- 1s/15 (fps) = 67ms should be enough for at least 1 frame
		usleep(67000);
		// Iterate over current list, until we find a zero address value
		for(int x=0;curLst[x].addr!=0;x++) {
			// Seek to address to be checked
			lseek(fd, curLst[x].addr, SEEK_SET);
			// Read it
			read(fd, &chkHeader, HEADER_SIZE);
			// If it changed
			if(memcmp(chkHeader,curLst[x].header,HEADER_SIZE)!=0) {
				if(DEBUG)
					fprintf(stderr, "Change detected address=%06X old=%08X new=%08X\n", curLst[x].addr, curLst[x].header, chkHeader);
				// Set current NAL (newest NAL in buffer)
				curNALx = x>0 ? x-1 : curLstLen-1;
				// Copy curLst to newLst up to current NAL
				newLstLen=0;
				for(int n=0;n<=x;n++)
					lstAdd(newLst, &newLstLen, curLst[n].addr, &curLst[n].header);
				// Found change so exit the for loop (which in turn will also exit while loop)
				break;
			}
		}
	}

	if(DEBUG)
		fprintf(stderr, "Search retries=%d, current NAL index=%d, address=%06X header=%08X\n", retries-1, curNALx, curLst[curNALx].addr, curLst[curNALx].header);

	// If we found a change (otherwise we'll bail out)
	if(curNALx>=0) {

		// At this point (and every iteration below) we should always have a few things set/reset:
		// curNALx should point to the most current NAL index of the circular buffer
		// curLst should be the the most up-to-date list of NALs (used to detect changes)
		// curLstLen must be accurate
		// newLst should be up-to-date with curLst from index 0 up to curNALx (so we can start adding new NALs to it)
		// newLstLen must be accurate

		ulong lastMatchAddr = 0;
		// While we don't have issues writing output (i.e. until stdout is closed)
		int failedWrite = 0;
		while(!failedWrite) {
			// We'll scan curLst to find  next NAL match and set matchNALx
			int matchNALx = 0;
			// Iterate over current list from curNALx until we find a match (or end of list)
			for(int x=curNALx+1;curLst[x].addr!=0;x++) {
				// Seek to address to be checked
				lseek(fd, curLst[x].addr, SEEK_SET);
				// Read it
				read(fd, &chkHeader, HEADER_SIZE);
				// If it's a match and it's past the last Match
				if(memcmp(chkHeader,curLst[x].header,HEADER_SIZE)==0 && curLst[x].addr > lastMatchAddr) {
					if(DEBUG)
						fprintf(stderr, "Match detected address=%06X\n", curLst[x].addr);
					matchNALx = x;
					// Save last match Address
					lastMatchAddr = curLst[x].addr;
					break;
				}
			}

			// NOTE: If no match is found by the end of curLst we leave matchNALx as 0 so we can start over on next iteration

			// Set length (to search NALs) to either end of circular buffer OR first old/matching NAL
			ulong newLength = matchNALx == 0 ? (searchAddress+searchLength)-curLst[curNALx].addr : curLst[matchNALx].addr-curLst[curNALx].addr;

			// Build upon newLst to include new NALs found between curNalx and either matchNALx or end of buffer
			if(DEBUG)
				fprintf(stderr, "Getting new NALs from %06X to %06X\n", curLst[curNALx].addr,curLst[curNALx].addr+newLength);
			buildList(newLst, &newLstLen, curLst[curNALx].addr,newLength);

			// If we got new data
			if(newLstLen-1>curNALx) {
				// Output data from curNALx up to latest NAL found
				output(newLst[curNALx].addr, newLst[newLstLen-1].addr-4);
				// Reset last match address
				lastMatchAddr = 0;
			}

			// If we found a match before end of list
			if(matchNALx>curNALx) {
				// Most current NAL is the newest one in newLst
				curNALx = newLstLen - 1;
				// Copy the remainder of the current list (from match) onto new list (as these NALs are still there too)
				for(int x=matchNALx;curLst[x].addr!=0;x++)
					lstAdd(newLst, &newLstLen, curLst[x].addr, &curLst[x].header);
			} else {
				// TODO Handle lastNAL
				// Most current NAL (possibly old) will be the first in circular buffer
				curNALx = 0;
				// Reset last match address
				lastMatchAddr = 0;
			}

			// The new list is now the current one (since it is the most up-to-date), swap them
			void * tmp = curLst;
			curLst = newLst;
			newLst = tmp;
			curLstLen = newLstLen;
			// Rebuild newLst (+newLstLen) with everything up to curNALx
			newLstLen=0;
			for(int x=0;x<=curNALx;x++)
				lstAdd(newLst, &newLstLen, curLst[x].addr, &curLst[x].header);

			// Wait for a new frame
			usleep(67000);
		}
	}

	free(lstA);
	free(lstB);
	free(buff);
	free(lastNALBuff);
	return 0;
}


