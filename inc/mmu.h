#ifndef MMU_H_INCLUDED
#define MMU_H_INCLUDED


#include <stddef.h>
#include <stdbool.h>
#include "regtypes.h"
#include "memtypes.h"
#include "memsetup.h"


extern void *CodeMemory;
extern void *DataMemory;
extern bool IsMemoryInited;
// Status of last memory function
extern MEMORY_STATUS MemoryStatus;
// Tracks how many ROM window access has happened
extern int ROMWinAccessCount;


MEMORY_STATUS memoryInit(char *CodeFileName, char *DataFileName);
MEMORY_STATUS memorySaveData(char *DataFileName);
MEMORY_STATUS memoryFree(void);
uint16_t memoryGetCodeWord(SR_t segment, PC_t offset);
uint64_t memoryGetData(SR_t segment, EA_t offset, size_t size);
void memorySetData(SR_t segment, EA_t offset, size_t size, uint64_t data);

#endif
