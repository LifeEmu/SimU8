#include <stdint.h>
#include <stdbool.h>

#include "memmap.h"
#include "mmu.h"


// Handler for mismatched addresses
uint8_t defaultHandler(uint32_t address, uint8_t data, bool isWrite) {
	MemoryStatus = MEMORY_UNMAPPED;
	return 0;
}

// --------
// Define your data memory handlers here.
// The handlers must be of the type `DataMemoryHandler_t`.
// The handlers must *not* be `inline`.

static uint8_t codeSegHandler(uint32_t address, uint8_t data, bool isWrite) {
	if( isWrite ) {
		MemoryStatus = MEMORY_READ_ONLY;
		return 0;
	}

	address &= ((CODE_MIRROW_MASK << 16) | 0xffff);	// mirrow memory

/*
// save space (only works for 999CNCW VerA ROM)
// first 0x50000 bytes, plus 18 bytes starting from 0x5ffee
	if( (address >= 0x70000) && (address < 0x72000) ) {
			// 5e000 - 5ffff
			address -= 0x12000;
	}

	if( address < 0x50000 ) {
		return *((uint8_t *)CodeMemory + address);	// return as-is
	}
	else if( address < 0x5ffee ) {
		return 0xff;		// not included
	}
	else if( address < 0x60000 ) {
		return *((uint8_t *)CodeMemory + address - 0xffee);
	}
	return 0xff;
*/
	return *((uint8_t *)CodeMemory + address);
}

static uint8_t romWindowHandler(uint32_t address, uint8_t data, bool isWrite) {
	++ROMWinAccessCount;
	MemoryStatus = MEMORY_ROM_WINDOW;

	return codeSegHandler(address, data, isWrite);
}

static uint8_t RAMHandler(uint32_t address, uint8_t data, bool isWrite) {
	uint8_t *p = (uint8_t *)DataMemory + address - ROM_WINDOW_SIZE;

	if( isWrite ) {
		*p = data;
		return 0;
	}
	else {
		return *p;
	}
}

static uint8_t VRAMHandler(uint32_t address, uint8_t data, bool isWrite) {
	if( (address & 0x1F) >= 0x18 ) {	// unmapped region of VRAM
		if( isWrite ) {
			MemoryStatus = MEMORY_UNMAPPED;
		}
		return 0;
	}

	return RAMHandler(address, data, isWrite);
}

// Define your memory regions here. Mismatched addresses defaults to unmapped addresses
// Note that you shouldn't change the name of it.
// I stole the idea from FraserBc :P

// default memory map, for real ES+
const DataMemoryRegion_t DATA_MEMORY_MAP[DATA_MEMORY_REGION_COUNT] = {
//	start		end +1		handler
	{0x09000,	0x0f000,	RAMHandler},		// CWII RAM
	{0x0f000,	0x10000,	SFRHandler},		// CWII SFRs
	{0x00000,	0x09000,	romWindowHandler},	// ROM window handler
	{0x10000,	0x100000,	codeSegHandler},	// segment 1-F

	{0x000000,	0x1000000,	defaultHandler}		// unmapped regions
};
