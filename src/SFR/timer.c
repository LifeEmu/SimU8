#include <stdint.h>

#include "../memmap.h"
#include "../mmu.h"
#include "../core.h"

#include "sfr.h"
#include "timer.h"
#include "standby.h"
#include "interrupt.h"

#include <stdio.h>

#define getSFR(SFR) ((volatile uint8_t *)DataMemory - ROM_WINDOW_SIZE + SFR)


static volatile uint16_t *TM0D, *TM0C;
static volatile uint8_t *TMSTR0;
static volatile uint8_t *IRQ0, *IE0;


// -------- core thread

void initTimer(void) {
	TM0D = (volatile uint16_t*)getSFR(SFR_TM0D);
	TM0C = (volatile uint16_t*)getSFR(SFR_TM0C);
	TMSTR0 = getSFR(SFR_TMSTR0);
	IRQ0 = getSFR(SFR_IRQ0);
	IE0 = getSFR(SFR_IE0);
}

inline void checkTimerInterrupt(void) {
	if( (*IRQ0 & *IE0) & 0x20 ) {
		sendIRQ(INTERRUPT_MI, TIMER_INT_INDEX);
	}
}

inline void cleanTimerIRQ(void) {
	*IRQ0 &= ~0x20;
}


// -------- peripheral thread

void updateTimer(void) {
	uint16_t counter = *TM0C;
//	printf("[timer] TM0C = %04X\n", counter);
	if( *TMSTR0 & 1 ) {
		counter += TIMER_STEP;
		if( counter >= *TM0D ) {
//			puts("[timer] Interrupt pending!");
			*TM0C = 0;	// reset counter
			*IRQ0 |= 0x20;	// set IRQ
			if( *IE0 & 0x20 ) {
				exitStandby();
			}
		}
		else {
			*TM0C = counter;
		}
	}
}
