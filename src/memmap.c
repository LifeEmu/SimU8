#include <stdint.h>
#include <stdbool.h>

#include "../inc/memmap.h"
#include "../inc/mmu.h"


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

	return *((uint8_t *)CodeMemory + (address & 0x1ffff));
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
	if( (address & 0xF) >= 0xC ) {	// unmapped region of VRAM
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
	{0x00000,	0x08000,	romWindowHandler},	// ROM window handler
	{0x0f800,	0x0fa00,	VRAMHandler},		// ES+ VRAM
	{0x08000,	0x08e00,	RAMHandler},		// ES+ RAM
	{0x0f000,	0x0f050,	SFRHandler},		// ES+ SFRs
	{0x10000,	0x20000,	codeSegHandler},	// segment 1
	{0x80000,	0xa0000,	codeSegHandler},	// segment 8+

	{0x000000,	0x1000000,	defaultHandler}		// unmapped regions
};
