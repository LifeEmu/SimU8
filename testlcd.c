#include <stdio.h>
#include <stdlib.h>

#include "inc/mmu.h"
#include "inc/core.h"
#include "inc/lcd.h"


unsigned char* VBuf = NULL;


unsigned char* createVBuf(int x, int y) {
	return (unsigned char*)malloc(x * y / 8);
}

void freeVBuf(unsigned char* buf) {
	free(buf);
}


void setPix(int x, int y, int c) {
	*(VBuf + y * LCD_WIDTH + x) = (c? 1 : 0);
}

void updateDisp() {
	int x, y;
	for( y = 0; y < LCD_HEIGHT; ++y ) {
		for( x = 0; x < LCD_WIDTH; ++x ) {
			putchar((*(VBuf + y * LCD_WIDTH + x) == 1)? 'o' : ' ');
		}
		putchar('\n');
	}
}


int main() {
	int i = 0;

	if( memoryInit("rom.bin", NULL) != MEMORY_OK ) {
		puts("Unable to initialize memory.");
		return -1;
	}

	if( (VBuf = createVBuf(LCD_WIDTH, LCD_HEIGHT)) == NULL ) {
		puts("Unable to allocate VBuf.");
		memoryFree();
		return -1;
	}

	puts("Input 'q' to exit.");

	for(i = 0x0000; i < 0x20000; i += (LCD_WIDTH / 8 * LCD_HEIGHT)) {
		printf("======== Displaying %05Xh ========\n", i);
		renderBuffer(CodeMemory + i);
		if( getchar() == 'q' )
			break;
	}

	memoryFree();
	freeVBuf(VBuf);

	return 0;
}

