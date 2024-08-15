#ifndef LCD_H_INCLUDED
#define LCD_H_INCLUDED


#include "lcdsetup.h"


// draws a pixel at `(x, y)`, color `c`
// YOU need to implement this function
extern void setPix(int x, int y, int c);
// updates the display
// YOU need to implement this function
extern void updateDisp(void);

void renderBuffer(const uint8_t* buf);
void renderVRAM(void);

#endif
