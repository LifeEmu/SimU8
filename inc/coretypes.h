#ifndef CORETYPES_H_INCLUDED
#define CORETYPES_H_INCLUDED


#include "regtypes.h"


typedef enum {
	CORE_OK,
	CORE_READ_ONLY,
	CORE_UNIMPLEMENTED,
	CORE_ILLEGAL_INSTRUCTION,
	CORE_MEMORY_UNINITIALIZED
} CORE_STATUS;

typedef enum {
	DATA_ACCESS_PAGE0,
	DATA_ACCESS_DSR
} DATA_ACCESS_PAGE;

typedef struct {
	PC_t PC;
	SR_t CSR;
	PC_t LRs[4];	// LR, ELR1, ELR2, ELR3
	SR_t LCSRs[4];	// LCSR, ECSR1, ECSR2, ECSR3
	SR_t DSR;	// Actually an SFR, included for simplicity
	EA_t EA;
	EA_t SP;
	PSW_t PSW;
	PSW_t EPSWs[3];	// EPSW1, EPSW2, EPSW3
	GR_t GR;
} CoreRegister_t;

#endif
