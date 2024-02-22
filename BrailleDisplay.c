#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <wchar.h>

#include "BrailleDisplay.h"

uint8_t *BScreen = NULL;	// stores lower byte of brailles' scalar

// So, Unicode scalar(U+<something>) is not the same as Unicode encoding(actual byte sequence)
// See https://www.unicode.org/versions/Unicode15.1.0/ch03.pdf

/*
 * Braille characters in Unicode is ordered as follows:
 *	0 3	each dot has different weight in binary data
 *	1 4	starting from U+2800, all the way up to U+28FF
 *	2 5
 *	6 7
 * UTF-8 encodes that range of scalars as 3-byte sequences:
 * 	zzzz_yyyy, yyxx_xxxx  -->  1110_zzzz, 10yy_yyyy, 10xx_xxxx
 * 	, in which `zzzz_yyyy` would always be 0x28, so it's safe to output a 0xE2 first
 * The second byte could be 0xA0 - 0xA3, so we can do `0xA0 | (byte >> 6)`
 */


inline static void _putBraille(uint8_t scalar) {
	putchar(0xe2);
	putchar(0xa0 | (scalar >> 6));
	putchar(0x80 | (scalar & 0x3f));
}


/*
 * Here, I plan to order the bits from initial buffer as follows:
 *	A  -->  AH AL ah al
 *	B  -->  BH BL bh bl
 *	C  -->  CH CL ch cl
 *	D  -->  DH DL dh dl
 * Actually, I want to just read the buffer line-by-line, so here is the tiny LUT:
 *	use each row as index (0-3)
 *	for upper 3 rows, it's {0, 8, 1, 9} << row
 *	for the last row, it's {0, 0x80, 0x40, 0xc0}
 */


// prints Unicode scalar using UTF-8 encoding
/*
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
*/

int Braille_createDisplay(void) {
	if( (BScreen = malloc(_BRAILLE_PER_LINE * _BRAILLE_PER_COL)) == NULL ) {
		return -1;	// insufficient memory
	}
	Braille_clearDisplay();
	return 0;
}

int Braille_destroyDisplay(void) {
	if( BScreen == NULL ) {
		return -1;	// can't free a NULL pointer
	}
	free(BScreen);
	return 0;
}

void Braille_clearDisplay(void) {
	memset(BScreen, 0, _BRAILLE_PER_LINE * _BRAILLE_PER_COL);	// set blank braille character
}

void Braille_flushDisplay(void) {
	uint8_t row = 0, col;
	uint8_t *curRow = BScreen;
	for( ; row < _BRAILLE_PER_COL; ++row ) {
		for( col = 0; col < _BRAILLE_PER_LINE; ++col ) {
			_putBraille(*curRow++);
		}
		putchar('\n');
	}
}

uint8_t Braille_getPix(uint8_t x, uint8_t y) {
	uint8_t data = *(BScreen + (y >> 2) * _BRAILLE_PER_LINE + (x >> 1));
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
	uint8_t *braille = (BScreen + (y >> 2) * _BRAILLE_PER_LINE + (x >> 1));
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
		*braille &= (mask ^ 0xff);
	}
}

void Braille_setDisplay(const uint8_t *bytes) {
	const uint8_t tableU[4] = {0, 8, 1, 9}, tableD[4] = {0, 0x80, 0x40, 0xc0};
	uint8_t brailleLine[_BRAILLE_PER_LINE], byte;
	int offset = 0, line = 0, row, col;
	for( ; line < _BRAILLE_PER_COL; ++line ) {	// braille array line
		memset(brailleLine, 0, _BRAILLE_PER_LINE);
		for( row = 0; row < 3; ++row ) {	// first 3 rows in one braille line
			for( col = 0; col < _BRAILLE_PER_LINE - 3; col += 4 ) {	// braille from left to right
				byte = *(bytes + offset++);
				if( offset > ((BRAILLE_DISPLAY_WIDTH >> 3) * BRAILLE_DISPLAY_HEIGHT) )
					goto update;
				// unrolled loop
				brailleLine[col] |= tableU[byte >> 6] << row;
				brailleLine[col + 1] |= tableU[(byte >> 4) & 3] << row;
				brailleLine[col + 2] |= tableU[(byte >> 2) & 3] << row;
				brailleLine[col + 3] |= tableU[byte & 3] << row;
			}
		}
		// last row of each line of braille
		for( col = 0; col < _BRAILLE_PER_LINE - 3; col += 4 ) {	// braille from left to right
			byte = *(bytes + offset++);
			if( offset > ((BRAILLE_DISPLAY_WIDTH >> 3) * BRAILLE_DISPLAY_HEIGHT) )
				goto update;
			brailleLine[col] |= tableD[byte >> 6];
			brailleLine[col + 1] |= tableD[(byte >> 4) & 3];
			brailleLine[col + 2] |= tableD[(byte >> 2) & 3];
			brailleLine[col + 3] |= tableD[byte & 3];
		}
update:
		memcpy(BScreen + _BRAILLE_PER_LINE * line, brailleLine, sizeof(uint8_t) * _BRAILLE_PER_LINE);
	}
}
