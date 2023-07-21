#include "../inc/core.h"


#include "mmu.c"


#define GET_DATA_SEG ((NextAccess == DATA_ACCESS_DSR)? DSR : (SR_t)0)
#define IS_ZERO(val) ((val)? 0 : 1)
#define SIGN8(val) (((val) >> 7) & 1)
#define SIGN16(val) (((val) >> 15) & 1)
#define SIGN32(val) (((val) >> 31) & 1)
#define SIGN64(val) (((val) >> 63) & 1)


SR_t DSR, CSR, LCSR, ECSR1, ECSR2, ECSR3;
PC_t PC, LR, ELR1, ELR2, ELR3;
EA_t EA, SP;
PSW_t PSW, EPSW1, EPSW2, EPSW3;
GR_t GR;


// Tracks how many steps the processor should ignore the interrupt
static int IntMaskCycle = 0;
// Tracks which segment the next data access would be accessing
static DATA_ACCESS_PAGE NextAccess = DATA_ACCESS_PAGE0;
// Tracks if last instruction should cause a wait cycle due to bus conflict
static int EAIncDelay = 0;


// Records how many cycles the last instruction has taken
int CycleCount;


// Zeros all registers
CORE_STATUS coreZero(void) {
	DSR = 0;
	CSR = 0;
	LCSR = 0;
	ECSR1 = 0;
	ECSR2 = 0;
	ECSR3 = 0;
	PC = 0;
	LR = 0;
	ELR1 = 0;
	ELR2 = 0;
	ELR3 = 0;
	EA = 0;
	SP = 0;
	PSW.raw = 0;
	EPSW1.raw = 0;
	EPSW2.raw = 0;
	EPSW3.raw = 0;
	GR.qrs[0] = 0;
	GR.qrs[1] = 0;

	return CORE_OK;
}


// resets core
CORE_STATUS coreReset(void) {
	if( IsMemoryInited == false )
		return CORE_MEMORY_UNINITIALIZED;

	// reset registers
	PSW.raw = 0;
	CSR = 0;
	DSR = 0;

	// initialize SP
	memoryGetCodeWord((SR_t)0, (PC_t)0x0000);
	SP = CodeWord;

	// initialize PC
	memoryGetCodeWord((SR_t)0, (PC_t)0x0002);
	PC = CodeWord;

	// reset other core states
	IntMaskCycle = 0;
	NextAccess = DATA_ACCESS_PAGE0;
	CycleCount = 0;

	return CORE_OK;
}


CORE_STATUS coreDispRegs(void) {
	int i, regValue;

	if( IsMemoryInited == false )
		return CORE_MEMORY_UNINITIALIZED;

	puts("======== Register values ========\n General registers:");

	// display general registers
	for( i = 0; i < 16; ++i ) {
		regValue = GR.rs[i];
		printf("\tR%-2d = %02Xh (%3d)\n", i, regValue, regValue);
	}
	putchar('\n');
	// display Extended Registers
	for( i = 0; i < 16; i += 2 ) {
		printf("\tER%-2d = %04Xh\n", i, GR.ers[i]);
	}

	puts("\n Control registers:");

	printf("\tCSR:PC = %01X:%04X\n", CSR, PC);
	printf("\tSP = %04X\n", SP);
	printf("\tDSR = %02X\n", DSR);
	printf("\tEA = %04X\n", EA);
	printf("\tPSW = %02X\n", PSW.raw);
	printf("\t\tC Z S V I H MIE\n\t\t%1d %1d %1d %1d %1d %1d  %1d\n", PSW.C, PSW.Z, PSW.S, PSW.OV, PSW.MIE, PSW.HC, PSW.ELevel);

	printf("\n\tLCSR:LR = %01X:%04X\n", LCSR, LR);
	printf("\tECSR1:ELR1 = %01X:%04X\n", ECSR1, ELR1);
	printf("\tECSR1:ELR1 = %01X:%04X\n", ECSR2, ELR2);
	printf("\tECSR1:ELR1 = %01X:%04X\n", ECSR3, ELR3);

	printf("\n\tEPSW1 = %02X\n", EPSW1.raw);
	printf("\tEPSW2 = %02X\n", EPSW1.raw);
	printf("\tEPSW3 = %02X\n", EPSW1.raw);

	puts("========       End       ========");

	return CORE_OK;
}


