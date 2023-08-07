#include <stdint.h>


#include "../inc/mmu.h"
#include "../inc/lcd.h"


// renders a buffer
void renderBuffer(const uint8_t* buf) {
	int row, col, bit, byte;
	for( row = 0; row < LCD_HEIGHT; ++row ) {
		for( col = 0; col < LCD_WIDTH; col += 8 ) {
			byte = *(buf + ((LCD_WIDTH * row + col) >> 3));
			for( bit = 7; bit >= 0; --bit ) {
				// assume 1BPP
				setPix(col + bit, row, byte & 0x01);
				byte >>= 1;
			}
		}
	}
	updateDisp();
}


// renders current VRAM
// memory must be initialized
void renderVRAM(void) {
	int row, col, bit, byte;
	const uint8_t* base = (uint8_t*)(DataMemory - ROM_WINDOW_SIZE + VRAM_BASE);
	for( row = 0; row < LCD_HEIGHT; ++row ) {
		for( col = 0; col < LCD_WIDTH; col += 8 ) {
			byte = *(base + ((LCD_WIDTH * row + col) >> 3));
			for( bit = 7; bit >= 0; --bit ) {
				// assume 1BPP
				setPix(col + bit, row, byte & 0x01);
				byte >>= 1;
			}
		}
	}
	updateDisp();
}
