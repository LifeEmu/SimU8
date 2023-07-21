#ifndef MMU_H_INCLUDED
#define MMU_H_INCLUDED


#include <stddef.h>
#include "regtypes.h"
#include "memtypes.h"
#include "memsetup.h"


MEMORY_STATUS memoryInit(char *CodeFileName, char *DataFileName);
MEMORY_STATUS memorySaveData(char *DataFileName);
MEMORY_STATUS memoryFree(void);
MEMORY_STATUS memoryGetCodeWord(SR_t segment, PC_t offset);
MEMORY_STATUS memoryGetData(SR_t segment, EA_t offset, size_t size);
MEMORY_STATUS memorySetData(SR_t segment, EA_t offset, size_t size, Data_t data);


#endif
