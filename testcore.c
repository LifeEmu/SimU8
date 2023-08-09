#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <conio.h>

#include "inc/mmu.h"
#include "inc/core.h"
#include "inc/lcd.h"

#define ROM_FILE_NAME "rom.bin"
#define DARK_PIXEL 'o'
#define LIGHT_PIXEL ' '

unsigned char* VBuf = NULL;


unsigned char* createVBuf(int x, int y) {
	return (unsigned char*)malloc(x * y);
}

void freeVBuf(unsigned char* buf) {
	free(buf);
}


void setPix(int x, int y, int c) {
	*(VBuf + y * LCD_WIDTH + x) = (c? DARK_PIXEL : LIGHT_PIXEL);
}

void updateDisp() {
	int y;
	char line[LCD_WIDTH + 1];
	line[LCD_WIDTH] = '\0';
	// status bar area
/*
 * 123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678
 * [S] [A]   M   STO  RCL    STAT  CMPLX  MAT  VCT   [D]  [R]  [G]    FIX  SCI   Math   v  ^   Disp
 */
	fputs(*(VBuf + 3) == DARK_PIXEL? "\n[S] " : "\n    ", stdout);
	fputs(*(VBuf + 5) == DARK_PIXEL? "[A]   " : "      ", stdout);
	fputs(*(VBuf + 8*1 + 3) == DARK_PIXEL? "M   " : "    ", stdout);
	fputs(*(VBuf + 8*1 + 6) == DARK_PIXEL? "STO  " : "     ", stdout);
	fputs(*(VBuf + 8*2 + 1) == DARK_PIXEL? "RCL    " : "       ", stdout);
	fputs(*(VBuf + 8*3 + 1) == DARK_PIXEL? "STAT  " : "      ", stdout);
	fputs(*(VBuf + 8*4 + 0) == DARK_PIXEL? "CMPLX  " : "       ", stdout);
	fputs(*(VBuf + 8*5 + 1) == DARK_PIXEL? "MAT  " : "     ", stdout);
	fputs(*(VBuf + 8*5 + 6) == DARK_PIXEL? "VCT   " : "      ", stdout);
	fputs(*(VBuf + 8*7 + 2) == DARK_PIXEL? "[D]  " : "     ", stdout);
	fputs(*(VBuf + 8*7 + 6) == DARK_PIXEL? "[R]  " : "     ", stdout);
	fputs(*(VBuf + 8*8 + 3) == DARK_PIXEL? "[G]    " : "       ", stdout);
	fputs(*(VBuf + 8*8 + 7) == DARK_PIXEL? "FIX  " : "     ", stdout);
	fputs(*(VBuf + 8*9 + 2) == DARK_PIXEL? "SCI   " : "      ", stdout);
	fputs(*(VBuf + 8*10 + 1) == DARK_PIXEL? "Math   " : "       ", stdout);
	fputs(*(VBuf + 8*10 + 4) == DARK_PIXEL? "v  " : "   ", stdout);
	fputs(*(VBuf + 8*11 + 0) == DARK_PIXEL? "^   " : "    ", stdout);
	fputs(*(VBuf + 8*11 + 3) == DARK_PIXEL? "Disp\n" : "    \n", stdout);

	// dot matrix area
	for( y = 1; y < LCD_HEIGHT; ++y ) {
		memcpy(line, VBuf + y * LCD_WIDTH, LCD_WIDTH);
		puts(line);
	}
}


