#ifndef MEMSETUP_H_INCLUDED
#define MEMSETUP_H_INCLUDED


// On segment 1 and above, code and data memory accesses access the same physical memories
// and an assumption is made here: I assume all pages above page 0 are read-only
#define CODE_PAGE_COUNT 2
#define DATA_PAGE_COUNT 2
// OKI says ROM window can only be "max 32K bytes", but CWI has ROM window of 0xD000 bytes
// Maybe CASIO asked for a modified core? I don't know.
#define ROM_WINDOW_SIZE 0x8000

// mask for page mirrowing
// If there're 3 code pages (0~3), mirrow mask should be set to 0x03
// real page < 3, mirrowed page > mask
/* Page	| Type
 * 0	| real
 * 1	| real
 * 2	| real
 * 3	| unmapped
 * 4	| mirrow
 * 5	| mirrow
 * 6	| mirrow
 * 7	| mirrow
 */
#define CODE_MIRROW_MASK 0x01
#define DATA_MIRROW_MASK 0x07


// Define this macro to use custom map logic
//#define CUSTOM_SEGMENT_MAP

// To adapt to CWI (and possibly other models) mapping Cseg0 to Dseg5, the conversion is implemented here


#ifndef CUSTOM_SEGMENT_MAP
// Default mapping logic
extern MEMORY_STATUS MemoryStatus;
static inline int16_t _mapToDataSeg(SR_t ds) {
	// mask segment so it resides in mirrow range
	if( ds > DATA_MIRROW_MASK ) {
		ds &= DATA_MIRROW_MASK;
		MemoryStatus = MEMORY_MIRROWED_BANK;
	}

	if( ds >= DATA_PAGE_COUNT ) {
		MemoryStatus = MEMORY_UNMAPPED;
		return -1;
	}

	return ds;
}
#else
// - This is called every time a data access to Data segment 1 and above is made
// I assume anything above data segment 0 is read-only so I can just not bother with writes but hooks
// - In: Data segment number that the core is trying to access
// - Out: Code segment number that you wish the core to see, `-1` means invalid
// - Return the input as-is to map code segment X to data segment X
extern int16_t _mapToDataSeg(SR_t ds);
#endif

#endif
