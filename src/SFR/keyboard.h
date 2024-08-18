#ifndef KEYBOARD_H_INCLUDED
#define KEYBOARD_H_INCLUDED


#include <stdint.h>


// maskable interrupt number for keyboard interrupt
#define KEYBOARD_INT_INDEX 0


// -------- core thread

// Initialize pointers for keyboard SFRs
// Call this after memory initialization!
// No further checking is done!
void initKeyboard(void);

// Check for keyboard interrupt, process it if conditions met
// This is called from core thread
void checkKeyboardInterrupt(void);

// Clean keyboard bit in IRQ0
void cleanKeyboardIRQ(void);

void coreUpdateKeyboard(void);

// -------- peripheral thread

// Please repeatedly call it on another thread or something.
void updateKeyboard(void);

// You need to implement this function
// Now it should be reenterable
// In: `maskedKO` - `KO` ANDed with `KOM`
// Out: `uint16_t KI` carrying information of the keyboard
extern uint16_t getKI(uint16_t maskedKO);

#endif
