#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <conio.h>

#include "src/memmap.h"
#include "src/mmu.h"
#include "src/core.h"
#include "src/lcd.h"
#include "BrailleDisplay.h"

#define ROM_FILE_NAME "rom_991cnx_c_real.bin"
#define CYCLE_SKIP 51200	// defines how many cycles the core go before checking for keyboard


// dummy SFR implementation
uint8_t SFRHandler(uint32_t address, uint8_t data, bool isWrite) {
	uint8_t *p = (uint8_t *)DataMemory + address - ROM_WINDOW_SIZE;

	if( isWrite ) {
		*p = data;
		return 0;
	}
	else {
		return *p;
	}
}

void createVBuf(void) {
	Braille_createDisplay();
}

inline void freeVBuf(void) {
	Braille_destroyDisplay();
}


void setPix(int x, int y, int c) {
	Braille_setPix(x, y, c);
}

void updateDisp() {
	// status bar area
/*
 * 123456781234567812345678123456781234567812345678
 * S A M STO RCL STAT CMPLX MAT VCT D R G FIX SCI Math v ^ Disp
 
	fputs(*(VBuf + 3) == DARK_PIXEL? "\nS " : "\n  ", stdout);
	fputs(*(VBuf + 5) == DARK_PIXEL? "A " : "  ", stdout);
	fputs(*(VBuf + 8*1 + 3) == DARK_PIXEL? "M " : "  ", stdout);
	fputs(*(VBuf + 8*1 + 6) == DARK_PIXEL? "STO " : "    ", stdout);
	fputs(*(VBuf + 8*2 + 1) == DARK_PIXEL? "RCL " : "    ", stdout);
	fputs(*(VBuf + 8*3 + 1) == DARK_PIXEL? "STAT " : "     ", stdout);
	fputs(*(VBuf + 8*4 + 0) == DARK_PIXEL? "CMPLX " : "      ", stdout);
	fputs(*(VBuf + 8*5 + 1) == DARK_PIXEL? "MAT " : "    ", stdout);
	fputs(*(VBuf + 8*5 + 6) == DARK_PIXEL? "VCT " : "    ", stdout);
	fputs(*(VBuf + 8*7 + 2) == DARK_PIXEL? "D " : "  ", stdout);
	fputs(*(VBuf + 8*7 + 6) == DARK_PIXEL? "R " : "  ", stdout);
	fputs(*(VBuf + 8*8 + 3) == DARK_PIXEL? "G " : "  ", stdout);
	fputs(*(VBuf + 8*8 + 7) == DARK_PIXEL? "FIX " : "    ", stdout);
	fputs(*(VBuf + 8*9 + 2) == DARK_PIXEL? "SCI " : "    ", stdout);
	fputs(*(VBuf + 8*10 + 1) == DARK_PIXEL? "Math " : "     ", stdout);
	fputs(*(VBuf + 8*10 + 4) == DARK_PIXEL? "v " : "  ", stdout);
	fputs(*(VBuf + 8*11 + 0) == DARK_PIXEL? "^ " : "  ", stdout);
	fputs(*(VBuf + 8*11 + 3) == DARK_PIXEL? "Disp\n" : "    \n", stdout);
*/
	// dot matrix area
	Braille_flushDisplay();
}


