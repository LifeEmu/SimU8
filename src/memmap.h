#ifndef MEMMAP_H_DEFINED
#define MEMMAP_H_DEFINED


#include <stdint.h>
#include <stdbool.h>


#define ROM_WINDOW_SIZE 0x8000

// On segment 1 and above, code and data memory accesses access the same physical memories
// and an assumption is made here: I assume all pages above page 0 are read-only
#define CODE_PAGE_COUNT 2
#define DATA_PAGE_COUNT 2

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

// number of entries in `DATA_MEMORY_MAP`
#define DATA_MEMORY_REGION_COUNT 7


// `uint8_t handler(uint32_t address, uint8_t data, bool isWrite);`
// Modifies `MemoryStatus`.
// Returns the byte at `address`, or write `data` to `address` if `isWrite` is true.
// The handlers must *not* be `inline`.
typedef uint8_t (*DataMemoryHandler_t)(uint32_t, uint8_t, bool);

// Defines data regions and their respective handlers
typedef struct {
	uint32_t start;
	uint32_t end;
	DataMemoryHandler_t handler;
} DataMemoryRegion_t;


extern const DataMemoryRegion_t DATA_MEMORY_MAP[DATA_MEMORY_REGION_COUNT];


uint8_t defaultHandler(uint32_t address, uint8_t data, bool isWrite);

// All peripherals interact with memory via this function
// Implement this yourself
extern uint8_t SFRHandler(uint32_t address, uint8_t data, bool isWrite);


#endif
