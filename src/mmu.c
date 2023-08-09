#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>


#include "../inc/mmu.h"


void *CodeMemory = NULL;
void *DataMemory = NULL;
bool IsMemoryInited = false;

// the word fetched from code memory
uint16_t CodeWord = 0;
// data fetched from data memory
Data_t DataRaw = {0};
// Tracks how many ROM window access has happened
int ROMWinAccessCount = 0;


// Initializes `CodeMemory` and `DataMemory`.
MEMORY_STATUS memoryInit(char *CodeFileName, char *DataFileName) {
	FILE *CodeFile, *DataFile;
	if( (CodeMemory = malloc((size_t)(CODE_PAGE_COUNT * 0x10000))) == NULL )
		return MEMORY_ALLOCATION_FAILED;

	if( (DataMemory = malloc((size_t)(0x10000 - ROM_WINDOW_SIZE))) == NULL ) {
		free(CodeMemory);
		return MEMORY_ALLOCATION_FAILED;
	}

	if( (CodeFile = fopen(CodeFileName, "rb")) == NULL) {
		free(CodeMemory);
		free(DataMemory);
		return MEMORY_ROM_MISSING;
	}

	fread(CodeMemory, sizeof(uint8_t), (size_t)(CODE_PAGE_COUNT * 0x10000), CodeFile);
	fclose(CodeFile);

	if( (DataFile = fopen(DataFileName, "rb")) == NULL) {
		memset(DataMemory, 0, (size_t)0x10000 - ROM_WINDOW_SIZE);
	}
	else {
		fread(DataMemory, sizeof(uint8_t), (size_t)(0x10000 - ROM_WINDOW_SIZE), DataFile);
		fclose(DataFile);
	}

	IsMemoryInited = true;
	return MEMORY_OK;
}


// Saves data in *DataMemory into file
MEMORY_STATUS memorySaveData(char *DataFileName) {
	FILE *DataFile;
	if( (DataFile = fopen(DataFileName, "wb")) == NULL )
		return MEMORY_SAVING_FAILED;

	fwrite(DataMemory, sizeof(uint8_t), (size_t)(0x10000 - ROM_WINDOW_SIZE), DataFile);
	fclose(DataFile);

	return MEMORY_OK;
}

// Frees memory allocated
MEMORY_STATUS memoryFree(void) {
	if( IsMemoryInited == false )
		return MEMORY_UNINITIALIZED;

	free(CodeMemory);
	CodeMemory = NULL;

	free(DataMemory);
	DataMemory = NULL;

	IsMemoryInited = false;
	return MEMORY_OK;
}

// Fetches a word from code memory
// The word fetched is returned in `CodeWord`
// It aligns to word boundary
// It returns `0xffff` in unmapped pages
MEMORY_STATUS memoryGetCodeWord(SR_t segment, PC_t offset) {
	MEMORY_STATUS retVal = MEMORY_OK;

	if( IsMemoryInited == false )
		return MEMORY_UNINITIALIZED;

	segment &= 0x0f;	// limit segment to 0~15
	offset &= 0xfffe;	// align to word boundary

	if( (segment & CODE_MIRROW_MASK) >= CODE_PAGE_COUNT ) {
		CodeWord = 0xffff;
		return MEMORY_UNMAPPED;
	}

	if( segment > CODE_MIRROW_MASK ) {
		segment &= CODE_MIRROW_MASK;
		retVal = MEMORY_MIRROWED_BANK;
	}

	CodeWord = *((uint16_t*)(CodeMemory + (segment << 16) + offset));
	return retVal;

}

// fetches some data from data memory
// the data fetched is returned in `DataRaw`
// size can only be 1, 2, 4, 8
MEMORY_STATUS memoryGetData(SR_t segment, EA_t offset, size_t size) {
	MEMORY_STATUS retVal = MEMORY_OK;

	ROMWinAccessCount = 0;
	if( IsMemoryInited == false )
		return MEMORY_UNINITIALIZED;

	segment &= 0xff;	// limit segment to 0~255
	switch( size ) {	// validate size
		case 0:
		case 1:
			// byte
			size = 1;
			break;
		case 2:
			// word
			size = 2;
			break;
		case 3:
		case 4:
			// dword
			size = 4;
			break;
		default:
			// qword
			size = 8;
	}
	if( size > 1 ) {
		(offset & 1)? retVal = MEMORY_UNALIGNED : 0;
		offset &= 0xfffe;	// align to word boundary
	}

	DataRaw.raw = 0;

	// mask segment so it resides in mirrow range
	if( segment > DATA_MIRROW_MASK ) {
		retVal = MEMORY_MIRROWED_BANK;
	}

	if( (segment & DATA_MIRROW_MASK) >= DATA_PAGE_COUNT )
		return MEMORY_UNMAPPED;

	offset += size;

	while( true ) {
		--offset;	// start reading from higher address towards lower address
		if( (segment == 0) && (offset < ROM_WINDOW_SIZE) ) {
			++ROMWinAccessCount;
			retVal = MEMORY_ROM_WINDOW;
		}

		if( (segment == 0) && (offset >= ROM_WINDOW_SIZE) ) {
			// data region of segment 0
			DataRaw.byte |= *(uint8_t*)(DataMemory + offset - ROM_WINDOW_SIZE);
		}
		else {
			// code memory
			DataRaw.byte |= *(uint8_t*)(CodeMemory + ((segment & DATA_MIRROW_MASK) << 16) + offset);
		}
		if( --size == 0 )
			break;
		DataRaw.raw <<= 8;
	}

	return retVal;
}

// writes some data into data memory
// size can only be 1, 2, 4, 8
MEMORY_STATUS memorySetData(SR_t segment, EA_t offset, size_t size, Data_t data) {
	MEMORY_STATUS retVal = MEMORY_OK;

	if( IsMemoryInited == false )
		return MEMORY_UNINITIALIZED;

	segment &= 0xff;	// limit segment to 0~255
	switch( size ) {	// validate size
		case 0:
		case 1:
			// byte
			size = 1;
			break;
		case 2:
			// word
			size = 2;
			break;
		case 3:
		case 4:
			// dword
			size = 4;
			break;
		default:
			// qword
			size = 8;
	}
	if( size > 1 ) {
		(offset & 1)? retVal = MEMORY_UNALIGNED : 0;
		offset &= 0xfffe;	// align to word boundary
	}

	DataRaw.raw = 0;

	// mask segment so it resides in mirrow range
	if( segment > DATA_MIRROW_MASK ) {
		segment &= DATA_MIRROW_MASK;
		retVal = MEMORY_MIRROWED_BANK;
	}

	if( segment >= DATA_PAGE_COUNT )
		return MEMORY_UNMAPPED;

/*
	while( size-- > 0 ) {
		*((uint8_t*)(DataMemory + (segment << 16) + offset++)) = data.byte;
		data.raw >>= 8;
	}
*/
	while( size-- > 0 ) {
		if( (segment == 0) && (offset >= ROM_WINDOW_SIZE) ) {
			// data region of segment 0
			*(uint8_t*)(DataMemory + (segment << 16) + offset - ROM_WINDOW_SIZE) = data.byte;
		}
		else {
			// code memory
			retVal = MEMORY_READ_ONLY;	// assume code memory is read-only
		}
		++offset;

		data.raw >>= 8;
	}


	return retVal;
}
