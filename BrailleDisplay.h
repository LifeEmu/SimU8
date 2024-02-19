#ifndef BRAILLE_DISPLAY_H_INCLUDED
#define BRAILLE_DISPLAY_H_INCLUDED

#include <wchar.h>

#define BRAILLE_DISPLAY_WIDTH 96
#define BRAILLE_DISPLAY_HEIGHT 31

#define _BRAILLE_PER_LINE ((BRAILLE_DISPLAY_WIDTH+1)>>1)
#define _BRAILLE_PER_COL ((BRAILLE_DISPLAY_HEIGHT+3)>>2)


extern wchar_t *BScreen;


int8_t Braille_createDisplay(void);
int8_t Braille_destroyDisplay(void);
void Braille_clearDisplay(void);
void Braille_flushDisplay(void);
uint8_t Braille_getPix(uint8_t x, uint8_t y);
void Braille_setPix(uint8_t x, uint8_t y, uint8_t c);
void Braille_setDisplay(uint8_t *bytes);


#endif
