#ifndef SFR_H_DEFINED
#define SFR_H_DEFINED


#define SFR_DSR		0xf000
#define SFR_STPACP	0xf008
#define SFR_SBYCON	0xf009
#define SFR_IE0		0xf010
#define SFR_IE1		0xf011
#define SFR_IRQ0	0xf014
#define SFR_IRQ1	0xf015
#define SFR_TM0D	0xf020	// timer interval
#define SFR_TM0C	0xf022	// timer counter
#define SFR_TMSTR0	0xf025	// timer start
#define SFR_KI0		0xf040	// KI lower
#define SFR_KI1		0xf041	// KI higher
#define SFR_KIM0	0xf042	// KI mask lower
#define SFR_KIM1	0xf043	// KI mask higher
#define SFR_KOM0	0xf044	// KO mask lower
#define SFR_KOM1	0xf045	// KO mask higher
#define SFR_KO0		0xf046	// KO lower
#define SFR_KO1		0xf047	// KO higher

#endif