CORE_STATUS coreStep(void) {
	CORE_STATUS retVal = CORE_OK;
	CycleCount = 0;
	uint8_t decodeIndex;

	uint64_t dest, src;
	uint64_t temp;

	if( IsMemoryInited == false ) {
		retVal = CORE_MEMORY_UNINITIALIZED;
		goto exit;
	}

	// fetch instruction
	memoryGetCodeWord(CSR, PC);

	// increment PC
	PC = (PC & 0xfffe) + 2;

	decodeIndex = ((CodeWord >> 8) & 0xf0) | (CodeWord & 0x0f);
	switch( decodeIndex ) {
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
		case 0x08:
		case 0x09:
		case 0x0a:
		case 0x0b:
		case 0x0c:
		case 0x0d:
		case 0x0e:
		case 0x0f:
			// MOV Rn, #imm8
			CycleCount = 1;
			GR.rs[(CodeWord >> 8) & 0x0f] = CodeWord & 0xff;

			PSW.Z = IS_ZERO(CodeWord & 0xff);
			PSW.S = SIGN8(CodeWord);
			break;

		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
		case 0x14:
		case 0x15:
		case 0x16:
		case 0x17:
		case 0x18:
		case 0x19:
		case 0x1a:
		case 0x1b:
		case 0x1c:
		case 0x1d:
		case 0x1e:
		case 0x1f:
			// ADD Rn, #imm8
			CycleCount = 1;
			dest = GR.rs[(CodeWord >> 8) & 0x0f];
			src = CodeWord & 0xff;

			temp = dest + src;

			PSW.Z = IS_ZERO(temp);
			PSW.C = (temp & 0x100)? 1 : 0;
			PSW.S = SIGN8(temp);
			// reference: Z80 user manual
			PSW.OV = (((dest & 0x7f) + (src & 0x7f)) >> 7) ^ PSW.C;
			PSW.HC = (((dest & 0x0f) + (src & 0x0f)) & 0x10)? 1 : 0;

			GR.rs[(CodeWord >> 8) & 0x0f] = (temp & 0xff);
			break;

		case 0x20:
		case 0x21:
		case 0x22:
		case 0x23:
		case 0x24:
		case 0x25:
		case 0x26:
		case 0x27:
		case 0x28:
		case 0x29:
		case 0x2a:
		case 0x2b:
		case 0x2c:
		case 0x2d:
		case 0x2e:
		case 0x2f:
			// AND Rn, #imm8
			CycleCount = 1;
			temp = GR.rs[(CodeWord >> 8) & 0x0f] & (CodeWord & 0xff);

			PSW.Z = IS_ZERO(temp);
			PSW.S = SIGN8(temp);

			GR.rs[(CodeWord >> 8) & 0x0f] = (temp & 0xff);
			break;

		case 0x30:
		case 0x31:
		case 0x32:
		case 0x33:
		case 0x34:
		case 0x35:
		case 0x36:
		case 0x37:
		case 0x38:
		case 0x39:
		case 0x3a:
		case 0x3b:
		case 0x3c:
		case 0x3d:
		case 0x3e:
		case 0x3f:
			// OR Rn, #imm8
			CycleCount = 1;
			temp = GR.rs[(CodeWord >> 8) & 0x0f] | (CodeWord & 0xff);

			PSW.Z = IS_ZERO(temp);
			PSW.S = SIGN8(temp);

			GR.rs[(CodeWord >> 8) & 0x0f] = (temp & 0xff);
			break;

		case 0x40:
		case 0x41:
		case 0x42:
		case 0x43:
		case 0x44:
		case 0x45:
		case 0x46:
		case 0x47:
		case 0x48:
		case 0x49:
		case 0x4a:
		case 0x4b:
		case 0x4c:
		case 0x4d:
		case 0x4e:
		case 0x4f:
			// XOR Rn, #imm8
			CycleCount = 1;
			temp = GR.rs[(CodeWord >> 8) & 0x0f] ^ (CodeWord & 0xff);

			PSW.Z = IS_ZERO(temp);
			PSW.S = SIGN8(temp);

			GR.rs[(CodeWord >> 8) & 0x0f] = (temp & 0xff);
			break;

		case 0x50:
		case 0x51:
		case 0x52:
		case 0x53:
		case 0x54:
		case 0x55:
		case 0x56:
		case 0x57:
		case 0x58:
		case 0x59:
		case 0x5a:
		case 0x5b:
		case 0x5c:
		case 0x5d:
		case 0x5e:
		case 0x5f:
			// CMPC Rn, #imm8
			CycleCount = 1;
			dest = GR.rs[(CodeWord >> 8) & 0x0f];
			src = CodeWord & 0xff;

			temp = dest - src - PSW.C;

			// reference: Z80 user manual
			PSW.OV = (((dest & 0x7f) - (src & 0x7f) - PSW.C) >> 7) ^ ((temp & 0x100)? 1 : 0);
			PSW.HC = (((dest & 0x0f) - (src & 0x0f) - PSW.C) & 0x10)? 1 : 0;

			PSW.Z = IS_ZERO(temp);
			PSW.C = (temp & 0x100)? 1 : 0;
			PSW.S = SIGN8(temp);

			break;

		case 0x60:
		case 0x61:
		case 0x62:
		case 0x63:
		case 0x64:
		case 0x65:
		case 0x66:
		case 0x67:
		case 0x68:
		case 0x69:
		case 0x6a:
		case 0x6b:
		case 0x6c:
		case 0x6d:
		case 0x6e:
		case 0x6f:
			// ADDC Rn, #imm8
			CycleCount = 1;
			dest = GR.rs[(CodeWord >> 8) & 0x0f];
			src = CodeWord & 0xff;

			temp = dest + src + PSW.C;

			// reference: Z80 user manual
			PSW.OV = (((dest & 0x7f) + (src & 0x7f) + PSW.C) >> 7) ^ ((temp & 0x100)? 1 : 0);
			PSW.HC = (((dest & 0x0f) + (src & 0x0f) + PSW.C) & 0x10)? 1 : 0;

			PSW.Z = IS_ZERO(temp);
			PSW.C = (temp & 0x100)? 1 : 0;
			PSW.S = SIGN8(temp);

			GR.rs[(CodeWord >> 8) & 0x0f] = (temp & 0xff);
			break;

		case 0x70:
		case 0x71:
		case 0x72:
		case 0x73:
		case 0x74:
		case 0x75:
		case 0x76:
		case 0x77:
		case 0x78:
		case 0x79:
		case 0x7a:
		case 0x7b:
		case 0x7c:
		case 0x7d:
		case 0x7e:
		case 0x7f:
			// CMP Rn, #imm8
			CycleCount = 1;
			dest = GR.rs[(CodeWord >> 8) & 0x0f];
			src = CodeWord & 0xff;

			temp = dest - src;

			PSW.Z = IS_ZERO(temp);
			PSW.C = (temp & 0x100)? 1 : 0;
			PSW.S = SIGN8(temp);
			// reference: Z80 user manual
			PSW.OV = (((dest & 0x7f) - (src & 0x7f)) >> 7) ^ PSW.C;
			PSW.HC = (((dest & 0x0f) - (src & 0x0f)) & 0x10)? 1 : 0;

			break;

		case 0x80:
			// MOV Rn, Rm
			CycleCount = 1;
			src = GR.rs[(CodeWord >> 4) & 0x0f];

			PSW.Z = IS_ZERO(src);
			PSW.S = SIGN8(src);

			GR.rs[(CodeWord >> 8) & 0x0f] = src;
			break;

		case 0x81:
			// ADD Rn, Rm
			CycleCount = 1;
			dest = GR.rs[(CodeWord >> 8) & 0x0f];
			src = GR.rs[(CodeWord >> 4) & 0x0f];

			temp = dest + src;

			PSW.Z = IS_ZERO(temp);
			PSW.C = (temp & 0x100)? 1 : 0;
			PSW.S = SIGN8(temp);
			// reference: Z80 user manual
			PSW.OV = (((dest & 0x7f) + (src & 0x7f)) >> 7) ^ PSW.C;
			PSW.HC = (((dest & 0x0f) + (src & 0x0f)) & 0x10)? 1 : 0;

			GR.rs[(CodeWord >> 8) & 0x0f] = (temp & 0xff);
			break;

		case 0x82:
			// AND Rn, Rm
			CycleCount = 1;
			temp = GR.rs[(CodeWord >> 8) & 0x0f] & GR.rs[(CodeWord >> 4) & 0x0f];

			PSW.Z = IS_ZERO(temp);
			PSW.S = SIGN8(temp);
			break;

		case 0x83:
			// OR Rn, Rm
			CycleCount = 1;
			temp = GR.rs[(CodeWord >> 8) & 0x0f] | GR.rs[(CodeWord >> 8) & 0xff];

			PSW.Z = IS_ZERO(temp);
			PSW.S = SIGN8(temp);

			GR.rs[(CodeWord >> 8) & 0x0f] = (temp & 0xff);
			break;

		case 0x84:
			// XOR Rn, Rm
			CycleCount = 1;
			temp = GR.rs[(CodeWord >> 8) & 0x0f] ^ GR.rs[(CodeWord >> 8) & 0xff];

			PSW.Z = IS_ZERO(temp);
			PSW.S = SIGN8(temp);

			GR.rs[(CodeWord >> 8) & 0x0f] = (temp & 0xff);
			break;

		case 0x85:
			// CMPC Rn, Rm
			CycleCount = 1;
			dest = GR.rs[(CodeWord >> 8) & 0x0f];
			src = GR.rs[(CodeWord >> 4) & 0x0f];

			temp = dest - src - PSW.C;

			// reference: Z80 user manual
			PSW.OV = (((dest & 0x7f) - (src & 0x7f) - PSW.C) >> 7) ^ ((temp & 0x100)? 1 : 0);
			PSW.HC = (((dest & 0x0f) - (src & 0x0f) - PSW.C) & 0x10)? 1 : 0;

			PSW.Z = IS_ZERO(temp);
			PSW.C = (temp & 0x100)? 1 : 0;
			PSW.S = SIGN8(temp);

			break;

		case 0x86:
			// ADDC Rn, Rm
			CycleCount = 1;
			dest = GR.rs[(CodeWord >> 8) & 0x0f];
			src = GR.rs[(CodeWord >> 4) & 0x0f];

			temp = dest + src + PSW.C;

			// reference: Z80 user manual
			PSW.OV = (((dest & 0x7f) + (src & 0x7f) + PSW.C) >> 7) ^ ((temp & 0x100)? 1 : 0);
			PSW.HC = (((dest & 0x0f) + (src & 0x0f) + PSW.C) & 0x10)? 1 : 0;

			PSW.Z = IS_ZERO(temp);
			PSW.C = (temp & 0x100)? 1 : 0;
			PSW.S = SIGN8(temp);

			GR.rs[(CodeWord >> 8) & 0x0f] = (temp & 0xff);
			break;

		case 0x87:
			// CMP Rn, Rm
			CycleCount = 1;
			dest = GR.rs[(CodeWord >> 8) & 0x0f];
			src = GR.rs[(CodeWord >> 4) & 0x0f];

			temp = dest - src;

			PSW.Z = IS_ZERO(temp);
			PSW.C = (temp & 0x100)? 1 : 0;
			PSW.S = SIGN8(temp);
			// reference: Z80 user manual
			PSW.OV = (((dest & 0x7f) - (src & 0x7f)) >> 7) ^ PSW.C;
			PSW.HC = (((dest & 0x0f) - (src & 0x0f)) & 0x10)? 1 : 0;

			break;

		case 0x88:
			// SUB Rn, Rm
			CycleCount = 1;
			dest = GR.rs[(CodeWord >> 8) & 0x0f];
			src = GR.rs[(CodeWord >> 4) & 0x0f];

			temp = dest - src;

			PSW.Z = IS_ZERO(temp);
			PSW.C = (temp & 0x100)? 1 : 0;
			PSW.S = SIGN8(temp);
			// reference: Z80 user manual
			PSW.OV = (((dest & 0x7f) - (src & 0x7f)) >> 7) ^ PSW.C;
			PSW.HC = (((dest & 0x0f) - (src & 0x0f)) & 0x10)? 1 : 0;

			GR.rs[(CodeWord >> 8) & 0x0f] = (temp & 0xff);

			break;

		case 0x89:
			// SUBC Rn, Rm
			CycleCount = 1;
			dest = GR.rs[(CodeWord >> 8) & 0x0f];
			src = GR.rs[(CodeWord >> 4) & 0x0f];

			temp = dest - src - PSW.C;

			// reference: Z80 user manual
			PSW.OV = (((dest & 0x7f) - (src & 0x7f) - PSW.C) >> 7) ^ ((temp & 0x100)? 1 : 0);
			PSW.HC = (((dest & 0x0f) - (src & 0x0f) - PSW.C) & 0x10)? 1 : 0;

			PSW.Z = IS_ZERO(temp);
			PSW.C = (temp & 0x100)? 1 : 0;
			PSW.S = SIGN8(temp);

			GR.rs[(CodeWord >> 8) & 0x0f] = (temp & 0xff);

			break;

		case 0x8a:
			// SLL Rn, Rm
			CycleCount = 1;
			dest = GR.rs[(CodeWord >> 8) & 0x0f];
			src = GR.rs[(CodeWord >> 4) & 0x0f];

			for( temp = (src & 0x07); temp > 0; temp-- ) {
				PSW.C = (dest & 0x80)? 1 : 0;
				dest <<= 1;
			}

			GR.rs[(CodeWord >> 8) & 0x0f] = (dest & 0xff);

			break;

		case 0x8b:
			// SLLC Rn, Rm
			CycleCount = 1;
			dest = (GR.rs[(CodeWord >> 8) & 0x0f] << 8) + GR.rs[((CodeWord >> 8) - 1) & 0x0f];
			src = GR.rs[(CodeWord >> 4) & 0x0f];

			for( temp = (src & 0x07); temp > 0; temp-- ) {
				PSW.C = (dest & 0x8000)? 1 : 0;
				dest <<= 1;
			}

			GR.rs[(CodeWord >> 8) & 0x0f] = ((dest >> 8) & 0xff);

			break;

		case 0x8c:
			// SRL Rn, Rm
			CycleCount = 1;
			dest = GR.rs[(CodeWord >> 8) & 0x0f];
			src = GR.rs[(CodeWord >> 4) & 0x0f];

			for( temp = (src & 0x07); temp > 0; temp-- ) {
				PSW.C = (dest & 0x01)? 1 : 0;
				dest >>= 1;
			}

			GR.rs[(CodeWord >> 8) & 0x0f] = (dest & 0xff);

			break;

		case 0x8d:
			// SRLC Rn, Rm
			CycleCount = 1;
			dest = (GR.rs[((CodeWord >> 8) + 1) & 0x0f] << 8) + GR.rs[(CodeWord >> 8) & 0x0f];
			src = GR.rs[(CodeWord >> 4) & 0x0f];

			for( temp = (src & 0x07); temp > 0; temp-- ) {
				PSW.C = (dest & 0x01)? 1 : 0;
				dest >>= 1;
			}

			GR.rs[(CodeWord >> 8) & 0x0f] = (dest & 0xff);

			break;

		case 0x8e:
			// SRA Rn, Rm
			CycleCount = 1;
			dest = GR.rs[(CodeWord >> 8) & 0x0f];
			src = GR.rs[(CodeWord >> 4) & 0x0f];

			for( temp = (src & 0x07); temp > 0; temp-- ) {
				PSW.C = (dest & 0x01)? 1 : 0;
				dest = (dest & 0x80) | (dest >> 1);
			}

			GR.rs[(CodeWord >> 8) & 0x0f] = (dest & 0xff);

			break;

		case 0x8f:
			if( (CodeWord & 0xf11f) == 0x810f ) {
				//EXTBW ERn
				src = GR.rs[(CodeWord >> 4) & 0x0f];

				PSW.S = SIGN8(dest);
				PSW.Z = IS_ZERO(dest);

				GR.rs[(CodeWord >> 8) & 0x0f] = PSW.S? 0xff : 0;
				break;
			}
			switch( CodeWord & 0xf0ff ) {
				case 0x801f:
					// DAA Rn
					// Uh gosh this is so much of a pain
					// reference: AMD64 general purpose and system instructions
					CycleCount = 1;
					dest = GR.rs[(CodeWord >> 8) & 0x0f];
					// lower nibble
					temp = dest & 0x0f;
					if( PSW.HC || ((dest & 0x0f) > 0x09) ) {
						dest += 0x06;
						PSW.HC = 1;
					}
					else {
						PSW.HC = 0;
					}
					// higher nibble
					if( PSW.C || ((dest & 0xf0) > 0x90) ) {
						dest += 0x60;
						PSW.C = 1;	// carry should always be set
					}
					else {
						PSW.C = 0;
					}
					// write back
					PSW.S = SIGN8(dest);
					PSW.Z = IS_ZERO(dest);
					GR.rs[(CodeWord >> 8) & 0x0f] = dest;
					break;

				case 0x803f:
					// DAS Rn
					// This is even more confusing than DAA
					// reference: AMD64 general purpose and system instructions
					CycleCount = 1;
					dest = GR.rs[(CodeWord >> 8) & 0x0f];
					// lower nibble
					if( PSW.HC || ((dest & 0x0f) > 0x09) ) {
						dest -= 0x06;
						PSW.HC = 1;
					}
					else {
						PSW.HC = 0;
					}
					// higher nibble
					if( PSW.C || ((dest & 0xf0) > 0x90) ) {
						dest -= 0x60;
						PSW.C = 1;	// carry should always be set
					}
					else {
						PSW.C = 0;
					}
					// write back
					PSW.S = SIGN8(dest);
					PSW.Z = IS_ZERO(dest);
					GR.rs[(CodeWord >> 8) & 0x0f] = dest;
					break;

				case 0x805f:
					//NEG Rn
					CycleCount = 1;
					dest = GR.rs[(CodeWord >> 8) & 0x0f];

					PSW.HC = (dest & 0x0f)? 0 : 1;	// ~0b0000 + 1 = 0b1_0000
					PSW.C = dest? 0 : 1;
					PSW.OV = ((dest & 0x7f)? 0 : 1) ^ PSW.C;

					dest = ~dest & 0xff;

					PSW.S = SIGN8(dest);
					PSW.Z = IS_ZERO(dest);

				default:
					retVal = CORE_ILLEGAL_INSTRUCTION;
			}
			break;

		case 0x90:
			if( (CodeWord & 0x0010) == 0x0000 ) {
				// L Rn, [ERm]
				// consider ROM window access delay
				memoryGetData(GET_DATA_SEG, GR.ers[(CodeWord >> 4) & 0x0e], 1);
				CycleCount = 1 + EAIncDelay + ROMWinAccessCount;

				src = DataRaw.byte;

				PSW.S = SIGN8(src);
				PSW.Z = IS_ZERO(src);

				GR.rs[(CodeWord >> 8) & 0x0f] = src;
			}
		default:
			retVal = CORE_ILLEGAL_INSTRUCTION;
			break;	
	}

	if( retVal == CORE_OK )
		((IntMaskCycle -= CycleCount) >= 0)? 0 : (IntMaskCycle = 0);
exit:
	return retVal;
}
