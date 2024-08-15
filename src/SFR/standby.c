#include "standby.h"
#include <stdio.h>

static uint8_t acceptor = 0;

volatile StandbyState_t StandbyState = STANDBY_NONE;


uint8_t STPACP(uint8_t data, bool isWrite) {
	if( isWrite ) {
		data &= 0xF0;
		if( (acceptor == 0) && (data == 0x50) ) {
			acceptor = 1;
		}
		else if( (acceptor == 1) && (data == 0xA0) ) {
			acceptor = 2;
		}
		else {
			acceptor = 0;
		}
	}
	return 0;
}

uint8_t SBYCON(uint8_t data, bool isWrite) {
	if( isWrite ) {
		if( data & 1 ) {
			StandbyState = STANDBY_HLT;
			puts("[standby] entered HALT mode");
		}
		if( (acceptor == 2) && (data & 2) ) {
			StandbyState = STANDBY_STP;
			puts("[standby] entered STOP mode");
			acceptor = 0;
		}
	}
	return 0;
}

inline void exitStandby(void) {
	StandbyState = STANDBY_NONE;
//	puts("[standby] woken from sleep");
}