int main(void) {
	int key, isSingleStep = 1, hasBreakpoint = 0, isCommand = 0;
	EA_t breakpoint;
//	uint16_t checksum = 0x0000, index, count;

	switch( memoryInit(ROM_FILE_NAME, NULL) ) {
		case MEMORY_ALLOCATION_FAILED:
			puts("Unable to allocate RAM for emulated memory.");
			return -1;

		case MEMORY_ROM_MISSING:
			printf("Cannot open ROM file \"%s\".\n", ROM_FILE_NAME);
			return -1;

		default:	// GCC told me to handle all the cases so here we go
			break;
	}


/*
	// calculate checksum
	// code segment 0
	count = 0; index = 0;
	do {
//		memoryGetData(8, index++, 1);
//		checksum -= DataRaw.byte;
		checksum -= *(uint8_t*)(CodeMemory + index++);
	} while( --count );
	printf("\nCount = %04X, index = %04X.\n", count, index);

	// code segment 1
	count = 0xfffc; index = 0;
	do {
//		memoryGetData(1, index++, 1);
//		checksum -= DataRaw.byte;
		checksum -= *(uint8_t*)(CodeMemory + 0x10000 + index++);
	} while( --count );

	printf("Count = %04X, index = %04X.\nChecksum = %04X.\n", count, index, checksum);
*/

/*
	// find out where the return value of segment 0 is bad
	count = 0; index = 0;
	do {
		memoryGetData(8, index, 1);
		if( DataRaw.byte != *(uint8_t*)(CodeMemory + index) ) {
			printf("Data from address %04Xh: memoryGetData: %02Xh | Direct access: %02Xh.\n", index, DataRaw.byte, *(uint8_t*)(CodeMemory + index));
			break;
		}
		++index;
	} while( --count );
*/


	if( (VBuf = createVBuf(LCD_WIDTH, LCD_HEIGHT)) == NULL ) {
		puts("Unable to allocate VBuf.");
		memoryFree();
		return -1;
	}


	printf("CodePointer = %p, DataPointer = %p\nWaiting for a key...\n", CodeMemory, DataMemory);

	coreReset();
	puts("input 'q' to exit.");

/*
	do {
		coreDispRegs();
		if( coreStep() == CORE_ILLEGAL_INSTRUCTION ) {
			printf("!!! Illegal Instruction! !!!\nCode word at last CSR:PC(%01X:%04X) = %04X\n", CSR, PC, CodeWord);
			coreDispRegs();
			break;
		}
	} while( getchar() != 'q' );
*/

	fflush(stdin);
	// main loop
	do {
		while( !_kbhit() ) {
			if( !isCommand ) {
				switch( coreStep() ) {
				case CORE_ILLEGAL_INSTRUCTION:
					printf("\n!!! Illegal Instruction !!!\n");
					coreDispRegs();
					goto exit;

				case CORE_READ_ONLY:
					puts("A write to read-only region has happened.");
					printf("CSR:PC = %01X:%04Xh.\n", CSR, PC);
					break;

				case CORE_UNIMPLEMENTED:
					puts("An unimplemented instruction has been skipped.");
					printf("Address = %01X%04Xh.\n", CSR, (PC - 2) & 0x0ffff);
					break;

				default:
					break;
				}

				// breakpoint
				if( hasBreakpoint && PC == breakpoint ) {
					isSingleStep = 1;
					coreDispRegs();
					printf("Breakpoint %04Xh has been hit!", PC);
					break;
				}

				if( isSingleStep ) {
					coreDispRegs();
					break;
				}
			}
		}
		key = tolower(_getch());	// get char and echo
		isCommand = 1;
		switch( key ) {
		case 'r':
			// show registers
			puts("Show registers (r)");
			coreDispRegs();
			break;

		case 'a':
			// show addresses
			puts("Show addresses (a)");
			printf("`CodeMemory` = %p\n`DataMemory` = %p.\n");
			break;

		case 's':
			// step
			puts("Single step mode (s)\nResume execution by typing 'c'.");
			isSingleStep = 1;
			break;

		case 'p':
			// continue
			puts("Execution resumed (p)\nType 's' to pause and step.");
			isSingleStep = 0;
			isCommand = 0;
			break;

		case 'b':
			// breakpoint
			puts("Set breakpoint (b)\nSingle step mode will be enabled if PC matches the breakpoint\nInput a hexadecimal number for breakpoint:");
			scanf("%x", &breakpoint);
			printf("Breakpoint set to %04Xh.\n", breakpoint);
			hasBreakpoint = 1;
			break;

		case 'n':
			// disable breakpoint
			puts("Disable breakpoint (n)\nBreakpoint has been disabled.");
			hasBreakpoint = 0;
			break;

		case 'c':
			// reset
			puts("Reset core (c)\nCore is reset.\nSingle step mode will be enabled.");
			coreReset();
			isSingleStep = 1;
			break;

		case 'd':
			// display
			puts("Display the LCD\n----------------");
			renderVRAM();
			puts("Display the buffer\n----------------");
			renderBuffer(DataMemory - ROM_WINDOW_SIZE + 0x87d0);
			puts("----------------");
			break;

		default:
			isCommand = 0;
			break;
		}
	} while( key != 'q' );

exit:
	coreReset();		// make sure core is not running..?
	memoryFree();
	freeVBuf(VBuf);
	puts("Successifully exited without crashing.");
	return 0;
}