void coreDispRegs(void) {
	int i, regValue;

	if( IsMemoryInited == false )
		return;

	puts("======== Register values ========\n General registers:");

	// display general registers
	for( i = 0; i < 16; ++i ) {
		regValue = GR.rs[i];
		printf("\tR%-2d = %02Xh (%3d)\n", i, regValue, regValue);
	}
	putchar('\n');
	// display Extended Registers
	for( i = 0; i < 16; i += 2 ) {
		printf("\tER%-2d = %04Xh\n", i, GR.ers[i >> 1]);
	}

	puts("\n Control registers:");

	printf("\tCSR:PC = %01X:%04Xh\n", CSR, PC);
	printf("\t\tCode words at CSR:PC: %04X", memoryGetCodeWord(CSR, PC));
	printf(" %04X\n", memoryGetCodeWord(CSR, (PC + 2) & 0xfffe));

	printf("\tSP = %04Xh\n\t\t%04Xh: ", SP, SP);
	for( i = 0; i < 8; ++i ) {
		printf("%02X ", memoryGetData(0, SP + i, 1));
	}
	printf("\n\t\t%04Xh: ", (SP + 8) & 0xffff);
	for( i = 8; i < 16; ++i ) {
		printf("%02X ", memoryGetData(0, SP + i, 1));
	}
	putchar('\n');
	putchar('\n');

	printf("\tDSR = %02Xh\n", DSR);
	printf("\tEA = %04Xh\n", EA);
	printf("\tPSW = %02Xh\n", PSW.raw);
	printf("\t\tC Z S V I H ELV\n\t\t%1d %1d %1d %1d %1d %1d  %1d\n", PSW.field.C, PSW.field.Z, PSW.field.S, PSW.field.OV, PSW.field.MIE, PSW.field.HC, PSW.field.ELevel);

	printf("\n\tLCSR:LR = %01X:%04Xh\n", LCSR, LR);
	printf("\tECSR1:ELR1 = %01X:%04Xh\n", ECSR1, ELR1);
	printf("\tECSR2:ELR2 = %01X:%04Xh\n", ECSR2, ELR2);
	printf("\tECSR3:ELR3 = %01X:%04Xh\n", ECSR3, ELR3);

	printf("\n\tEPSW1 = %02Xh\n", EPSW1.raw);
	printf("\tEPSW2 = %02Xh\n", EPSW2.raw);
	printf("\tEPSW3 = %02Xh\n", EPSW3.raw);

	puts("========       End       ========");
}


