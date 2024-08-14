#ifndef MEMMAP_H_DEFINED
#define MEMMAP_H_DEFINED


#include <stdint.h>
#include <stdbool.h>


#define ROM_WINDOW_SIZE 0x8000
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
