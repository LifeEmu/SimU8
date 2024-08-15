#include "../../inc/mmu.h"
#include "../../inc/core.h"

#include "keyboard.h"
#include "../../inc/SFR/sfr.h"
#include "standby.h"
#include "interrupt.h"

#include <stdio.h>

#define getSFR(SFR) ((volatile uint8_t *)DataMemory - ROM_WINDOW_SIZE + SFR)


static volatile uint16_t *KI, *KIM, *KO, *KOM;
static volatile uint8_t *IRQ0, *IE0;


// -------- core thread

void initKeyboard(void) {
	KI = (volatile uint16_t *)getSFR(SFR_KI0);
	KO = (volatile uint16_t *)getSFR(SFR_KO0);
	KIM = (volatile uint16_t *)getSFR(SFR_KIM0);
	KOM = (volatile uint16_t *)getSFR(SFR_KOM0);
	IRQ0 = getSFR(SFR_IRQ0);
	IE0 = getSFR(SFR_IE0);
}

// Check for keyboard interrupt, process it if conditions met
// This is called from core thread
inline void checkKeyboardInterrupt(void) {
	if( (*IRQ0 & *IE0) & 0x02 ) {
		sendIRQ(INTERRUPT_MI, KEYBOARD_INT_INDEX);
//		puts("Keyboard interrupt pending!");
	}
}

// Clean keyboard bit in IRQ0
inline void cleanKeyboardIRQ(void) {
	*IRQ0 &= ~0x02;
}


void coreUpdateKeyboard(void) {
	*KI = getKI(*KO & ~*KOM);
	if( ~*KI & *KIM ) {
		*IRQ0 |= 0x02;
	}
}


// -------- peripheral thread

// Please repeatedly call it on another thread or something.
void updateKeyboard(void) {
	*KI = getKI(*KO & ~*KOM);
	if( ~*KI & *KIM ) {
		*IRQ0 |= 0x02;
		exitStandby();
	}
}

