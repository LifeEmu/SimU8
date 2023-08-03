#include <stdio.h>
#include <stdbool.h>


#include "../inc/mmu.h"
#include "../inc/core.h"


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


// Enum for types of ALU operations
typedef enum {
	_ALU_ADD,
	_ALU_ADD_W,
	_ALU_ADDC,
	_ALU_AND,
	_ALU_OR,
	_ALU_XOR,
	_ALU_CMP,
	_ALU_CMP_W,
	_ALU_CMPC,
	_ALU_SUB,
	_ALU_SUBC,
	_ALU_SLL,
	_ALU_SRL,
	_ALU_SRA,
	_ALU_DAA,
	_ALU_DAS,
	_ALU_NEG,
	_ALU_SB,
	_ALU_TB,
	_ALU_RB
} _ALU_OP;


// Do ALU operations, modifies PSW
// A place to hide all the ugly code behind the scene
// If only I could use the flags of the host CPU...
static uint16_t _ALU(register uint16_t dest, register uint16_t src, _ALU_OP op) {
	uint32_t retVal = 0;
	int16_t shifter = 0;

	switch( op ) {
		case _ALU_ADD:
			// 8-bit addition
			dest &= 0xff;
			src &= 0xff;
			retVal = dest + src;
			PSW.C = (retVal & 0x100)? 1 : 0;
			retVal &= 0xff;
			PSW.Z = IS_ZERO(retVal);
			PSW.S = SIGN8(retVal);
			// reference: Z80 user manual
			PSW.OV = (((dest & 0x7f) + (src & 0x7f)) >> 7) ^ PSW.C;
			PSW.HC = (((dest & 0x0f) + (src & 0x0f)) & 0x10)? 1 : 0;
			break;

		case _ALU_ADD_W:
			// 16-bit addition
			retVal = dest + src;
			PSW.C = (retVal & 0x10000)? 1 : 0;
			retVal &= 0xffff;
			PSW.Z = IS_ZERO(retVal);
			PSW.S = SIGN16(retVal);
			// reference: Z80 user manual
			PSW.OV = (((dest & 0x7fff) + (src & 0x7fff)) >> 15) ^ PSW.C;
			PSW.HC = (((dest & 0x0fff) + (src & 0x0fff)) & 0x1000)? 1 : 0;
			break;
		
		case _ALU_ADDC:
			// 8-bit addition with carry
			dest &= 0xff;
			src &= 0xff;
			retVal = dest + src + PSW.C;
			PSW.C = (retVal & 0x100)? 1 : 0;
			retVal &= 0xff;
			PSW.Z = IS_ZERO(retVal);
			PSW.S = SIGN8(retVal);
			// reference: Z80 user manual
			PSW.OV = (((dest & 0x7f) + (src & 0x7f) + PSW.C) >> 7) ^ PSW.C;
			PSW.HC = (((dest & 0x0f) + (src & 0x0f) + PSW.C) & 0x10)? 1 : 0;
			break;
		
		case _ALU_AND:
			// 8-bit logical AND
			dest &= 0xff;
			src &= 0xff;
			retVal = dest & src;
			PSW.Z = IS_ZERO(retVal);
			PSW.S = SIGN8(retVal);
			break;

		case _ALU_OR:
			// 8-bit logical OR
			dest &= 0xff;
			src &= 0xff;
			retVal = dest | src;
			PSW.Z = IS_ZERO(retVal);
			PSW.S = SIGN8(retVal);
			break;

		case _ALU_XOR:
			// 8-bit logical XOR
			dest &= 0xff;
			src &= 0xff;
			retVal = dest ^ src;
			PSW.Z = IS_ZERO(retVal);
			PSW.S = SIGN8(retVal);
			break;

		case _ALU_CMP:
		case _ALU_SUB:
			// 8-bit comparison & subtraction
			dest &= 0xff;
			src &= 0xff;
			retVal = dest - src;
			PSW.C = (retVal & 0x100)? 1 : 0;
			retVal &= 0xff;
			PSW.Z = IS_ZERO(retVal);
			PSW.S = SIGN8(retVal);
			// reference: Z80 user manual
			PSW.OV = (((dest & 0x7f) - (src & 0x7f)) >> 7) ^ PSW.C;
			PSW.HC = (((dest & 0x0f) - (src & 0x0f)) & 0x10)? 1 : 0;
			break;

		case _ALU_CMP_W:
			// 16-bit comparison
			retVal = dest - src;
			PSW.C = (retVal & 0x10000)? 1 : 0;
			retVal &= 0xffff;
			PSW.Z = IS_ZERO(retVal);
			PSW.S = SIGN16(retVal);
			// reference: Z80 user manual
			PSW.OV = (((dest & 0x7fff) - (src & 0x7fff)) >> 15) ^ PSW.C;
			PSW.HC = (((dest & 0x0fff) - (src & 0x0fff)) & 0x10)? 1 : 0;
			break;

		case _ALU_CMPC:
		case _ALU_SUBC:
			// 8-bit comparison & subtraction with carry
			dest &= 0xff;
			src &= 0xff;
			retVal = dest - src - PSW.C;
			PSW.C = (retVal & 0x100)? 1 : 0;
			retVal &= 0xff;
			PSW.Z = IS_ZERO(retVal);
			PSW.S = SIGN8(retVal);
			// reference: Z80 user manual
			PSW.OV = (((dest & 0x7f) - (src & 0x7f) - PSW.C) >> 7) ^ PSW.C;
			PSW.HC = (((dest & 0x0f) - (src & 0x0f) - PSW.C) & 0x10)? 1 : 0;
			break;

		case _ALU_SLL:
			// logical shift left
			retVal = dest << (src & 0x07);
			PSW.C = (retVal & 0x100)? 1 : 0;
			retVal &= 0xff;
			break;

		case _ALU_SRL:
			// logical shift right
			retVal = (dest << 1) >> ((src & 0x07) + 1);	// leave space for carry flag
			PSW.C = retVal & 0x01;
			retVal = (retVal >> 1) & 0xff;
			break;

		case _ALU_SRA:
			// arithmetic shift right
			shifter = dest << 8;		// use arithmetic shift of host processor
			shifter >>= ((src & 0x07) + 7);	// leave space for carry flag
			PSW.C = shifter & 0x01;
			retVal = (shifter >> 1) & 0xff;
			break;

		case _ALU_DAA:
			// decimal adjustment for addition
			// Uh gosh this is so much of a pain
			// reference: AMD64 general purpose and system instructions
			retVal = dest;
			// lower nibble
			if( PSW.HC || ((retVal & 0x0f) > 0x09) ) {
				retVal += 0x06;
				PSW.HC = 1;
			}
			else {
				PSW.HC = 0;
			}
			// higher nibble
			if( PSW.C || ((retVal & 0xf0) > 0x90) ) {
				retVal += 0x60;
				PSW.C = 1;	// carry should always be set
			}
			else {
				PSW.C = 0;
			}
			PSW.S = SIGN8(retVal);
			PSW.Z = IS_ZERO(retVal);
			break;

		case _ALU_DAS:
			// decimal adjustment for subtraction
			// This is even more confusing than DAA
			// reference: AMD64 general purpose and system instructions
			retVal = dest;
			// lower nibble
			if( PSW.HC || ((retVal & 0x0f) > 0x09) ) {
				retVal -= 0x06;
				PSW.HC = 1;
			}
			else {
				PSW.HC = 0;
			}
			// higher nibble
			if( PSW.C || ((retVal & 0xf0) > 0x90) ) {
				retVal -= 0x60;
				PSW.C = 1;	// carry should always be set
			}
			else {
				PSW.C = 0;
			}
			// write back
			PSW.S = SIGN8(retVal);
			PSW.Z = IS_ZERO(retVal);
			break;

		case _ALU_NEG:
			// 8-bit negate
			retVal = dest & 0xff;
			PSW.HC = (retVal & 0x0f)? 0 : 1;	// ~0b0000 + 1 = 0b1_0000
			PSW.C = retVal? 0 : 1;			// ~0b0000_0000 + 1 = 0b1_0000_0000
			PSW.OV = ((retVal & 0x7f)? 0 : 1) ^ PSW.C;
			retVal = (~retVal + 1) & 0xff;
			PSW.S = SIGN8(retVal);
			PSW.Z = IS_ZERO(retVal);
			break;

		case _ALU_SB:
			// set bit
			src = 0x01 << (src & 0x07);
			PSW.Z = IS_ZERO(dest & src);
			dest |= src;
			break;

		case _ALU_TB:
			// test bit
			src = 0x01 << (src & 0x07);
			PSW.Z = IS_ZERO(dest & src);
			break;

		case _ALU_RB:
			// reset bit
			src = 0x01 << (src & 0x07);
			PSW.Z = IS_ZERO(dest & src);
			dest &= ~src;
			break;
	}

	return retVal;
}


