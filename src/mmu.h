#ifndef MMU_H_INCLUDED
#define MMU_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif


#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "regtypes.h"
#include "memtypes.h"
#include "mmustub.h"


extern uint8_t *CodeMemory;
extern uint8_t *DataMemory;
extern bool IsMemoryInited;
// Status of last memory operation
extern MEMORY_STATUS MemoryStatus;
// Tracks how many ROM window access has happened
extern unsigned int ROMWinAccessCount;


MEMORY_STATUS memoryInit(stub_MMUFileID_t codeFileID, stub_MMUFileID_t dataFileID);
MEMORY_STATUS memorySaveData(stub_MMUFileID_t dataFileID);
MEMORY_STATUS memoryLoadData(stub_MMUFileID_t dataFileID);
MEMORY_STATUS memoryFree(void);
uint16_t memoryGetCodeWord(SR_t segment, PC_t offset);
uint64_t memoryGetData(SR_t segment, EA_t offset, size_t size);
void memorySetData(SR_t segment, EA_t offset, size_t size, uint64_t data);


#ifdef __cplusplus
}
#endif

#endif
