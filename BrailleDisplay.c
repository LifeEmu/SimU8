#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <wchar.h>

#include "BrailleDisplay.h"

wchar_t *BScreen = NULL;

// So, Unicode scalar(U+<something>) is not the same as Unicode encoding(actual byte sequence)
// See https://www.unicode.org/versions/Unicode15.1.0/ch03.pdf

// prints Unicode scalar using UTF-8 encoding
static void _putUnicodeScalar(uint32_t scalar) {
	uint8_t encoding[4] = {0, 0, 0, 0}, len;
	if( scalar < 0x0080 ) {
		// ASCII range
		encoding[0] = scalar & 0x7f;
		len = 1;
	}
	else if( scalar < 0x0800 ) {
		// 2-byte sequence
		encoding[0] = ((scalar >> 6) & 0x1f) | 0xc0;
		encoding[1] = (scalar & 0x3f)| 0x80;
		len = 2;
	}
	else if( scalar < 0x010000 ) {
		// 3-byte sequence
		encoding[0] = ((scalar >> 12) & 0x0f)| 0xe0;
		encoding[1] = ((scalar >> 6) & 0x3f) | 0x80;
		encoding[2] = (scalar & 0x3f)| 0x80;
		len = 3;
	}
	else if( scalar < 0x200000 ) {
		// 4-byte sequence
		encoding[0] = ((scalar >> 18) & 0x07)| 0xf0;
		encoding[1] = ((scalar >> 12) & 0x0f)| 0x80;
		encoding[2] = ((scalar >> 6) & 0x3f) | 0x80;
		encoding[3] = (scalar & 0x3f)| 0x80;
		len = 4;
	}
	fwrite(encoding, sizeof(uint8_t), len, stdout);
}


int8_t Braille_createDisplay(void) {
	if( (BScreen = malloc(sizeof(wchar_t) * _BRAILLE_PER_LINE * _BRAILLE_PER_COL)) == NULL ) {
		return -1;	// insufficient memory
	}
	Braille_clearDisplay();
	return 0;
}

int8_t Braille_destroyDisplay(void) {
	if( BScreen == NULL ) {
		return -1;	// can't free a NULL pointer
	}
	free(BScreen);
	return 0;
}

void Braille_clearDisplay(void) {
	wchar_t *cur = BScreen;
	int i = _BRAILLE_PER_LINE * _BRAILLE_PER_COL;
	for( ; i > 0; --i ) {
		*cur++ = 0x2800;
	}
//	memset(BScreen, 0x2800, _BRAILLE_PER_LINE * _BRAILLE_PER_COL * sizeof(wchar_t));	// set blank braille character
//	^ Yep, that doesn't work because `memset` only deals with bytes
}

void Braille_flushDisplay(void) {
	uint8_t row = 0, col;
	wchar_t *curRow = BScreen;
	for( ; row < _BRAILLE_PER_COL; ++row ) {
		for( col = 0; col < _BRAILLE_PER_LINE; ++col ) {
			_putUnicodeScalar(*(curRow + col));
		}
		putchar('\n');
		curRow += _BRAILLE_PER_LINE;
	}
}

uint8_t Braille_getPix(uint8_t x, uint8_t y) {
	uint8_t data = *(BScreen + (y >> 2) * _BRAILLE_PER_LINE + (x >> 1)) & 0xff;
	uint8_t mask;
	switch( y % 4 ) {
		case 0:
		case 1:
		case 2:
			mask = 1 << (3 * (x % 2) + (y % 4));
			break;
		case 3:
			mask = 0x40 << (x % 2);
			break;
	}
	return (data & mask)? 1 : 0;
}

void Braille_setPix(uint8_t x, uint8_t y, uint8_t c) {
	wchar_t *braille = (BScreen + (y >> 2) * _BRAILLE_PER_LINE + (x >> 1));
	uint8_t mask;
	switch( y % 4 ) {
		case 0:
		case 1:
		case 2:
			mask = 1 << (3 * (x % 2) + (y % 4));
			break;
		case 3:
			mask = 0x40 << (x % 2);
			break;
	}
	if( c != 0 ) {
		*braille |= mask;
	}
	else {
		*braille &= (mask ^ 0xffff);
	}
}

void Braille_setDisplay(uint8_t *bytes) {
	wchar_t brailleLine[_BRAILLE_PER_LINE];
	uint8_t line = 0, row, col, byte, offset = 0;
	int i;
	for( ; line < _BRAILLE_PER_COL; ++line ) {	// braille array line
//		memset(brailleLine, 0x2800, sizeof(wchar_t) * _BRAILLE_PER_LINE);
//		^ Mehhhh moment for `memset`, again
		for( i = 0; i < _BRAILLE_PER_LINE; ++i ) {
			brailleLine[i] = 0x2800;
		}
		for( row = 0; row < 3; ++row ) {	// first 3 rows in one braille line
			for( col = 0; col < _BRAILLE_PER_LINE; col += 4 ) {	// braille from left to right
				byte = *(bytes + offset++);
				if( offset > ((BRAILLE_DISPLAY_WIDTH >> 3) * BRAILLE_DISPLAY_HEIGHT) )
					goto update;
				// unrolled loop
				brailleLine[line * _BRAILLE_PER_LINE + col] |= (((byte & 0x80) >> 7) | ((byte & 0x40) >> 3)) << row;
				brailleLine[line * _BRAILLE_PER_LINE + col + 1] |= (((byte & 0x20) >> 5) | ((byte & 0x10) >> 1)) << row;
				brailleLine[line * _BRAILLE_PER_LINE + col + 2] |= (((byte & 0x08) >> 3) | ((byte & 0x04) << 1)) << row;
				brailleLine[line * _BRAILLE_PER_LINE + col + 3] |= (((byte & 0x02) >> 1) | ((byte & 0x01) << 3)) << row;
			}
		}
		// last row of each line of braille
		for( col = 0; col < _BRAILLE_PER_LINE; col += 4 ) {	// braille from left to right
			byte = *(bytes + offset++);
			if( offset > ((BRAILLE_DISPLAY_WIDTH >> 3) * BRAILLE_DISPLAY_HEIGHT) )
				goto update;
			brailleLine[line * _BRAILLE_PER_LINE + col] |= ((byte & 0x80) >> 1) | ((byte & 0x40) << 1);
			brailleLine[line * _BRAILLE_PER_LINE + col + 1] |= ((byte & 0x20) << 1) | ((byte & 0x10) << 3);
			brailleLine[line * _BRAILLE_PER_LINE + col + 2] |= ((byte & 0x20) << 3) | ((byte & 0x10) << 5);
			brailleLine[line * _BRAILLE_PER_LINE + col + 3] |= ((byte & 0x20) << 5) | ((byte & 0x10) << 7);
		}
update:
		memcpy(BScreen + _BRAILLE_PER_LINE * line, brailleLine, sizeof(wchar_t) * _BRAILLE_PER_LINE);
	}
}
