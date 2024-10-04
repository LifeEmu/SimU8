#include <stdio.h>	// file I/O
#include <stdlib.h>	// memory allocation
#include <string.h>	// memory operation
#include <stdint.h>	// integer types

#include "mmustub.h"


stub_MMUStatus_t stub_mmuLoadCodeMemory(const stub_MMUInitStruct_t s, void *p) {
	FILE *f;
	if( p == NULL )
		return STUB_MMU_ERROR;

	if( (f = fopen(s.codeMemoryID, "rb")) == NULL) {
		return STUB_MMU_ERROR;
	}

	fread(p, sizeof(uint8_t), (size_t)s.codeMemorySize, f);
	fclose(f);

	return STUB_MMU_OK;
}


stub_MMUStatus_t stub_mmuLoadDataMemory(const stub_MMUInitStruct_t s, void *p) {
	FILE *f;
	if( p == NULL )
		return STUB_MMU_ERROR;

	// zero data memory if not found
	if( (f = fopen(s.dataMemoryID, "rb")) == NULL) {
		memset(p, 0, s.dataMemorySize);
		return STUB_MMU_OK;
	}

	fread(p, sizeof(uint8_t), (size_t)s.dataMemorySize, f);
	fclose(f);

	return STUB_MMU_OK;
}


stub_MMUStatus_t stub_mmuSaveDataMemory(const stub_MMUInitStruct_t s, void *p) {
	FILE *f;
	if( p == NULL )
		return STUB_MMU_ERROR;

	// zero data memory if not found
	if( (f = fopen(s.dataMemoryID, "wb")) == NULL) {
		return STUB_MMU_ERROR;
	}

	fwrite(p, sizeof(uint8_t), (size_t)s.dataMemorySize, f);
	fclose(f);

	return STUB_MMU_OK;
}


void* stub_mmuInitCodeMemory(const stub_MMUInitStruct_t s) {
	void *p = malloc((size_t)s.codeMemorySize);

	if( p == NULL )
		return NULL;

	if( stub_mmuLoadCodeMemory(s, p) != STUB_MMU_OK ) {
		free(p);	// here `p` is not NULL, so it's safe to ignore the warning
		return NULL;
	}

	return p;
}


void* stub_mmuInitDataMemory(const stub_MMUInitStruct_t s) {
	void *p = malloc((size_t)s.dataMemorySize);

	if( p == NULL )
		return p;

	if( stub_mmuLoadDataMemory(s, p) != STUB_MMU_OK ) {
		free(p);	// here `p` is not NULL, so it's safe to ignore the warning
		return NULL;
	}

	return p;
}


void stub_mmuFreeCodeMemory(void *p) {
	if( p != NULL )
		free(p);
}


inline void stub_mmuFreeDataMemory(void *p) {
	stub_mmuFreeCodeMemory(p);
}