// Gets pointer to current EPSW
PSW_t* getCurrEPSW(void) {
	switch( PSW.ELevel ) {
		case 2:
			return &EPSW2;
		case 3:
			return &EPSW3;
		default:
			return &EPSW1;
	}
}

// Gets pointer to current ELR
PC_t* getCurrELR(void) {
	switch( PSW.ELevel ) {
		case 1:
			return &ELR1;
		case 2:
			return &ELR2;
		case 3:
			return &ELR3;
		default:
			return &LR;
	}
}

// Gets pointer to current ECSR
SR_t* getCurrECSR(void) {
	switch( PSW.ELevel ) {
		case 1:
			return &ECSR1;
		case 2:
			return &ECSR2;
		case 3:
			return &ECSR3;
		default:
			return &LCSR;
	}
}


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
		printf("\tER%-2d = %04Xh\n", i, GR.ers[i >> 1]);
	}

	puts("\n Control registers:");

	printf("\tCSR:PC = %01X:%04Xh\n", CSR, PC);
	printf("\tSP = %04Xh\n", SP);
	printf("\tDSR = %02Xh\n", DSR);
	printf("\tEA = %04Xh\n", EA);
	printf("\tPSW = %02Xh\n", PSW.raw);
	printf("\t\tC Z S V I H MIE\n\t\t%1d %1d %1d %1d %1d %1d  %1d\n", PSW.C, PSW.Z, PSW.S, PSW.OV, PSW.MIE, PSW.HC, PSW.ELevel);

	printf("\n\tLCSR:LR = %01X:%04Xh\n", LCSR, LR);
	printf("\tECSR1:ELR1 = %01X:%04Xh\n", ECSR1, ELR1);
	printf("\tECSR2:ELR2 = %01X:%04Xh\n", ECSR2, ELR2);
	printf("\tECSR3:ELR3 = %01X:%04Xh\n", ECSR3, ELR3);

	printf("\n\tEPSW1 = %02Xh\n", EPSW1.raw);
	printf("\tEPSW2 = %02Xh\n", EPSW2.raw);
	printf("\tEPSW3 = %02Xh\n", EPSW3.raw);

	puts("========       End       ========");

	return CORE_OK;
}


