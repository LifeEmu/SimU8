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
			PSW.field.C = (retVal & 0x100)? 1 : 0;
			retVal &= 0xff;
			PSW.field.Z = IS_ZERO(retVal);
			PSW.field.S = SIGN8(retVal);
			// reference: Z80 user manual
			PSW.field.OV = (((dest & 0x7f) + (src & 0x7f)) >> 7) ^ PSW.field.C;
			PSW.field.HC = (((dest & 0x0f) + (src & 0x0f)) & 0x10)? 1 : 0;
			break;

		case _ALU_ADD_W:
			// 16-bit addition
			retVal = dest + src;
			PSW.field.C = (retVal & 0x10000)? 1 : 0;
			retVal &= 0xffff;
//			printf("\n_ALU_ADD_W | result = %04Xh\n", retVal);
			PSW.field.Z = IS_ZERO(retVal);
			PSW.field.S = SIGN16(retVal);
			// reference: Z80 user manual
			PSW.field.OV = (((dest & 0x7fff) + (src & 0x7fff)) >> 15) ^ PSW.field.C;
			PSW.field.HC = (((dest & 0x0fff) + (src & 0x0fff)) & 0x1000)? 1 : 0;
//			printf("\t\tPSW = %02X\n", PSW.raw);
			break;

		case _ALU_ADDC:
			// 8-bit addition with carry
			dest &= 0xff;
			src &= 0xff;
			retVal = dest + src + PSW.field.C;
			PSW.field.C = (retVal & 0x100)? 1 : 0;
			retVal &= 0xff;
			PSW.field.Z = IS_ZERO(retVal);
			PSW.field.S = SIGN8(retVal);
			// reference: Z80 user manual
			PSW.field.OV = (((dest & 0x7f) + (src & 0x7f) + PSW.field.C) >> 7) ^ PSW.field.C;
			PSW.field.HC = (((dest & 0x0f) + (src & 0x0f) + PSW.field.C) & 0x10)? 1 : 0;
			break;

		case _ALU_AND:
			// 8-bit logical AND
			dest &= 0xff;
			src &= 0xff;
			retVal = dest & src;
			PSW.field.Z = IS_ZERO(retVal);
			PSW.field.S = SIGN8(retVal);
			break;

		case _ALU_OR:
			// 8-bit logical OR
			dest &= 0xff;
			src &= 0xff;
			retVal = dest | src;
			PSW.field.Z = IS_ZERO(retVal);
			PSW.field.S = SIGN8(retVal);
			break;

		case _ALU_XOR:
			// 8-bit logical XOR
			dest &= 0xff;
			src &= 0xff;
			retVal = dest ^ src;
			PSW.field.Z = IS_ZERO(retVal);
			PSW.field.S = SIGN8(retVal);
			break;

		case _ALU_CMP:
		case _ALU_SUB:
			// 8-bit comparison & subtraction
			dest &= 0xff;
			src &= 0xff;
			retVal = dest - src;
			PSW.field.C = (retVal & 0x100)? 1 : 0;
			retVal &= 0xff;
			PSW.field.Z = IS_ZERO(retVal);
			PSW.field.S = SIGN8(retVal);
			// reference: Z80 user manual
			PSW.field.OV = (((dest & 0x7f) - (src & 0x7f)) >> 7) ^ PSW.field.C;
			PSW.field.HC = (((dest & 0x0f) - (src & 0x0f)) & 0x10)? 1 : 0;
			break;

		case _ALU_CMP_W:
			// 16-bit comparison
			retVal = dest - src;
			PSW.field.C = (retVal & 0x10000)? 1 : 0;
			retVal &= 0xffff;
			PSW.field.Z = IS_ZERO(retVal);
			PSW.field.S = SIGN16(retVal);
			// reference: Z80 user manual
			PSW.field.OV = (((dest & 0x7fff) - (src & 0x7fff)) >> 15) ^ PSW.field.C;
			PSW.field.HC = (((dest & 0x0fff) - (src & 0x0fff)) & 0x10)? 1 : 0;
			break;

		case _ALU_CMPC:
		case _ALU_SUBC:
			// 8-bit comparison & subtraction with carry
			dest &= 0xff;
			src &= 0xff;
			retVal = dest - src - PSW.field.C;
			PSW.field.C = (retVal & 0x100)? 1 : 0;
			retVal &= 0xff;
			PSW.field.Z = IS_ZERO(retVal);
			PSW.field.S = SIGN8(retVal);
			// reference: Z80 user manual
			PSW.field.OV = (((dest & 0x7f) - (src & 0x7f) - PSW.field.C) >> 7) ^ PSW.field.C;
			PSW.field.HC = (((dest & 0x0f) - (src & 0x0f) - PSW.field.C) & 0x10)? 1 : 0;
			break;

		case _ALU_SLL:
			// logical shift left
			retVal = dest << (src & 0x07);
			PSW.field.C = (retVal & 0x100)? 1 : 0;
			retVal &= 0xff;
			break;

		case _ALU_SRL:
			// logical shift right
			retVal = (dest << 1) >> ((src & 0x07) + 1);	// leave space for carry flag
			PSW.field.C = retVal & 0x01;
			retVal = (retVal >> 1) & 0xff;
			break;

		case _ALU_SRA:
			// arithmetic shift right
			shifter = dest << 8;		// use arithmetic shift of host processor
			shifter >>= ((src & 0x07) + 7);	// leave space for carry flag
			PSW.field.C = shifter & 0x01;
			retVal = (shifter >> 1) & 0xff;
			break;

		case _ALU_DAA:
			// decimal adjustment for addition
			// Uh gosh this is so much of a pain
			// reference: AMD64 general purpose and system instructions
			retVal = dest;
			// lower nibble
			if( PSW.field.HC || ((retVal & 0x0f) > 0x09) ) {
				retVal += 0x06;
				PSW.field.HC = 1;
			}
			else {
				PSW.field.HC = 0;
			}
			// higher nibble
			if( PSW.field.C || ((retVal & 0xf0) > 0x90) ) {
				retVal += 0x60;
				PSW.field.C = 1;	// carry should always be set
			}
			else {
				PSW.field.C = 0;
			}
			PSW.field.S = SIGN8(retVal);
			PSW.field.Z = IS_ZERO(retVal);
			break;

		case _ALU_DAS:
			// decimal adjustment for subtraction
			// This is even more confusing than DAA
			// reference: AMD64 general purpose and system instructions
			retVal = dest;
			// lower nibble
			if( PSW.field.HC || ((retVal & 0x0f) > 0x09) ) {
				retVal -= 0x06;
				PSW.field.HC = 1;
			}
			else {
				PSW.field.HC = 0;
			}
			// higher nibble
			if( PSW.field.C || ((retVal & 0xf0) > 0x90) ) {
				retVal -= 0x60;
				PSW.field.C = 1;	// carry should always be set
			}
			else {
				PSW.field.C = 0;
			}
			// write back
			PSW.field.S = SIGN8(retVal);
			PSW.field.Z = IS_ZERO(retVal);
			break;

		case _ALU_NEG:
			// 8-bit negate
			retVal = dest & 0xff;
			PSW.field.HC = (retVal & 0x0f)? 0 : 1;	// ~0b0000 + 1 = 0b1_0000
			PSW.field.C = retVal? 0 : 1;			// ~0b0000_0000 + 1 = 0b1_0000_0000
			PSW.field.OV = ((retVal & 0x7f)? 0 : 1) ^ PSW.field.C;
			retVal = (~retVal + 1) & 0xff;
			PSW.field.S = SIGN8(retVal);
			PSW.field.Z = IS_ZERO(retVal);
			break;

		case _ALU_SB:
			// set bit
			src = 0x01 << (src & 0x07);
			PSW.field.Z = IS_ZERO(dest & src);
			dest |= src;
			break;

		case _ALU_TB:
			// test bit
			src = 0x01 << (src & 0x07);
			PSW.field.Z = IS_ZERO(dest & src);
			break;

		case _ALU_RB:
			// reset bit
			src = 0x01 << (src & 0x07);
			PSW.field.Z = IS_ZERO(dest & src);
			dest &= ~src;
			break;
	}

	return retVal;
}


