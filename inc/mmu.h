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
// the word fetched from code memory
extern uint16_t CodeWord;
// data fetched from data memory
extern Data_t DataRaw;
// Tracks how many ROM window access has happened
extern int ROMWinAccessCount;


MEMORY_STATUS memoryInit(char *CodeFileName, char *DataFileName);
MEMORY_STATUS memorySaveData(char *DataFileName);
MEMORY_STATUS memoryFree(void);
MEMORY_STATUS memoryGetCodeWord(SR_t segment, PC_t offset);
MEMORY_STATUS memoryGetData(SR_t segment, EA_t offset, size_t size);
MEMORY_STATUS memorySetData(SR_t segment, EA_t offset, size_t size, Data_t data);

#endif