int main(void) {
	int isSingleStep = 1, hasBreakpoint = 0, isCommand = 0, line, offset;
	uint8_t tempByte;
	EA_t breakPC, jumpPC, dumpAR;
	SR_t breakCSR, jumpCSR, dumpDSR;
	char key, keyZero;
	char saveFileName[80];
	// `hexBytes` contains hexadecimal representation of bytes
#define DUMP_BYTE_PER_ROW 16
#define DUMP_ROWS 8
	char hexBytes[(2+1)*DUMP_BYTE_PER_ROW +1], charBytes[DUMP_BYTE_PER_ROW +1];
	hexBytes[(2+1)*DUMP_BYTE_PER_ROW] = '\0'; charBytes[DUMP_BYTE_PER_ROW] = '\0';

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

	createVBuf();

	printf("CodePointer = %p, DataPointer = %p\nWaiting for a key...\n", CodeMemory, DataMemory);

	coreReset();
	puts("input 'q' to exit.");

	fflush(stdin);
	unsigned int cycle = 0;
	// main loop
	do {
		while( (cycle < CYCLE_SKIP) || !_kbhit() ) {
			if( !isCommand ) {
				switch( coreStep() ) {
				case CORE_ILLEGAL_INSTRUCTION:
					coreDispRegs();
					isSingleStep = 1;
					printf("\n!!! Illegal Instruction !!!\n");
					printf("CSR:PC = %01X:%04Xh.\n", CSR, PC);
					puts("Single step mode is activated. Press 'c' to reset core.");
					break;

				case CORE_READ_ONLY:
					puts("\nA write to read-only region has happened.");
					printf("CSR:PC = %01X:%04Xh.\n", CSR, PC);
					break;

				case CORE_UNIMPLEMENTED:
					puts("\nAn unimplemented instruction has been skipped.");
					printf("Address = %01X%04Xh.\n", CSR, (PC - 2) & 0x0ffff);
					break;

				default:
					break;
				}
				cycle += CycleCount;

				// breakpoint
				if( hasBreakpoint && (CSR == breakCSR) && (PC == breakPC)) {
					isSingleStep = 1;
					coreDispRegs();
					printf("\nBreakpoint %01X:%04Xh has been hit!\n", CSR, PC);
					break;
				}

				if( isSingleStep ) {
					coreDispRegs();
					break;
				}
			}
			else {
				break;
			}
		}
		printf("Cycle = %d.\n", cycle);
		cycle = 0;
		key = tolower(_getch());	// get char and echo
		isCommand = 1;
		switch( key ) {
		case 'r':
			// show registers
			coreDispRegs();
			puts("\nShow registers (r)");
			break;

		case 'a':
			// show addresses
			puts("\nShow addresses (a)");
			printf("`CodeMemory` = %p\n`DataMemory` = %p.\n", CodeMemory, DataMemory);
			break;

		case 's':
			// step
			puts("\nSingle step mode (s)\nResume execution by pressing 'p'.");
			isSingleStep = 1;
			break;

		case 'p':
			// continue
			puts("\nExecution resumed (p)\nPress 's' to pause and enter single step mode.");
			isSingleStep = 0;
			isCommand = 0;
			break;

		case 'b':
			// breakpoint
			puts("\nSet breakpoint... (b)\nSingle step mode will be enabled if CSR:PC matches the breakpoint\nInput CSR:PC for breakpoint(DON'T include the colon):");
			scanf("%01x%04x", &breakCSR, &breakPC);
			breakCSR &= 0x0f;
			printf("Breakpoint set to %01X:%04Xh.\n", breakCSR, breakPC);
			hasBreakpoint = 1;
			break;

		case 'n':
			// disable breakpoint
			puts("\nDisable breakpoint (n)\nBreakpoint has been disabled.");
			hasBreakpoint = 0;
			break;

		case 'c':
			// reset
			puts("\nReset core (c)\nCore is reset.\nSingle step mode will be enabled.");
			coreReset();
			isSingleStep = 1;
			break;

		case 'd':
			// display
			puts("\nDisplay the LCD (d)\n----------------");
			renderVRAM();
			puts("Buffer 1\n----------------");
//			renderBuffer(DataMemory - ROM_WINDOW_SIZE + 0xe3d4);
			Braille_setDisplay(DataMemory - ROM_WINDOW_SIZE + 0xe3d4);
			Braille_flushDisplay();
			puts("Buffer 2\n----------------");
//			renderBuffer(DataMemory - ROM_WINDOW_SIZE + 0xddd4);
			Braille_setDisplay(DataMemory - ROM_WINDOW_SIZE + 0xddd4);
			Braille_flushDisplay();
			puts("----------------");
			break;

		case 'j':
			// jump
			puts("\nJump to... (j)\nInput a new value for CSR:PC (DON'T include the colon):");
			scanf("%01x%04x", &jumpCSR, &jumpPC);
			PC = jumpPC & 0xfffe; CSR = jumpCSR & 0x0f;
			printf("CSR:PC set to %01X:%04Xh.\n", CSR, PC);
			break;

		case 'm':
			// memory
			puts("\nShow memory... (m)\nInput an address for data memory (6 hexadecimal digits):");
			scanf("%02X%04X", &dumpDSR, &dumpAR);
			puts("======== Memory dump ========");
			for( line = 0; line < DUMP_ROWS; ++line ) {
				for( offset = 0; offset < DUMP_BYTE_PER_ROW; ++offset ) {
					tempByte = memoryGetData(dumpDSR, dumpAR, 1);
					sprintf(hexBytes + (1+2)*offset, "%02X ", tempByte);
					dumpAR = (dumpAR + 1) & 0xffff;
					charBytes[offset] = (tempByte >= 0x20 && tempByte <= 0x7f)? tempByte : '.';
				}
				printf("%02X%04X: %s|%s\n", dumpDSR, (dumpAR - DUMP_BYTE_PER_ROW) & 0xffff, hexBytes, charBytes);
			}
			puts("========     End     ========");
			break;

		case 'z':
			// zero RAM
			puts("\nZero RAM (z)\nDo you really want to clear RAM?(Y/n)");
			keyZero = tolower(_getche());
			putchar('\n');
			if( keyZero == 'y' ) {
				memset(DataMemory, 0, (size_t)0x10000 - ROM_WINDOW_SIZE);
				puts("RAM cleared!");
			}
			else {
				puts("Operation canceled.");
			}
			break;

		case 'w':
			puts("\nWrite data memory to file...(w)\nInput filename to save:");
			fflush(stdin);
			gets(saveFileName);	// I know it's unsafe blah blah
			if( memorySaveData(saveFileName) != MEMORY_OK )
				puts("Saving failed...");
			else
				puts("Saving complete!");
			break;

		case 'e':
			puts("\nRead data memory from file...(e)\nInput savestate file name:");
			fflush(stdin);
			gets(saveFileName);	// I know it's unsafe blah blah
			if( memoryLoadData(saveFileName) != MEMORY_OK )
				puts("Loading failed...");
			else
				puts("Loading complete!");
			break;

		default:
			isCommand = 0;
			break;
		}
	} while( key != 'q' );

	memoryFree();
	freeVBuf();
	puts("Successifully exited without crashing.");
	return 0;
}
