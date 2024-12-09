#ifndef TIMER_H_INCLUDED
#define TIMER_H_INCLUDED


// maskable interrupt number for timer interrupt
#define TIMER_INT_INDEX 4

// timer counter increment step
// will be added to timer counter when `updateTimer` is called
#define TIMER_STEP 16	// Assume 1000+ updates per second


// -------- core thread

// Initialize pointers to timer SFRs.
// Call it after memory is initialized!
// No further checking is done!
void initTimer(void);

// Check for timer interrupt, process it if conditions met
// This is called from core thread
void checkTimerInterrupt(void);

// Clean timer bit in IRQ0
void cleanTimerIRQ(void);

// -------- peripheral thread

// Due to the fact that timer runs asynchronously against the core,
//   you probably have to update it periodically.
// Please repeatedly call it on another thread or something.
void updateTimer(void);

#endif
