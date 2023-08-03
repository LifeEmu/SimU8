#ifndef CORE_H_INCLUDED
#define CORE_H_INCLUDED


#include "regtypes.h"
#include "coretypes.h"


extern SR_t DSR, CSR, LCSR, ECSR1, ECSR2, ECSR3;
extern PC_t PC, LR, ELR1, ELR2, ELR3;
extern EA_t EA, SP;
extern PSW_t PSW, EPSW1, EPSW2, EPSW3;
extern GR_t GR;
// Records how many cycles the last instruction has taken
extern int CycleCount;


CORE_STATUS coreZero(void);
CORE_STATUS coreReset(void);
CORE_STATUS coreDispRegs(void);
CORE_STATUS coreStep(void);

#endif
