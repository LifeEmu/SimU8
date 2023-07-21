#ifndef MEMTYPES_H_INCLUDED
#define MEMTYPES_H_INCLUDED


#include <stdint.h>


typedef enum {
	MEMORY_OK,
	MEMORY_UNINITIALIZED,
	MEMORY_ALLOCATION_FAILED,
	MEMORY_ROM_MISSING,
	MEMORY_SAVING_FAILED,
	MEMORY_UNMAPPED,
	MEMORY_ROM_WINDOW,
	MEMORY_MIRROWED_BANK,
	MEMORY_UNALIGNED,
	MEMORY_READ_ONLY
} MEMORY_STATUS;
typedef union {
	uint64_t raw;
	uint64_t qword;
	uint32_t dword;
	uint16_t word;
	uint8_t byte;
} Data_t;

#endif
