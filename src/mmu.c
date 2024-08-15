#include <stdint.h>
#include <stdbool.h>

#include "regtypes.h"
#include "memtypes.h"
#include "mmustub.h"
#include "memmap.h"


void *CodeMemory = NULL;
void *DataMemory = NULL;
bool IsMemoryInited = false;
// Status of last memory operation
MEMORY_STATUS MemoryStatus;
// Tracks how many ROM window access has happened
unsigned int ROMWinAccessCount = 0;


// Initializes `CodeMemory` and `DataMemory`.
MEMORY_STATUS memoryInit(stub_MMUFileID_t codeFileID, stub_MMUFileID_t dataFileID) {
	stub_MMUInitStruct_t s = {
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
MEMORY_STATUS memorySaveData(stub_MMUFileID_t dataFileID) {
	stub_MMUInitStruct_t s = {
		.dataMemoryID = dataFileID,
		.dataMemorySize = 0x10000 - ROM_WINDOW_SIZE
	};

	if( stub_mmuSaveDataMemory(s, DataMemory) == STUB_MMU_ERROR )
		return MEMORY_SAVING_FAILED;

	return MEMORY_OK;
}

// Loads data memory from a binary file
// WARNING: This will overwrite existing file!!!
MEMORY_STATUS memoryLoadData(stub_MMUFileID_t dataFileID) {
	stub_MMUInitStruct_t s = {
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


// Looks up `address` in `DATA_MEMORY_MAP`.
// Returns a pointer to matching entry for `address`
// This is an internal helper function. you don't really need to call it.
static const DataMemoryRegion_t * lookupRegion(uint32_t address) {
	unsigned int index;
	const DataMemoryRegion_t *p = DATA_MEMORY_MAP;

	for( index = 0; index < DATA_MEMORY_REGION_COUNT; ++index ) {
		if( (address >= p -> start) && (address < p -> end) )
			break;
		++p;
	}
	return p;
}


// fetches some data from data memory
// Unmapped memory reads 0
// size can only be 1, 2, 4, 8
uint64_t memoryGetData(SR_t segment, EA_t offset, size_t size) {
	uint64_t retVal = 0;
	uint32_t flatAddress;
	const DataMemoryRegion_t * region;

	MemoryStatus = MEMORY_OK;

	ROMWinAccessCount = 0;
	if( IsMemoryInited == false ) {
		MemoryStatus = MEMORY_UNINITIALIZED;
		return 0;
	}

	// validate size
	switch( size ) {
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

	// align to word boundary
	if( size > 1 ) {
		(offset & 1)? MemoryStatus = MEMORY_UNALIGNED : 0;
		offset &= 0xfffe;
	}

	flatAddress = (segment << 16) + offset;

	// lookup for the region the address belongs to
	region = lookupRegion(flatAddress);

	if( (flatAddress += size - 1) < region -> end ) {
		// all the accesses happen within the region
		// so we can do only 1 lookup
		do {
			retVal <<= 8;
			retVal |= (*(region -> handler))(flatAddress--, 0, false);
		} while( --size != 0 );

		return retVal;
	}
	else {
		// this single access splits across different regions
		// we do multiple lookups to ensure compatibility
		do {
			region = lookupRegion(flatAddress);
			retVal <<= 8;
			retVal |= (*(region -> handler))(flatAddress--, 0, false);
		} while( --size != 0 );

		return retVal;
	}
}

// writes some data into data memory
// size can only be 1, 2, 4, 8
void memorySetData(SR_t segment, EA_t offset, size_t size, uint64_t data) {
	uint32_t flatAddress;
	const DataMemoryRegion_t * region;

	MemoryStatus = MEMORY_OK;

	ROMWinAccessCount = 0;
	if( IsMemoryInited == false ) {
		MemoryStatus = MEMORY_UNINITIALIZED;
		goto exit;
	}

	// validate size
	switch( size ) {
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

	// align to word boundary
	if( size > 1 ) {
		(offset & 1)? MemoryStatus = MEMORY_UNALIGNED : 0;
		offset &= 0xfffe;
	}

	flatAddress = (segment << 16) + offset;

	// lookup for the region the address belongs to
	region = lookupRegion(flatAddress);

	if( (flatAddress + size - 1) < region -> end ) {
		// all the accesses happen within the region
		// so we can do only 1 lookup
		do {
			(*(region -> handler))(flatAddress++, data & 0xff, true);
			data >>= 8;
		} while( --size != 0 );
	}
	else {
		// this single access splits across different regions
		// we do multiple lookups to ensure compatibility
		do {
			(*(region -> handler))(flatAddress++, data & 0xff, true);
			data >>= 8;
			if( --size == 0 )
				break;
			region = lookupRegion(flatAddress);
		} while( 1 );
	}
exit:
	;
}
