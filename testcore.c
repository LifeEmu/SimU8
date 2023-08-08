#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <conio.h>

#include "inc/mmu.h"
#include "inc/core.h"
#include "inc/lcd.h"

#define ROM_FILE_NAME "rom.bin"


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
	// status bar area
/*
 * 123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678
 * [S] [A]   M   STO  RCL    STAT  CMPLX  MAT  VCT   [D]  [R]  [G]    FIX  SCI   Math   v  ^   Disp
 */
	fputs((*VBuf & 0x10)? "\n[S] " : "\n    ", stdout);
	fputs((*VBuf & 0x04)? "[A]   " : "      ", stdout);
	fputs((*(VBuf + 1) & 0x10)? "M   " : "    ", stdout);
	fputs((*(VBuf + 1) & 0x02)? "STO  " : "     ", stdout);
	fputs((*(VBuf + 2) & 0x40)? "RCL    " : "       ", stdout);
	fputs((*(VBuf + 3) & 0x40)? "STAT  " : "      ", stdout);
	fputs((*(VBuf + 4) & 0x80)? "CMPLX  " : "       ", stdout);
	fputs((*(VBuf + 5) & 0x40)? "MAT  " : "     ", stdout);
	fputs((*(VBuf + 5) & 0x01)? "VCT   " : "      ", stdout);
	fputs((*(VBuf + 7) & 0x20)? "[D]  " : "     ", stdout);
	fputs((*(VBuf + 7) & 0x02)? "[R]  " : "     ", stdout);
	fputs((*(VBuf + 8) & 0x10)? "[G]    " : "       ", stdout);
	fputs((*(VBuf + 8) & 0x01)? "FIX  " : "     ", stdout);
	fputs((*(VBuf + 9) & 0x20)? "SCI   " : "      ", stdout);
	fputs((*(VBuf + 10) & 0x40)? "Math   " : "       ", stdout);
	fputs((*(VBuf + 10) & 0x08)? "v  " : "   ", stdout);
	fputs((*(VBuf + 11) & 0x10)? "^   " : "    ", stdout);
	fputs((*(VBuf + 11) & 0x10)? "Disp\n" : "    \n", stdout);

	// dot matrix area
	for( y = 1; y < LCD_HEIGHT; ++y ) {
		for( x = 0; x < LCD_WIDTH; ++x ) {
			putchar((*(VBuf + y * LCD_WIDTH + x) == 1)? 'o' : ' ');
		}
		putchar('\n');
	}
}


int main(void) {
	int key, isSingleStep = 1, hasBreakpoint = 0;
	EA_t breakpoint;

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
				printf("Address = %01X%04Xh.\n", CSR, PC - 2);
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
		key = tolower(_getch());	// get char and echo
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
			puts("----------------");
			break;

		default:
			break;
		}
	} while( key != 'q' );

exit:
	memoryFree();
	freeVBuf(VBuf);
	return 0;
}
