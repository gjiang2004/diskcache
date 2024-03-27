#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cache.h"
#include "jbod.h"
#include "mdadm.h"
#include "net.h"

int is_mounted = 0;
int is_written = 0;

int mdadm_mount(void) {
	// YOUR CODE GOES HERE
	jbod_cmd_t mount = JBOD_MOUNT;
	mount = mount << 12;
	int r = jbod_client_operation(mount, NULL);
	if (r == 0) {
		return 1;
	} else {
		return -1;
	}
}

int mdadm_unmount(void) {
	// YOUR CODE GOES HERE
	jbod_cmd_t unmount = JBOD_UNMOUNT;
	unmount = unmount << 12;
	int r = jbod_client_operation(unmount, NULL);
	if (r == 0) {
		return 1;
	} else {
		return -1;
	}
}

int mdadm_write_permission(void){
	// YOUR CODE GOES HERE
	jbod_cmd_t writeperm = JBOD_WRITE_PERMISSION;
	writeperm = writeperm << 12;
	int r = jbod_client_operation(writeperm, NULL);
	if (r == 0) {
		return 1;
	} else {
		return -1;
	}
}


int mdadm_revoke_write_permission(void){
	// YOUR CODE GOES HERE
	jbod_cmd_t revokeperm = JBOD_REVOKE_WRITE_PERMISSION;
	revokeperm = revokeperm << 12;
	int r = jbod_client_operation(revokeperm, NULL);
	if (r == 0) {
		return 1;
	} else {
		return -1;
	}
}


int mdadm_read(uint32_t start_addr, uint32_t read_len, uint8_t *read_buf)  {
	// YOUR CODE GOES HERE
	if (read_len > 1024) {
    	return -1;
	}
	if (start_addr + read_len >= 1048576) {
		return -1;
	}
	if (read_buf == NULL && read_len != 0) {
		return -1;
	}
	if (read_buf == NULL && read_len == 0) {
		return 0;
	}
	if (mdadm_mount() == 1) {
		return -1;
	}

	int currentDiskID = start_addr / 65536;
	int currentBlockID = (start_addr - currentDiskID * 65536) / 256;
	int currentAddressID = start_addr % 256;

	jbod_cmd_t seektodisk = JBOD_SEEK_TO_DISK;
	jbod_cmd_t seektoblock = JBOD_SEEK_TO_BLOCK;
	uint8_t buffer[256] = {0};
	jbod_cmd_t read = JBOD_READ_BLOCK;
	read = read << 12;
	seektodisk = seektodisk << 12;
	seektoblock = seektoblock << 8;
	seektodisk += currentDiskID;
	seektoblock += currentBlockID;
	seektoblock = seektoblock << 4;

	jbod_client_operation(seektodisk, NULL);
	jbod_client_operation(seektoblock, NULL);
	if (cache_lookup(currentDiskID, currentBlockID, buffer) == -1) {
		jbod_client_operation(read+currentBlockID*16, buffer);
		cache_insert(currentDiskID, currentBlockID, buffer);
	}
	
	*read_buf = buffer[currentAddressID];
	int len = read_len;

	while (len > 0) {
		if (len <= 256 - currentAddressID) {
			for(int j = currentAddressID; j < len + currentAddressID; j++) {
				*read_buf = buffer[j];
				read_buf++;
			}
		} else {
			for(int j = currentAddressID; j < 256; j++) {
				*read_buf = buffer[j];
				read_buf++;
			}
		}
		seektoblock += 16;
		currentBlockID++;
		if (currentBlockID >= 256) {
			currentBlockID = 0;
			currentDiskID++;
			seektoblock = JBOD_SEEK_TO_BLOCK;
			seektoblock = seektoblock << 12;
			seektodisk++;
		}

		jbod_client_operation(seektodisk, NULL);
		jbod_client_operation(seektoblock, NULL);
		if (cache_lookup(currentDiskID, currentBlockID, buffer) == -1) {
			jbod_client_operation(read+currentBlockID*16, buffer);
			cache_insert(currentDiskID, currentBlockID, buffer);
		}
		len -= (256 - currentAddressID);
		currentAddressID = 0;
	}
	return read_len;
}



int mdadm_write(uint32_t start_addr, uint32_t write_len, const uint8_t *write_buf) {
	// YOUR CODE GOES HERE
	if (write_len > 1024) {
    	return -1;
	}
	if (start_addr >= 1048576) {
		return -1;
	}
	if (start_addr + write_len > 1048576) {
		return -1;
	}
	if (write_buf == NULL && write_len != 0) {
		return -1;
	}
	if (write_buf == NULL && write_len == 0) {
		return 0;
	}
	if (mdadm_mount() == 1) {
		return -1;
	}
	if (mdadm_write_permission() == 1) {
		return -1;
	}

	int currentDiskID = start_addr / 65536;
	int currentBlockID = (start_addr - currentDiskID * 65536) / 256;
	
	uint8_t buffer[256];

	int len = write_len;
	int currentAddressID = start_addr % 256;

	while (len > 0) {
		jbod_cmd_t seektodisk = JBOD_SEEK_TO_DISK;
		jbod_cmd_t seektoblock = JBOD_SEEK_TO_BLOCK;
		seektodisk = seektodisk << 12;
		seektoblock = seektoblock << 8;
		seektodisk += currentDiskID;
		seektoblock += currentBlockID;
		seektoblock = seektoblock << 4;
		jbod_client_operation(seektodisk, NULL);
		jbod_client_operation(seektoblock, NULL);
		jbod_cmd_t read = JBOD_READ_BLOCK;
		read = read << 12;
		if (cache_lookup(currentDiskID, currentBlockID, buffer) == -1) {
			jbod_client_operation(read+currentBlockID*16, buffer);
			for (int i = currentAddressID; i < 256 && len > 0; i++) {
				buffer[i] = *(write_buf);
				write_buf++;
				len--;
			}
			cache_insert(currentDiskID, currentBlockID, buffer);
		} else {
			for (int i = currentAddressID; i < 256 && len > 0; i++) {
				buffer[i] = *(write_buf);
				write_buf++;
				len--;
			}
			cache_update(currentDiskID, currentBlockID, buffer);
		}
		
		seektoblock = JBOD_SEEK_TO_BLOCK;
		seektoblock = seektoblock << 8;
		seektoblock += currentBlockID;
		seektoblock = seektoblock << 4;
		jbod_client_operation(seektoblock, NULL);

		jbod_cmd_t writeblock = JBOD_WRITE_BLOCK;
		writeblock = writeblock << 8;
		writeblock += currentBlockID;
		writeblock = writeblock << 4;
		jbod_client_operation(writeblock, buffer);

		seektoblock += 16;
		currentBlockID++;
		if (currentBlockID >= 256) {
			currentBlockID = 0;
			currentDiskID++;
			seektoblock = JBOD_SEEK_TO_BLOCK;
			seektoblock = seektoblock << 12;
			seektodisk++;
		}
		currentAddressID = 0;
	}
	return write_len;
}