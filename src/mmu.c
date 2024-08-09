#include <stdint.h>
#include <stdbool.h>


#include "../inc/mmu.h"
#include "../inc/mmustub.h"


void *CodeMemory = NULL;
void *DataMemory = NULL;
bool IsMemoryInited = false;
// Status of last memory function
MEMORY_STATUS MemoryStatus;
// Tracks how many ROM window access has happened
unsigned int ROMWinAccessCount = 0;


// Initializes `CodeMemory` and `DataMemory`.
MEMORY_STATUS memoryInit(stub_mmuFileID_t codeFileID, stub_mmuFileID_t dataFileID) {
	stub_mmuInitStruct_t s = {
		.codeMemoryID = codeFileID,
		.dataMemoryID = dataFileID,
		.codeMemorySize = CODE_PAGE_COUNT * 0x10000,
		.dataMemorySize = 0x10000 - ROM_WINDOW_SIZE
	};

	if( (CodeMemory = stub_mmuInitCodeMemory(s)) == NULL ) {
		return MEMORY_ALLOCATION_FAILED;
	}

	if( (DataMemory = stub_mmuInitDataMemory(s)) == NULL ) {
		stub_mmuFreeCodeMemory(CodeMemory);
		CodeMemory = NULL;
		return MEMORY_ALLOCATION_FAILED;
	}

	IsMemoryInited = true;
	return MEMORY_OK;
}

// Saves data in *DataMemory into file
MEMORY_STATUS memorySaveData(stub_mmuFileID_t dataFileID) {
	stub_mmuInitStruct_t s = {
		.dataMemoryID = dataFileID,
		.dataMemorySize = 0x10000 - ROM_WINDOW_SIZE
	};

	if( stub_mmuSaveDataMemory(s, DataMemory) == STUB_MMU_ERROR )
		return MEMORY_SAVING_FAILED;

	return MEMORY_OK;
}

// Loads data memory from a binary file
// WARNING: This will overwrite existing file!!!
MEMORY_STATUS memoryLoadData(stub_mmuFileID_t dataFileID) {
	stub_mmuInitStruct_t s = {
		.dataMemoryID = dataFileID,
		.dataMemorySize = 0x10000 - ROM_WINDOW_SIZE
	};

	if( IsMemoryInited == false )
		return MEMORY_UNINITIALIZED;

	if( stub_mmuLoadDataMemory(s, DataMemory) == STUB_MMU_ERROR )
		return MEMORY_LOADING_FAILED;

	return MEMORY_OK;
}

// Frees memory allocated
MEMORY_STATUS memoryFree(void) {
	if( IsMemoryInited == false )
		return MEMORY_UNINITIALIZED;

	stub_mmuFreeCodeMemory(CodeMemory);
	stub_mmuFreeDataMemory(DataMemory);

	IsMemoryInited = false;
	return MEMORY_OK;
}

// Fetches a word from code memory
// It aligns to word boundary
// It returns `0xffff` in unmapped pages
uint16_t memoryGetCodeWord(SR_t segment, PC_t offset) {
	MemoryStatus = MEMORY_OK;

	if( IsMemoryInited == false ) {
		MemoryStatus = MEMORY_UNINITIALIZED;
		return 0;
	}

	segment &= 0x0f;	// limit segment to 0~15
	offset &= 0xfffe;	// align to word boundary

	if( (segment & CODE_MIRROW_MASK) >= CODE_PAGE_COUNT ) {
		MemoryStatus = MEMORY_UNMAPPED;
		return 0xffff;
	}

	if( segment > CODE_MIRROW_MASK ) {
		segment &= CODE_MIRROW_MASK;
		MemoryStatus = MEMORY_MIRROWED_BANK;
	}

	return *((uint16_t*)(CodeMemory + (segment << 16) + offset));
}

// fetches some data from data memory
// Unmapped memory reads 0
// size can only be 1, 2, 4, 8
uint64_t memoryGetData(SR_t segment, EA_t offset, size_t size) {
	uint64_t retVal = 0;
	MemoryStatus = MEMORY_OK;

	ROMWinAccessCount = 0;
	if( IsMemoryInited == false ) {
		MemoryStatus = MEMORY_UNINITIALIZED;
		return 0;
	}

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
		(offset & 1)? MemoryStatus = MEMORY_UNALIGNED : 0;
		offset &= 0xfffe;	// align to word boundary
	}

	offset += size;

	if( segment == 0 ) {
		if( (offset > 0x8DFF) && (offset < 0xF000) ) {
			// unmapped memory
			// This is just a temporary solution
			return 0;
		}
		else {
			do {
				retVal <<= 8;
				--offset;
				if( offset < ROM_WINDOW_SIZE ) {
					// ROM window
					++ROMWinAccessCount;
					MemoryStatus = MEMORY_ROM_WINDOW;
					retVal |= *(uint8_t*)(CodeMemory + offset);
				}
				else {
					// RAM in rest of segment 0
					retVal |= *(uint8_t*)(DataMemory - ROM_WINDOW_SIZE + offset);
				}

			} while( --size != 0 );

			return retVal;
		}
	}

	// Else it's data segment 1+
	// Compiler PLEASE optimize this you know what I want to do
	if( (_mapToDataSeg(segment) == -1)) {
		// Unmapped memory
		return 0;
	}
	// Else it's valid
	segment = (uint8_t)(_mapToDataSeg(segment) & 0xff);

	do {
		retVal <<= 8;
		retVal |= *(uint8_t*)(CodeMemory + (segment << 16) + --offset);
	} while( --size != 0 );

	return retVal;
}

// writes some data into data memory
// size can only be 1, 2, 4, 8
void memorySetData(SR_t segment, EA_t offset, size_t size, uint64_t data) {

	if( IsMemoryInited == false ) {
		MemoryStatus = MEMORY_UNINITIALIZED;
		return;
	}

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
		MemoryStatus = ((offset & 1)? MEMORY_UNALIGNED : MEMORY_OK);
		offset &= 0xfffe;	// align to word boundary
	}

	// I assume everything above data segment 0 is read-only
	if( (segment != 0)) {
		MemoryStatus = MEMORY_READ_ONLY;
		return;
	}

	while( size-- > 0 ) {
		if( offset >= ROM_WINDOW_SIZE ) {
			// data region of segment 0
			if( (offset > 0x8DFF) && (offset < 0xF000) ) {
				// temporary solution
				MemoryStatus = MEMORY_READ_ONLY;
			}
			else {
				*(uint8_t*)(DataMemory + offset - ROM_WINDOW_SIZE) = (uint8_t)(data & 0xff);
			}
		}
		else {
			// code memory
			MemoryStatus = MEMORY_READ_ONLY;	// assume code memory is read-only
		}
		++offset;

		data >>= 8;
	}
}