// Pushes a value onto U8 stack
// Note that it modifies SP
static void _pushValue(uint64_t value, uint8_t bytes) {
	Data_t tempData = {.raw = value};
	int i = 0;
	bytes = ((bytes > 8)? 8 : bytes);
	if( bytes & 0x01 )
		--SP;
	SP -= bytes;
	while( bytes-- > 0 ) {
		memorySetData(0, SP + i++, 1, tempData);
		tempData.raw >>= 8;
	}
}

// Pops a value from U8 stack
// Note that it modifies SP
static Data_t _popValue(uint8_t bytes) {
	Data_t tempData = {.raw = 0};
	EA_t adj = (bytes + 1) & 0xfffe;
	bytes = ((bytes > 8)? 8 : bytes);
	while( bytes-- > 0 ) {
		tempData.raw <<= 8;
		memoryGetData(0, SP + bytes, 1);
		tempData.byte = DataRaw.byte;
	}
	SP += adj;
	return tempData;
}


// Gets pointer to current EPSW
static PSW_t* _getCurrEPSW(void) {
	switch( PSW.field.ELevel ) {
		case 2:
			return &EPSW2;
		case 3:
			return &EPSW3;
		default:
			return &EPSW1;
	}
}

// Gets pointer to current ELR
static PC_t* _getCurrELR(void) {
	switch( PSW.field.ELevel ) {
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
static SR_t* _getCurrECSR(void) {
	switch( PSW.field.ELevel ) {
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


// Sign extends an n-bit integer
static uint16_t _signExtend(uint16_t num, uint8_t bits) {
	int16_t retVal = num << (16 - bits);
	return (retVal >> (16 - bits));
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
	printf("\t\tCode words at CSR:PC: %04X %04X\n", *(uint16_t*)(CodeMemory + (CSR << 16) + PC), *(uint16_t*)(CodeMemory + (CSR << 16) + PC + 2));
	printf("\tSP = %04Xh\n", SP);
	printf("\tDSR = %02Xh\n", DSR);
	printf("\tEA = %04Xh\n", EA);
	printf("\tPSW = %02Xh\n", PSW.raw);
	printf("\t\tC Z S V I H MIE\n\t\t%1d %1d %1d %1d %1d %1d  %1d\n", PSW.field.C, PSW.field.Z, PSW.field.S, PSW.field.OV, PSW.field.MIE, PSW.field.HC, PSW.field.ELevel);

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
	// xxxx_dddd_ssss_xxxx
	// x: decodeIndex; d: Dest, s: src
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

			PSW.field.Z = IS_ZERO(immNum);
			PSW.field.S = SIGN8(immNum);
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

			PSW.field.Z = IS_ZERO(src);
			PSW.field.S = SIGN8(src);

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
			PSW.field.C = (dest & 0x100)? 1 : 0;

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
			PSW.field.C = dest & 0x01;
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

				PSW.field.S = SIGN8(dest);
				PSW.field.Z = IS_ZERO(dest);

				GR.rs[regNumDest] = PSW.field.S? 0xff : 0;
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

			PSW.field.S = SIGN8(dest);
			PSW.field.Z = IS_ZERO(dest);
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
					case 0x9011:
						// ST Rn, [adr]
						// fetch destination address
						memoryGetCodeWord(CSR, PC);
						dest = CodeWord;
						CycleCount += EAIncDelay;
						PC = (PC + 2) & 0xfffe;
						break;

					case 0x9031:
						// ST Rn, [EA]
						dest = EA;
						break;

					case 0x9051:
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

			PSW.field.S = SIGN16(dest);
			PSW.field.Z = IS_ZERO(dest);
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

			PSW.field.S = SIGN32(dest);
			PSW.field.Z = IS_ZERO(dest);
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

			PSW.field.S = SIGN64(dest);
			PSW.field.Z = IS_ZERO(dest);
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
			PSW.field.C = (dest & 0x100)? 1 : 0;

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
			PSW.field.C = dest & 0x01;
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
			if( PSW.field.ELevel != 0 )
				GR.rs[regNumDest] = _getCurrEPSW()->raw;
			CycleCount = 2;
			break;

		case 0xa5:
			if( (CodeWord & 0x01f0) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// MOV ERn, ELR
			GR.ers[regNumDest] = *_getCurrELR();
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
			GR.rs[regNumDest] = *_getCurrECSR();
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
			if( (CodeWord & 0x01f0) == 0x0010 ) {
				// MOV ERn, SP
				GR.ers[regNumDest >> 1] = SP;
				CycleCount = 1;
				break;
			}
			if( (CodeWord & 0x0f10) == 0x0100 ) {
				// MOV SP, ERm
				SP = GR.ers[regNumSrc >> 1];
				CycleCount = 1;
				break;
			}
			retVal = CORE_ILLEGAL_INSTRUCTION;
			break;

		case 0xab:
			if( (CodeWord & 0x0f00) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// MOV PSW, Rm
			PSW.raw = GR.rs[regNumSrc];
			CycleCount = 1;
			break;

		case 0xac:
			if( (CodeWord & 0x0f00) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// MOV EPSW, Rm
			_getCurrEPSW()->raw = GR.rs[regNumSrc];
			CycleCount = 2;
			break;

		case 0xad:
			if( (CodeWord & 0x01f0) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// MOV ELR, ERm
			*_getCurrELR() = GR.ers[regNumDest >> 1];
			CycleCount = 3;
			break;

		case 0xae:
			// MOV CRn, Rm
			retVal = CORE_UNIMPLEMENTED;
			break;

		case 0xaf:
			if( (CodeWord & 0x0f00) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// MOV ECSR, Rm
			*_getCurrECSR() = GR.rs[regNumSrc];
			CycleCount = 2;
			break;

		case 0xb0:
		case 0xb1:
		case 0xb2:
		case 0xb3:
		case 0xb4:
		case 0xb5:
		case 0xb6:
		case 0xb7:
		case 0xb8:
		case 0xb9:
		case 0xba:
		case 0xbb:
		case 0xbc:
		case 0xbd:
		case 0xbe:
		case 0xbf:
			switch( CodeWord & 0x01c0 ) {
				case 0x0000:
					// L ERn, disp6[BP]
					src = GR.ers[12 >> 1];		// src = ER12
					src = (src + _signExtend(CodeWord & 0x003f, 6)) & 0xffff;
					memoryGetData(GET_DATA_SEG, src, 2);
					GR.ers[regNumDest >> 1] = DataRaw.word;
					CycleCount += ROMWinAccessCount;
					break;

				case 0x0040:
					// L ERn, disp6[FP]
					src = GR.ers[14 >> 1];		// src = ER14
					src = (src + _signExtend(CodeWord & 0x003f, 6)) & 0xffff;
					memoryGetData(GET_DATA_SEG, src, 2);
					GR.ers[regNumDest >> 1] = DataRaw.word;
					CycleCount += ROMWinAccessCount;
					break;

				case 0x0080:
					// ST ERn, disp6[BP]
					dest = GR.ers[12 >> 1];		// dest = ER12
					dest = (dest + _signExtend(CodeWord & 0x003f, 6)) & 0xffff;
					tempData.word = GR.ers[regNumDest >> 1];
					memorySetData(GET_DATA_SEG, dest, 2, tempData);
					break;

				case 0x00c0:
					// ST ERn, disp6[FP]
					dest = GR.ers[14 >> 1];		// dest = ER14
					dest = (dest + _signExtend(CodeWord & 0x003f, 6)) & 0xffff;
					tempData.word = GR.ers[regNumDest >> 1];
					memorySetData(GET_DATA_SEG, dest, 2, tempData);
					break;

				default:
					retVal = CORE_ILLEGAL_INSTRUCTION;
					goto exit;
			}
			CycleCount += 3 + EAIncDelay;
			break;

		case 0xc0:
		case 0xc1:
		case 0xc2:
		case 0xc3:
		case 0xc4:
		case 0xc5:
		case 0xc6:
		case 0xc7:
		case 0xc8:
		case 0xc9:
		case 0xca:
		case 0xcb:
		case 0xcc:
		case 0xcd:
		case 0xce:
		case 0xcf:
			switch( CodeWord & 0x0f00 ) {
				case 0x0000:
					// GE
					src = !PSW.field.C;
					break;
				case 0x0100:
					// LT
					src = PSW.field.C;
					break;
				case 0x0200:
					// GT
					src = !(PSW.field.C | PSW.field.Z);
					break;
				case 0x0300:
					// LE
					src = PSW.field.C | PSW.field.Z;
					break;
				case 0x0400:
					// GES
					src = !(PSW.field.OV ^ PSW.field.S);
					break;
				case 0x0500:
					// LTS
					src = PSW.field.OV ^ PSW.field.S;
					break;
				case 0x0600:
					// GTS
					src = !((PSW.field.OV ^ PSW.field.S) | PSW.field.Z);
					break;
				case 0x0700:
					// LES
					src = (PSW.field.OV ^ PSW.field.S) | PSW.field.Z;
					break;
				case 0x0800:
					// NE
					src = !PSW.field.Z;
					break;
				case 0x0900:
					// EQ
					src = PSW.field.Z;
					break;
				case 0x0a00:
					// NV
					src = !PSW.field.OV;
					break;
				case 0x0b00:
					// OV
					src = PSW.field.OV;
					break;
				case 0x0c00:
					// PS
					src = !PSW.field.S;
					break;
				case 0x0d00:
					// NS
					src = PSW.field.S;
					break;
				case 0x0e00:
					// AL
					src = 1;
					break;
				case 0x0f00:
					retVal = CORE_ILLEGAL_INSTRUCTION;
					goto exit;
			}
			CycleCount = 1;
			if( src ) {
				PC += (_signExtend(immNum, 8) << 1) & 0xffff;
				CycleCount = 3;
			}
			break;

		case 0xd0:
		case 0xd1:
		case 0xd2:
		case 0xd3:
		case 0xd4:
		case 0xd5:
		case 0xd6:
		case 0xd7:
		case 0xd8:
		case 0xd9:
		case 0xda:
		case 0xdb:
		case 0xdc:
		case 0xdd:
		case 0xde:
		case 0xdf:
			switch( CodeWord & 0x00c0 ) {
				case 0x0000:
					// L Rn, disp6[BP]
					src = GR.ers[12 >> 1];		// src = ER12
					src = (src + _signExtend(CodeWord & 0x003f, 6)) & 0xffff;
					memoryGetData(GET_DATA_SEG, src, 1);
					GR.rs[regNumDest] = DataRaw.byte;
					CycleCount += ROMWinAccessCount;
					break;

				case 0x0040:
					// L Rn, disp6[FP]
					src = GR.ers[14 >> 1];		// src = ER14
					src = (src + _signExtend(CodeWord & 0x003f, 6)) & 0xffff;
					memoryGetData(GET_DATA_SEG, src, 1);
					GR.rs[regNumDest >> 1] = DataRaw.byte;
					CycleCount += ROMWinAccessCount;
					break;

				case 0x0080:
					// ST Rn, disp6[BP]
					dest = GR.ers[12 >> 1];		// dest = ER12
					dest = (dest + _signExtend(CodeWord & 0x003f, 6)) & 0xffff;
					tempData.byte = GR.rs[regNumDest];
					memorySetData(GET_DATA_SEG, dest, 1, tempData);
					break;

				case 0x00c0:
					// ST Rn, disp6[FP]
					dest = GR.ers[14 >> 1];		// dest = ER14
					dest = (dest + _signExtend(CodeWord & 0x003f, 6)) & 0xffff;
					tempData.byte = GR.rs[regNumDest];
					memorySetData(GET_DATA_SEG, dest, 1, tempData);
					break;

				default:
					retVal = CORE_ILLEGAL_INSTRUCTION;
					goto exit;
			}
			CycleCount += 3 + EAIncDelay;
			break;

		case 0xe0:
		case 0xe1:
		case 0xe2:
		case 0xe3:
		case 0xe4:
		case 0xe5:
		case 0xe6:
		case 0xe7:
		case 0xe8:
		case 0xe9:
		case 0xea:
		case 0xeb:
		case 0xec:
		case 0xed:
		case 0xee:
		case 0xef:
			// I don't like it when it has immediates on the end of the instructions
			if( (CodeWord & 0x0180) == 0x0000 ) {
				// MOV ERn, #imm7
				src = _signExtend(CodeWord & 0x007f, 7);
				GR.ers[regNumDest >> 1] = src;
				PSW.field.Z = IS_ZERO(src);
				PSW.field.S = SIGN16(src);
				CycleCount = 2;
				break;
			}
			if( (CodeWord & 0x0180) == 0x0080 ) {
				// ADD ERn, #imm7
				src = _signExtend(CodeWord & 0x007f, 7);
//				printf("\nADD ERn, #imm7 | ERn = %04X, #imm7 = %04X\n", GR.ers[regNumDest >> 1], _signExtend(CodeWord & 0x007f, 7));
				GR.ers[regNumDest >> 1] = _ALU(GR.ers[regNumDest >> 1], src, _ALU_ADD_W);
				CycleCount = 2;
				break;
			}
			switch( CodeWord & 0x0f00 ) {
				case 0x0100:
					// ADD SP, #signed8
					SP = SP + _signExtend(immNum, 8);
					CycleCount = 2;
					break;

				case 0x0300:
					// _LDSR #imm8
					DSR = immNum;
					isDSRSet = true;
					CycleCount = 1;
					break;

				case 0x0500:
					// SWI #snum
					retVal = CORE_UNIMPLEMENTED;
					break;

				case 0x0900:
					// MOV PSW, #unsigned8
					PSW.raw = immNum;
					CycleCount = 1;
					break;

				case 0x0b00:
					if( CodeWord == 0xfb7f ) {
						// RC
						PSW.field.C = 0;
						CycleCount = 1;
						break;
					}
					if( CodeWord == 0xfbf7 ) {
						// DI
						PSW.field.MIE = 0;
						CycleCount = 3;
						break;
					}

					retVal = CORE_ILLEGAL_INSTRUCTION;
					break;

				case 0x0d00:
					if( CodeWord == 0xfd08 ) {
						// EI
						PSW.field.MIE = 1;
						CycleCount = 1;
						// Todo: Disable maskable interrupts for 2 cycles
						break;
					}
					if( CodeWord == 0xfd80 ) {
						// SC
						PSW.field.C = 1;
						CycleCount = 1;
						break;
					}

					retVal = CORE_ILLEGAL_INSTRUCTION;
					break;
			}
			break;

		case 0xf0:
			if( (CodeWord & 0x00f0) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// B Cadr
			memoryGetCodeWord(CSR, PC);
			PC = CodeWord & 0xfffe;
			CSR = regNumDest;
			CycleCount = 2 + EAIncDelay;
			break;

		case 0xf1:
			if( (CodeWord & 0x00f0) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// BL Cadr
			memoryGetCodeWord(CSR, PC);
			LR = (PC + 2) & 0xfffe;
			LCSR = CSR;
			PC = CodeWord & 0xfffe;
			CSR = regNumDest;
			CycleCount = 2 + EAIncDelay;
			break;

		case 0xf2:
			if( (CodeWord & 0x0f10) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// B ERn
			PC = GR.ers[regNumSrc >> 1] & 0xfffe;
			CycleCount = 2 + EAIncDelay;
			break;

		case 0xf3:
			if( (CodeWord & 0x0f10) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// BL ERn
			LR = (PC + 2) & 0xfffe;
			LCSR = CSR;
			PC = GR.ers[regNumSrc >> 1] & 0xfffe;
			CycleCount = 2 + EAIncDelay;
			break;

		case 0xf4:
			if( (CodeWord & 0x0100) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// MUL ERn, Rm
			CycleCount = 8;
			dest = GR.rs[regNumDest] * GR.rs[regNumSrc];
			PSW.field.Z = IS_ZERO(dest);
			GR.ers[regNumDest >> 1] = dest & 0xffff;
			break;

		case 0xf5:
			if( (CodeWord & 0x0110) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// MOV ERn, ERm
			CycleCount = 2;
			dest = GR.ers[regNumSrc >> 1];
			PSW.field.Z = IS_ZERO(dest);
			PSW.field.S = SIGN16(dest);
			GR.ers[regNumDest >> 1] = dest;
			break;

		case 0xf6:
			if( (CodeWord & 0x0110) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// ADD ERn, ERm
			CycleCount = 2;
			GR.ers[regNumDest >> 1] = _ALU(GR.ers[regNumDest >> 1], GR.ers[regNumSrc >> 1], _ALU_ADD_W);
			break;

		case 0xf7:
			if( (CodeWord & 0x0110) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// CMP ERn, ERm
			CycleCount = 2;
			GR.ers[regNumDest >> 1] = _ALU(GR.ers[regNumDest >> 1], GR.ers[regNumSrc >> 1], _ALU_CMP_W);
			break;

		case 0xf9:
			if( (CodeWord & 0x0100) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// DIV ERn, Rm
			dest = GR.ers[regNumDest >> 1];
			src = GR.rs[regNumSrc];
			CycleCount = 16;
			PSW.field.Z = IS_ZERO(dest);
			PSW.field.C = 0;
			if( src == 0 ) {
				// Divisor is 0
				PSW.field.C = 1;
				GR.rs[regNumSrc] = dest & 0xff;		// remainder
				GR.ers[regNumDest >> 1] = 0xffff;	// result
				break;
			}
			// Else both number are not zero
			GR.rs[regNumSrc] = (dest % src) & 0xff;
			GR.ers[regNumDest] = (dest / src) & 0xffff;
			break;

		case 0xfa:
			if( (CodeWord & 0x0010) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// LEA [ERm]
			EA = GR.ers[regNumSrc >> 1];
			CycleCount = 1;
			break;

		case 0xfb:
			if( (CodeWord & 0x0010) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// LEA disp16[ERm]
			dest = GR.ers[regNumSrc >> 1];
			memoryGetCodeWord(CSR, PC);
			PC = (PC + 2) & 0xfffe;
			EA = (CodeWord + dest) & 0xffff;
			CycleCount = 2;
			break;

		case 0xfc:
			if( (CodeWord & 0x0010) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// LEA Dadr
			memoryGetCodeWord(CSR, PC);
			PC = (PC + 2) & 0xfffe;
			EA = CodeWord;
			CycleCount = 2;
			break;

		case 0xfd:
			// MOV CRn, [EA]
			// MOV CRn, [EA+]
			// MOV CERn, [EA]
			// MOV CERn, [EA+]
			// MOV CXRn, [EA]
			// MOV CXRn, [EA+]
			// MOV CQRn, [EA]
			// MOV CQRn, [EA+]
			// MOV [EA], CRm
			// MOV [EA+], CRm
			// MOV [EA], CERm
			// MOV [EA+], CERm
			// MOV [EA], CXRm
			// MOV [EA+], CXRm
			// MOV [EA], CQRm
			// MOV [EA+], CQRm
			retVal = CORE_UNIMPLEMENTED;
			break;

		case 0xfe:
			switch( CodeWord & 0x00f0 ) {
				case 0x0000:
					// POP Rn
					GR.rs[regNumDest] = _popValue(1).byte;
					CycleCount = 2 + EAIncDelay;
					break;

				case 0x0010:
					// POP ERn
					if( (regNumDest & 0x01) != 0x00 ) {
						retVal = CORE_ILLEGAL_INSTRUCTION;
						break;
					}
					GR.ers[regNumDest >> 1] = _popValue(2).word;
					CycleCount = 2 + EAIncDelay;
					break;

				case 0x0020:
					// POP XRn
					if( (regNumDest & 0x03) != 0x00 ) {
						retVal = CORE_ILLEGAL_INSTRUCTION;
						break;
					}
					GR.xrs[regNumDest >> 2] = _popValue(4).dword;
					CycleCount = 4 + EAIncDelay;
					break;

				case 0x0030:
					// POP QRn
					if( (regNumDest & 0x07) != 0x00 ) {
						retVal = CORE_ILLEGAL_INSTRUCTION;
						break;
					}
					GR.qrs[regNumDest >> 3] = _popValue(8).qword;
					CycleCount = 8 + EAIncDelay;
					break;

				case 0x0040:
					// PUSH Rn
					_pushValue(GR.rs[regNumDest], 1);
					CycleCount = 2 + EAIncDelay;
					break;

				case 0x0050:
					// PUSH ERn
					if( (regNumDest & 0x01) != 0x00 ) {
						retVal = CORE_ILLEGAL_INSTRUCTION;
						break;
					}
					_pushValue(GR.ers[regNumDest >> 1], 2);
					CycleCount = 2 + EAIncDelay;
					break;

				case 0x0060:
					// PUSH XRn
					if( (regNumDest & 0x03) != 0x00 ) {
						retVal = CORE_ILLEGAL_INSTRUCTION;
						break;
					}
					_pushValue(GR.xrs[regNumDest >> 2], 4);
					CycleCount = 4 + EAIncDelay;
					break;

				case 0x0070:
					// PUSH QRn
					if( (regNumDest & 0x07) != 0x00 ) {
						retVal = CORE_ILLEGAL_INSTRUCTION;
						break;
					}
					_pushValue(GR.qrs[regNumDest >> 3], 8);
					CycleCount = 8 + EAIncDelay;
					break;

				case 0x0080:
					// POP lepa
					// Assume LARGE model (with CSR)
					if( regNumDest & 0x01 ) {
						// EA
						EA = _popValue(2).word;
						CycleCount += 2;
					}
					if( regNumDest & 0x08 ) {
						// LR
						LR = _popValue(2).word;
						LCSR = _popValue(1).byte;
						CycleCount += 4;
					}
					if( regNumDest & 0x04 ) {
						// PSW
						PSW.raw = _popValue(1).byte;
						CycleCount += 2;
					}
					if( regNumDest & 0x02 ) {
						// PC
						PC = _popValue(2).word & 0xfffe;
						CSR = _popValue(1).byte;
						CycleCount += 7;
					}
					if( CycleCount )
						CycleCount = 1;		// Assume 1 cycle if no register
					else
						CycleCount += EAIncDelay;
					break;

				case 0x00c0:
					// PUSH lepa
					// Assume LARGE model (with CSR)
					if( regNumDest & 0x02 ) {
						// ELR
						_pushValue(*_getCurrECSR(), 1);
						_pushValue(*_getCurrELR(), 2);
						CycleCount += 4;
					}
					if( regNumDest & 0x04 ) {
						// EPSW
						_pushValue(_getCurrEPSW()->raw, 1);
						CycleCount += 2;
					}
					if( regNumDest & 0x08 ) {
						// LR
						_pushValue(CSR, 1);
						_pushValue(LR, 2);
						CycleCount += 4;
					}
					if( regNumDest & 0x01 ) {
						// EA
						_pushValue(EA, 2);
						CycleCount += 2;
					}
					if( CycleCount )
						CycleCount = 1;		// Assume 1 cycle if no register
					else
						CycleCount += EAIncDelay;
					break;

				default:
					retVal = CORE_ILLEGAL_INSTRUCTION;
					break;

			}
			break;

		case 0xff:
			switch( CodeWord ) {
				case 0xfe0f:
					// RTI
					CSR = *_getCurrECSR();
					PC = *_getCurrELR();
					PSW.raw = _getCurrEPSW()->raw;
					CycleCount = 2 + EAIncDelay;
					break;

				case 0xfe1f:
					// RT
					CSR = LCSR;
					PC = LR;
					CycleCount = 2 + EAIncDelay;
					break;

				case 0xfe2f:
					// INC [EA]
					memoryGetData(GET_DATA_SEG, EA, 1);
					tempData.byte = _ALU(tempData.byte, 1, _ALU_ADD);
					memorySetData(GET_DATA_SEG, EA, 1, tempData);
					CycleCount = 2 + EAIncDelay;
					break;

				case 0xfe3f:
					// DEC [EA]
					memoryGetData(GET_DATA_SEG, EA, 1);
					tempData.byte = _ALU(tempData.byte, 1, _ALU_SUB);
					memorySetData(GET_DATA_SEG, EA, 1, tempData);
					CycleCount = 2 + EAIncDelay;
					break;

				case 0xfe8f:
					// NOP
					CycleCount = 1;
					break;

				case 0xfe9f:
					// _UDSR
					isDSRSet = true;
					CycleCount = 1;
					break;

				case 0xfecf:
					// CPLC
					PSW.field.C ^= 1;
					CycleCount = 1;
					break;

				case 0xffff:
					// BRK
					retVal = CORE_UNIMPLEMENTED;
					break;

				default:
					retVal = CORE_ILLEGAL_INSTRUCTION;
					break;

			}
			break;

		default:
			retVal = CORE_ILLEGAL_INSTRUCTION;
			break;
	}


	if( retVal == CORE_OK ) {
		EAIncDelay = isEAInc? 1 : 0;
		NextAccess = isDSRSet? DATA_ACCESS_DSR : DATA_ACCESS_PAGE0;

		if( (IntMaskCycle -= CycleCount) < 0 )
			IntMaskCycle = 0;

		// Mask interrupts if DSR prefix instruction is used
		if( isDSRSet && (IntMaskCycle == 0) )
			++IntMaskCycle;
		}

exit:
	return retVal;
}