CORE_STATUS coreStep(void) {
	CORE_STATUS retVal = CORE_OK;
	CycleCount = 0;
	uint8_t decodeIndex, regNumDest, regNumSrc;
	uint16_t immNum;
	bool isEAInc = false;
	bool isDSRSet = false;

	uint64_t dest, src;
	Data_t tempData = {0};

	if( IsMemoryInited == false ) {
		retVal = CORE_MEMORY_UNINITIALIZED;
		goto exit;
	}

	// fetch instruction
	memoryGetCodeWord(CSR, PC);

	// increment PC
	PC = (PC + 2) & 0xfffe;

	decodeIndex = ((CodeWord >> 8) & 0xf0) | (CodeWord & 0x0f);
	regNumDest = (CodeWord >> 8) & 0x0f;
	regNumSrc = (CodeWord >> 4) & 0x0f;
	immNum = CodeWord & 0xff;
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
			GR.rs[regNumDest] = immNum;

			PSW.Z = IS_ZERO(immNum);
			PSW.S = SIGN8(immNum);
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
			dest = GR.rs[regNumDest];
			src = immNum;
			GR.rs[regNumDest] = _ALU(dest, src, _ALU_ADD);
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
			dest = GR.rs[(CodeWord >> 8) & 0x0f];
			src = (immNum);
			GR.rs[regNumDest] = _ALU(dest, src, _ALU_AND);
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
			dest = GR.rs[regNumDest];
			src = (immNum);
			GR.rs[regNumDest] = _ALU(dest, src, _ALU_OR);
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
			dest = GR.rs[regNumDest];
			src = (immNum);
			GR.rs[regNumDest] = _ALU(dest, src, _ALU_XOR);
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
			dest = GR.rs[regNumDest];
			src = immNum;
			_ALU(dest, src, _ALU_CMPC);
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
			dest = GR.rs[regNumDest];
			src = immNum;
			GR.rs[regNumDest] = _ALU(dest, src, _ALU_ADDC);
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
			dest = GR.rs[regNumDest];
			src = immNum;
			_ALU(dest, src, _ALU_CMP);
			break;

		case 0x80:
			// MOV Rn, Rm
			CycleCount = 1;
			src = GR.rs[regNumSrc];

			PSW.Z = IS_ZERO(src);
			PSW.S = SIGN8(src);

			GR.rs[regNumDest] = src;
			break;

		case 0x81:
			// ADD Rn, Rm
			CycleCount = 1;
			dest = GR.rs[regNumDest];
			src = GR.rs[regNumSrc];
			GR.rs[regNumDest] = _ALU(dest, src, _ALU_ADD);
			break;

		case 0x82:
			// AND Rn, Rm
			CycleCount = 1;
			dest = GR.rs[regNumDest];
			src = GR.rs[regNumSrc];
			GR.rs[regNumDest] = _ALU(dest, src, _ALU_AND);
			break;

		case 0x83:
			// OR Rn, Rm
			CycleCount = 1;
			dest = GR.rs[regNumDest];
			src = GR.rs[regNumSrc];
			GR.rs[regNumDest] = _ALU(dest, src, _ALU_OR);
			break;

		case 0x84:
			// XOR Rn, Rm
			CycleCount = 1;
			dest = GR.rs[regNumDest];
			src = GR.rs[regNumSrc];
			GR.rs[regNumDest] = _ALU(dest, src, _ALU_XOR);
			break;

		case 0x85:
			// CMPC Rn, Rm
			CycleCount = 1;
			dest = GR.rs[regNumDest];
			src = GR.rs[regNumSrc];
			_ALU(dest, src, _ALU_CMPC);
			break;

		case 0x86:
			// ADDC Rn, Rm
			CycleCount = 1;
			dest = GR.rs[regNumDest];
			src = GR.rs[regNumSrc];
			GR.rs[regNumDest] = _ALU(dest, src, _ALU_ADDC);
			break;

		case 0x87:
			// CMP Rn, Rm
			CycleCount = 1;
			src = GR.rs[regNumSrc];
			_ALU(dest, src, _ALU_CMP);
			break;

		case 0x88:
			// SUB Rn, Rm
			CycleCount = 1;
			dest = GR.rs[regNumDest];
			src = GR.rs[regNumSrc];
			GR.rs[regNumDest] = _ALU(dest, src, _ALU_SUB);
			break;

		case 0x89:
			// SUBC Rn, Rm
			CycleCount = 1;
			dest = GR.rs[regNumDest];
			src = GR.rs[regNumSrc];
			GR.rs[regNumDest] = _ALU(dest, src, _ALU_SUBC);
			break;

		case 0x8a:
			// SLL Rn, Rm
			CycleCount = 1 + EAIncDelay;
			dest = GR.rs[regNumDest];
			src = GR.rs[regNumSrc];
			GR.rs[regNumDest] = _ALU(dest, src, _ALU_SLL);
			break;

		case 0x8b:
			// SLLC Rn, Rm
			CycleCount = 1 + EAIncDelay;
			dest = (GR.rs[regNumDest] << 8) | GR.rs[(regNumDest - 1) & 0x0f];
			src = GR.rs[regNumSrc] & 0x07;

			dest >>= (8 - src);
			PSW.C = (dest & 0x100)? 1 : 0;

			GR.rs[regNumDest] = (dest & 0xff);

			break;

		case 0x8c:
			// SRL Rn, Rm
			CycleCount = 1 + EAIncDelay;
			dest = GR.rs[regNumDest];
			src = GR.rs[regNumSrc];
			GR.rs[regNumDest] = _ALU(dest, src, _ALU_SRL);
			break;

		case 0x8d:
			// SRLC Rn, Rm
			CycleCount = 1 + EAIncDelay;
			dest = (GR.rs[(regNumDest + 1) & 0x0f] << 9) | (GR.rs[regNumDest] << 1);	// bit 0 for carry
			src = GR.rs[regNumSrc] & 0x07;

			dest >>= src;
			PSW.C = dest & 0x01;
			dest = (dest >> 1) & 0xff;

			GR.rs[regNumDest] = (dest & 0xff);

			break;

		case 0x8e:
			// SRA Rn, Rm
			CycleCount = 1 + EAIncDelay;
			dest = GR.rs[regNumDest];
			src = GR.rs[regNumSrc];
			GR.rs[regNumDest] = _ALU(dest, src, _ALU_SRA);
			break;

		case 0x8f:
			if( (CodeWord & 0xf11f) == 0x810f ) {
				//EXTBW ERn
				src = GR.rs[regNumSrc];

				PSW.S = SIGN8(dest);
				PSW.Z = IS_ZERO(dest);

				GR.rs[regNumDest] = PSW.S? 0xff : 0;
				break;
			}
			switch( CodeWord & 0xf0ff ) {
				case 0x801f:
					// DAA Rn
					// Uh gosh this is so much of a pain
					// reference: AMD64 general purpose and system instructions
					CycleCount = 1;
					dest = GR.rs[regNumDest];
					GR.rs[regNumDest] = _ALU(dest, 0, _ALU_DAA);
					break;

				case 0x803f:
					// DAS Rn
					// This is even more confusing than DAA
					// reference: AMD64 general purpose and system instructions
					CycleCount = 1;
					dest = GR.rs[regNumDest];
					GR.rs[regNumDest] = _ALU(dest, 0, _ALU_DAS);
					break;

				case 0x805f:
					//NEG Rn
					CycleCount = 1;
					dest = GR.rs[regNumDest];
					GR.rs[regNumDest] = _ALU(dest, 0, _ALU_NEG);
					break;

				default:
					retVal = CORE_ILLEGAL_INSTRUCTION;
					break;
			}
			break;

		case 0x90:
			if( (CodeWord & 0x0010) == 0x0000 ) {
				// L Rn, [ERm]
				src = GR.ers[regNumSrc >> 1];
				CycleCount += EAIncDelay;
			}
			else {
				switch( CodeWord & 0xf0ff ) {
					case 0x9010:
						// L Rn, [adr]
						// fetch source address
						memoryGetCodeWord(CSR, PC);
						src = CodeWord;
						CycleCount += EAIncDelay;
						PC = (PC + 2) & 0xfffe;
						break;

					case 0x9030:
						// L Rn, [EA]
						src = EA;
						break;

					case 0x9050:
						// L Rn, [EA+]
						src = EA;
						EA += 1; isEAInc = true;
						break;

					default:
						retVal = CORE_ILLEGAL_INSTRUCTION;
						goto exit;
				}
			}

			memoryGetData(GET_DATA_SEG, src, 1);
			dest = DataRaw.byte;
			CycleCount += 1 + ROMWinAccessCount;

			PSW.S = SIGN8(dest);
			PSW.Z = IS_ZERO(dest);
			GR.rs[regNumDest] = dest;
			break;

		case 0x91:
			if( (CodeWord & 0x0010) == 0x0000 ) {
				// ST Rn, [ERm]
				dest = GR.ers[regNumSrc >> 1];
				CycleCount += EAIncDelay;
			}
			else {
				switch( CodeWord & 0xf0ff ) {
					case 0x9010:
						// ST Rn, [adr]
						// fetch destination address
						memoryGetCodeWord(CSR, PC);
						dest = CodeWord;
						CycleCount += EAIncDelay;
						PC = (PC + 2) & 0xfffe;
						break;

					case 0x9030:
						// ST Rn, [EA]
						dest = EA;
						break;

					case 0x9050:
						// ST Rn, [EA+]
						dest = EA;
						EA += 1; isEAInc = true;
						break;

					default:
						retVal = CORE_ILLEGAL_INSTRUCTION;
						goto exit;
				}
			}

			tempData.byte = GR.rs[regNumDest];
			memorySetData(GET_DATA_SEG, dest, 1, tempData);
			CycleCount += 1;
			break;

		case 0x92:
			if( (CodeWord & 0x0110) == 0x0000 ) {
				// L ERn, [ERm]
				src = GR.ers[regNumSrc >> 1];
				CycleCount += EAIncDelay;
			}
			else {
				switch( CodeWord & 0xf1ff ) {
					case 0x9012:
						// L ERn, [adr]
						// fetch source address
						memoryGetCodeWord(CSR, PC);
						src = CodeWord;
						CycleCount += EAIncDelay;
						PC = (PC + 2) & 0xfffe;
						break;

					case 0x9032:
						// L ERn, [EA]
						src = EA;
						break;

					case 0x9052:
						// L ERn, [EA+]
						src = EA;
						EA = (EA + 2) & 0xfffe; isEAInc = true;
						break;

					default:
						retVal = CORE_ILLEGAL_INSTRUCTION;
						goto exit;
				}
			}

			memoryGetData(GET_DATA_SEG, src, 2);
			dest = DataRaw.word;
			CycleCount += 2 + ROMWinAccessCount;

			PSW.S = SIGN16(dest);
			PSW.Z = IS_ZERO(dest);
			GR.ers[regNumDest >> 1] = dest;
			break;

		case 0x93:
			if( (CodeWord & 0x0110) == 0x0000 ) {
				// ST ERn, [ERm]
				dest = GR.ers[regNumSrc >> 1];
				CycleCount += EAIncDelay;
			}
			else {
				switch( CodeWord & 0xf1ff ) {
					case 0x9013:
						// ST ERn, [adr]
						// fetch destination address
						memoryGetCodeWord(CSR, PC);
						dest = CodeWord;
						CycleCount += EAIncDelay;
						PC = (PC + 2) & 0xfffe;
						break;

					case 0x9033:
						// ST ERn, [EA]
						dest = EA;
						break;

					case 0x9053:
						// ST ERn, [EA+]
						dest = EA;
						EA = (EA + 2) & 0xfffe; isEAInc = true;
						break;

					default:
						retVal = CORE_ILLEGAL_INSTRUCTION;
						goto exit;
				}
			}

			tempData.word = GR.ers[regNumDest >> 1];
			memorySetData(GET_DATA_SEG, dest, 2, tempData);
			CycleCount += 2;
			break;

		case 0x94:
			src = EA;
			switch( CodeWord & 0xf3ff ) {
				case 0x9034:
					// L XRn, [EA]
					break;

				case 0x9054:
					// L XRn, [EA+]
					EA = (EA + 4) & 0xfffe;
					isEAInc = true;
					break;

				default:
					retVal = CORE_ILLEGAL_INSTRUCTION;
					goto exit;
			}

			memoryGetData(GET_DATA_SEG, src, 4);
			dest = DataRaw.dword;
			CycleCount = 4 + ROMWinAccessCount;

			PSW.S = SIGN32(dest);
			PSW.Z = IS_ZERO(dest);
			GR.xrs[regNumDest >> 2] = dest;
			break;

		case 0x95:
			dest = EA;
			switch( CodeWord & 0xf3ff ) {
				case 0x9035:
					// ST XRn, [EA]
					break;

				case 0x9055:
					// ST XRn, [EA+]
					EA = (EA + 4) & 0xfffe;
					isEAInc = true;
					break;

				default:
					retVal = CORE_ILLEGAL_INSTRUCTION;
					goto exit;
			}

			tempData.dword = GR.xrs[regNumDest >> 2];
			memorySetData(GET_DATA_SEG, dest, 4, tempData);
			CycleCount = 4;
			break;

		case 0x96:
			src = EA;
			switch( CodeWord & 0xf7ff ) {
				case 0x9036:
					// L QRn, [EA]
					break;

				case 0x9056:
					// L QRn, [EA+]
					EA = (EA + 8) & 0xfffe;
					isEAInc = true;
					break;

				default:
					retVal = CORE_ILLEGAL_INSTRUCTION;
					goto exit;
			}

			memoryGetData(GET_DATA_SEG, src, 8);
			dest = DataRaw.qword;
			CycleCount = 8 + ROMWinAccessCount;

			PSW.S = SIGN64(dest);
			PSW.Z = IS_ZERO(dest);
			GR.qrs[regNumDest >> 3] = dest;
			break;

		case 0x97:
			dest = EA;
			switch( CodeWord & 0xf7ff ) {
				case 0x9037:
					// ST QRn, [EA]
					break;

				case 0x9057:
					// ST QRn, [EA+]
					EA = (EA + 8) & 0xfffe;
					isEAInc = true;
					break;

				default:
					retVal = CORE_ILLEGAL_INSTRUCTION;
					goto exit;
			}

			tempData.qword = GR.qrs[regNumDest >> 3];
			memorySetData(GET_DATA_SEG, dest, 8, tempData);
			CycleCount = 8;
			break;

		case 0x98:
			if( (CodeWord & 0xf01f) != 0x9008 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// L Rn, d16[ERm]
			src = GR.ers[regNumSrc >> 1];
			memoryGetCodeWord(CSR, PC);
			src = (src + CodeWord) & 0xffff;
			memoryGetData(GET_DATA_SEG, src, 1);
			GR.rs[regNumDest] = DataRaw.byte;
			CycleCount = 2 + ROMWinAccessCount + EAIncDelay;
			break;

		case 0x99:
			if( (CodeWord & 0xf01f) != 0x9009 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// ST Rn, d16[ERm]
			dest = GR.ers[regNumSrc >> 1];
			memoryGetCodeWord(CSR, PC);
			dest = (dest + CodeWord) & 0xffff;
			tempData.byte = GR.rs[regNumDest];
			memorySetData(GET_DATA_SEG, dest, 1, tempData);
			CycleCount = 2 + EAIncDelay;
			break;

		case 0x9a:
			if( (CodeWord & 0x0080) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// SLL Rn, Rm
			CycleCount = 1 + EAIncDelay;
			dest = GR.rs[regNumDest];
			src = regNumSrc;
			GR.rs[regNumDest] = _ALU(dest, src, _ALU_SLL);
			break;

		case 0x9b:
			if( (CodeWord & 0x0080) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// SLLC Rn, Rm
			CycleCount = 1 + EAIncDelay;
			dest = (GR.rs[regNumDest] << 8) | GR.rs[(regNumDest - 1) & 0x0f];
			src = regNumSrc & 0x07;

			dest >>= (8 - src);
			PSW.C = (dest & 0x100)? 1 : 0;

			GR.rs[regNumDest] = (dest & 0xff);

			break;

		case 0x9c:
			if( (CodeWord & 0x0080) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// SRL Rn, Rm
			CycleCount = 1 + EAIncDelay;
			dest = GR.rs[regNumDest];
			src = regNumSrc;
			GR.rs[regNumDest] = _ALU(dest, src, _ALU_SRL);
			break;

		case 0x9d:
			if( (CodeWord & 0x0080) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// SRLC Rn, Rm
			CycleCount = 1 + EAIncDelay;
			dest = (GR.rs[(regNumDest + 1) & 0x0f] << 9) | (GR.rs[regNumDest] << 1);	// bit 0 for carry
			src = regNumSrc & 0x07;

			dest >>= src;
			PSW.C = dest & 0x01;
			dest = (dest >> 1) & 0xff;

			GR.rs[regNumDest] = (dest & 0xff);

			break;

		case 0x9e:
			if( (CodeWord & 0x0080) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// SRA Rn, Rm
			CycleCount = 1 + EAIncDelay;
			dest = GR.rs[regNumDest];
			src = regNumSrc;
			GR.rs[regNumDest] = _ALU(dest, src, _ALU_SRA);
			break;

		case 0x9f:
			if( (CodeWord & 0x0f00) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// _LDSR Rd
			CycleCount = 1;
			DSR = GR.rs[regNumSrc];
			isDSRSet = true;
			break;

		case 0xa0:
			src = regNumSrc & 0x07;
			if( (CodeWord & 0x0080) != 0x0000 ) {
				if( (CodeWord & 0x0f80) != 0x0080 ) {
					retVal = CORE_ILLEGAL_INSTRUCTION;
					break;
				}
				// SB Dbitadr
				memoryGetCodeWord(CSR, PC);
				memoryGetData(GET_DATA_SEG, (EA_t)CodeWord, 1);
				tempData.byte = _ALU(DataRaw.byte, src, _ALU_SB);
				memorySetData(GET_DATA_SEG, CodeWord, 1, tempData);
				CycleCount = 2 + EAIncDelay;
				break;
			}
			// SB Rn.b
			GR.rs[regNumDest] = _ALU(GR.rs[regNumDest], src, _ALU_SB);
			CycleCount = 1;
			break;

		case 0xa1:
			src = regNumSrc & 0x07;
			if( (CodeWord & 0x0080) != 0x0000 ) {
				if( (CodeWord & 0x0f80) != 0x0080 ) {
					retVal = CORE_ILLEGAL_INSTRUCTION;
					break;
				}
				// TB Dbitadr
				memoryGetCodeWord(CSR, PC);
				memoryGetData(GET_DATA_SEG, (EA_t)CodeWord, 1);
				_ALU(DataRaw.byte, src, _ALU_TB);
				CycleCount = 2 + ROMWinAccessCount + EAIncDelay;
				break;
			}
			// TB Rn.b
			_ALU(GR.rs[regNumDest], src, _ALU_TB);
			CycleCount = 1;
			break;

		case 0xa2:
			src = regNumSrc & 0x07;
			if( (CodeWord & 0x0080) != 0x0000 ) {
				if( (CodeWord & 0x0f80) != 0x0080 ) {
					retVal = CORE_ILLEGAL_INSTRUCTION;
					break;
				}
				// RB Dbitadr
				memoryGetCodeWord(CSR, PC);
				memoryGetData(GET_DATA_SEG, (EA_t)CodeWord, 1);
				tempData.byte = _ALU(DataRaw.byte, src, _ALU_RB);
				memorySetData(GET_DATA_SEG, CodeWord, 1, tempData);
				CycleCount = 2 + EAIncDelay;
				break;
			}
			// RB Rn.b
			GR.rs[regNumDest] = _ALU(GR.rs[regNumDest], src, _ALU_RB);
			CycleCount = 1;
			break;

		case 0xa3:
			if( (CodeWord & 0x00f0) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// MOV Rn, PSW
			GR.rs[regNumDest] = PSW.raw;
			CycleCount = 1;
			break;

		case 0xa4:
			if( (CodeWord & 0x00f0) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// MOV Rn, EPSW
			if( PSW.ELevel != 0 )
				GR.rs[regNumDest] = getCurrEPSW()->raw;
			CycleCount = 2;
			break;

		case 0xa5:
			if( (CodeWord & 0x01f0) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// MOV ERn, ELR
			GR.ers[regNumDest] = *getCurrELR();
			CycleCount = 3;
			break;

		case 0xa6:
			// MOV Rn, CRm
			retVal = CORE_UNIMPLEMENTED;
			break;

		case 0xa7:
			if( (CodeWord & 0x00f0) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// MOV Rn, ECSR
			GR.rs[regNumDest] = *getCurrECSR();
			CycleCount = 2;
			break;

		case 0xa8:
			if( (CodeWord & 0xf11f) != 0xa008 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// L ERn, d16[ERm]
			src = GR.ers[regNumSrc >> 1];
			memoryGetCodeWord(CSR, PC);
			src = (src + CodeWord) & 0xffff;
			memoryGetData(GET_DATA_SEG, src, 2);
			GR.ers[regNumDest >> 1] = DataRaw.word;
			CycleCount = 3 + ROMWinAccessCount + EAIncDelay;
			break;

		case 0xa9:
			if( (CodeWord & 0xf11f) != 0xa009 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// ST ERn, d16[ERm]
			dest = GR.ers[regNumSrc >> 1];
			memoryGetCodeWord(CSR, PC);
			dest = (dest + CodeWord) & 0xffff;
			tempData.word = GR.ers[regNumDest >> 1];
			memorySetData(GET_DATA_SEG, dest, 2, tempData);
			CycleCount = 3 + EAIncDelay;
			break;

		case 0xaa:
			if( (CodeWord & 0x0f10) != 0x0110 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// MOV SP, ERm
			SP = GR.ers[regNumSrc >> 1];
			CycleCount = 1;
			break;


		default:
			retVal = CORE_ILLEGAL_INSTRUCTION;
			break;	
	}



	EAIncDelay = isEAInc? 1 : 0;
	NextAccess = isDSRSet? DATA_ACCESS_DSR : DATA_ACCESS_PAGE0;

	if( retVal == CORE_OK )
		if( (IntMaskCycle -= CycleCount) < 0 )
			IntMaskCycle = 0;

exit:
	return retVal;
}
