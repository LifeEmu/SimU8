#ifndef STANDBY_H_INCLUDED
#define STANDBY_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>


typedef enum {
	STANDBY_NONE = 0,
	STANDBY_HLT = 1,
	STANDBY_STP = 2
} StandbyState_t;


// Standby state
// For simplicity, non-zero means core is stopped
extern volatile StandbyState_t StandbyState;


uint8_t STPACP(uint8_t data, bool isWrite);
uint8_t SBYCON(uint8_t data, bool isWrite);

// sets `StandbyState` to `STANDBY_NONE`
void exitStandby(void);

#endif
