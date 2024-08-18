#include "interrupt.h"

#include "../core.h"


volatile Interrupt_t InterruptType;
volatile uint8_t InterruptIndex;

volatile bool IsInterruptChecked;
volatile bool IsInterruptAccepted;


inline void sendIRQ(Interrupt_t intr, uint8_t index) {
	IsInterruptChecked = false;
	InterruptType = intr;
	InterruptIndex = index;
}

int handleInterrupt(void) {
	IsInterruptChecked = true;
	switch( InterruptType ) {
		case INTERRUPT_NMI:
			coreDoNMI();
			InterruptType = INTERRUPT_NONE;
			break;

		case INTERRUPT_MI:
			if( (IsInterruptAccepted = coreDoMI(InterruptIndex)) ) {
				InterruptType = INTERRUPT_NONE;	// clean IRQ on accepting
				return InterruptIndex;
			}
			break;

		default:
			break;
	}
	return -1;
}
