#ifndef INTERRUPT_H_INCLUDED
#define INTERRUPT_H_INCLUDED


#include <stdint.h>
#include <stdbool.h>


typedef enum {
	INTERRUPT_NONE = 0,
	INTERRUPT_NMI,
	INTERRUPT_MI
} Interrupt_t;


extern volatile Interrupt_t InterruptType;
extern volatile uint8_t InterruptIndex;

extern volatile bool IsInterruptChecked;
extern volatile bool IsInterruptAccepted;


// Call this function to send an interrupt request
void sendIRQ(Interrupt_t intr, uint8_t index);

// You need to run this function on the same thread the core runs on
// The more frequently it runs, the faster the core responds to external interrupts
//   - but that slows down the core if you do that too excessively
// It returns the index it handled, or `-1` if none/NMI
int handleInterrupt(void);

#endif
