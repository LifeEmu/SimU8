#include "../memmap.h"
#include "../mmu.h"
#include "../core.h"

#include "sfr.h"
#include "standby.h"
#include "timer.h"
#include "keyboard.h"

#include <stdio.h>

#define getSFR(SFR) ((volatile uint8_t *)DataMemory - ROM_WINDOW_SIZE + SFR)


static inline uint8_t _directRW(uint8_t *p, uint8_t data, bool isWrite) {
	if( isWrite ) {
		*p = data;
	}
	return *p;
}

uint8_t SFRHandler(uint32_t address, uint8_t data, bool isWrite) {
	uint8_t *p = (uint8_t *)DataMemory - ROM_WINDOW_SIZE + address;

	switch( address ) {
		case SFR_DSR:
			if( isWrite )
				DSR = data;
			*p = DSR;	// to synchronize DSR and memory
			return DSR;
			break;

		case SFR_STPACP:
			return STPACP(data, isWrite);
			break;

		case SFR_SBYCON:
			return SBYCON(data, isWrite);
			break;

		case SFR_IE0:
			return _directRW(p, data, isWrite);
			break;

		case SFR_IE1:
			return _directRW(p, data, isWrite);
			break;

		case SFR_IRQ0:
			return _directRW(p, data, isWrite);
			break;

		case SFR_IRQ1:
			return _directRW(p, data, isWrite);
			break;

		case SFR_TM0D:
			return _directRW(p, data, isWrite);
			break;

		case SFR_TM0C:
			if( isWrite )
				*p = 0;
			return *p;
			break;

		case SFR_TMSTR0:
			if( isWrite )
				*p = (data & 1);
			return 0;
			break;

		case SFR_KI0:
			return _directRW(p, data, isWrite);
			break;
		
		case SFR_KI1:
			return _directRW(p, data, isWrite);
			break;

		case SFR_KIM0:
			return _directRW(p, data, isWrite);
			break;

		case SFR_KIM1:
			return _directRW(p, data, isWrite);
			break;

		case SFR_KOM0:
			return _directRW(p, data, isWrite);
			break;

		case SFR_KOM1:
			return _directRW(p, data, isWrite);
			break;

		case SFR_KO0:
			if( isWrite ) {
				*p = data;
				coreUpdateKeyboard();
			}
			return *p;
			break;

		case SFR_KO1:
			if( isWrite ) {
				*p = data;
				coreUpdateKeyboard();
			}
			return *p;
			break;

		default:
			return _directRW(p, data, isWrite);
			break;
	}
}
