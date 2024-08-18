#ifndef CORE_H_INCLUDED
#define CORE_H_INCLUDED


#include <stdbool.h>

#include "regtypes.h"
#include "coretypes.h"


// Defining this makes it emulate nX-U16/100
// differences: cycle count, SP word alignment, EPSW interacting, etc.
#define CORE_IS_U16

// macros for compatibility
#define DSR (CoreRegister.DSR)
#define CSR (CoreRegister.CSR)
#define LCSR (CoreRegister.LCSRs[0])
#define ECSR1 (CoreRegister.LCSRs[1])
#define ECSR2 (CoreRegister.LCSRs[2])
#define ECSR3 (CoreRegister.LCSRs[3])
#define PC (CoreRegister.PC)
#define LR (CoreRegister.LRs[0])
#define ELR1 (CoreRegister.LRs[1])
#define ELR2 (CoreRegister.LRs[2])
#define ELR3 (CoreRegister.LRs[3])
#define EA (CoreRegister.EA)
#define SP (CoreRegister.SP)
#define PSW (CoreRegister.PSW)
#define EPSW1 (CoreRegister.EPSWs[0])
#define EPSW2 (CoreRegister.EPSWs[1])
#define EPSW3 (CoreRegister.EPSWs[2])
#define GR (CoreRegister.GR)


// A struct containing core registers
extern CoreRegister_t CoreRegister;

// Records how many cycles the last instruction has taken
extern int CycleCount;


CORE_STATUS coreZero(void);
CORE_STATUS coreReset(void);
CORE_STATUS coreStep(void);

void coreDoNMI(void);
bool coreDoMI(uint8_t index);
void coreDoSWI(uint8_t index);


#endif
