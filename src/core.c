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


// ALU operations, modifies PSW
// A place to hide all the ugly code behind the scene
// If only I could use the flags of the host CPU...

// 8-bit addition
static uint8_t _ALU_ADD(register uint8_t dest, register uint8_t src) {
	uint16_t retVal = dest + src;
	PSW.field.C = (retVal & 0x100)? 1 : 0;
	retVal &= 0xff;
	PSW.field.Z = IS_ZERO(retVal);
	PSW.field.S = SIGN8(retVal);
	// reference: Z80 user manual
	PSW.field.OV = (((dest & 0x7f) + (src & 0x7f)) >> 7) ^ PSW.field.C;
	PSW.field.HC = (((dest & 0x0f) + (src & 0x0f)) & 0x10)? 1 : 0;
	return (uint8_t)retVal;
}

// 16-bit addition
static uint16_t _ALU_ADD_W(register uint16_t dest, register uint16_t src) {
	uint32_t retVal = dest + src;
	PSW.field.C = (retVal & 0x10000)? 1 : 0;
	retVal &= 0xffff;
	PSW.field.Z = IS_ZERO(retVal);
	PSW.field.S = SIGN16(retVal);
	// reference: Z80 user manual
	PSW.field.OV = (((dest & 0x7fff) + (src & 0x7fff)) >> 15) ^ PSW.field.C;
	PSW.field.HC = (((dest & 0x0fff) + (src & 0x0fff)) & 0x1000)? 1 : 0;
	return (uint16_t)retVal;
}

// 8-bit addition with carry
static uint8_t _ALU_ADDC(register uint8_t dest, register uint8_t src) {
	uint16_t retVal = dest + src + PSW.field.C;
	PSW.field.C = (retVal & 0x100)? 1 : 0;
	retVal &= 0xff;
	PSW.field.Z = PSW.field.Z & IS_ZERO(retVal);
	PSW.field.S = SIGN8(retVal);
	// reference: Z80 user manual
	PSW.field.OV = (((dest & 0x7f) + (src & 0x7f) + PSW.field.C) >> 7) ^ PSW.field.C;
	PSW.field.HC = (((dest & 0x0f) + (src & 0x0f) + PSW.field.C) & 0x10)? 1 : 0;
	return (uint8_t)retVal;
}

// 8-bit logical AND
static inline uint8_t _ALU_AND(register uint8_t dest, register uint8_t src) {
	uint8_t retVal = dest & src;
	PSW.field.Z = IS_ZERO(retVal);
	PSW.field.S = SIGN8(retVal);
	return retVal;
}

// 8-bit logical OR
static inline uint8_t _ALU_OR(register uint8_t dest, register uint8_t src) {
	uint8_t retVal = dest | src;
	PSW.field.Z = IS_ZERO(retVal);
	PSW.field.S = SIGN8(retVal);
	return retVal;
}

// 8-bit logical XOR
static inline uint8_t _ALU_XOR(register uint8_t dest, register uint8_t src) {
	uint8_t retVal = dest ^ src;
	PSW.field.Z = IS_ZERO(retVal);
	PSW.field.S = SIGN8(retVal);
	return retVal;
}

// 16-bit comparison
static void _ALU_CMP_W(register uint16_t dest, register uint16_t src) {
	uint32_t retVal = dest - src;
	PSW.field.C = (retVal & 0x10000)? 1 : 0;
	retVal &= 0xffff;
	PSW.field.Z = IS_ZERO(retVal);
	PSW.field.S = SIGN16(retVal);
	// reference: Z80 user manual
	PSW.field.OV = (((dest & 0x7fff) - (src & 0x7fff)) >> 15) ^ PSW.field.C;
	PSW.field.HC = (((dest & 0x0fff) - (src & 0x0fff)) & 0x10)? 1 : 0;
}

// 8-bit comparison & subtraction
static uint8_t _ALU_SUB(register uint8_t dest, register uint8_t src) {
	uint16_t retVal = dest - src;
	PSW.field.C = (retVal & 0x100)? 1 : 0;
	retVal &= 0xff;
	PSW.field.Z = IS_ZERO(retVal);
	PSW.field.S = SIGN8(retVal);
	// reference: Z80 user manual
	PSW.field.OV = (((dest & 0x7f) - (src & 0x7f)) >> 7) ^ PSW.field.C;
	PSW.field.HC = (((dest & 0x0f) - (src & 0x0f)) & 0x10)? 1 : 0;
	return (uint8_t)retVal;
}

#define _ALU_CMP(d, s) _ALU_SUB(d, s)


// 8-bit comparison & subtraction with carry
static uint8_t _ALU_SUBC(register uint8_t dest, register uint8_t src) {
	uint16_t retVal = dest - src - PSW.field.C;
	PSW.field.C = (retVal & 0x100)? 1 : 0;
	retVal &= 0xff;
	PSW.field.Z = PSW.field.Z & IS_ZERO(retVal);
	PSW.field.S = SIGN8(retVal);
	// reference: Z80 user manual
	PSW.field.OV = (((dest & 0x7f) - (src & 0x7f) - PSW.field.C) >> 7) ^ PSW.field.C;
	PSW.field.HC = (((dest & 0x0f) - (src & 0x0f) - PSW.field.C) & 0x10)? 1 : 0;
	return (uint8_t)retVal;
}

#define _ALU_CMPC(d, s) _ALU_SUBC(d, s)

// 8-bit logical left shift
static uint8_t _ALU_SLL(register uint8_t data, register uint8_t count) {
	uint16_t retVal = data << (count & 0x07);
	if( count == 0 ) {
		return data;
	}
	PSW.field.C = (retVal & 0x100)? 1 : 0;
	return (uint8_t)(retVal & 0xff);
}

// 8-bit logical right shift
static uint8_t _ALU_SRL(register uint8_t data, register uint8_t count) {
	uint16_t retVal = (data << 1) >> (count & 0x07);	// leave space for carry flag
	if( count == 0 ) {
		return data;
	}
	PSW.field.C = retVal & 0x01;
	return (uint8_t)((retVal >> 1) & 0xff);
}

// 8-bit arithmetic right shift
static uint8_t _ALU_SRA(register uint8_t data, register uint8_t count) {
	int16_t retVal = data << 8;		// use arithmetic shift of host processor
	if( count == 0 ) {
		return data;
	}
	retVal >>= ((count & 0x07) + 7);	// leave space for carry flag
	PSW.field.C = retVal & 0x01;
	return (uint8_t)((retVal >> 1) & 0xff);
}

// decimal adjustment for addition
static inline uint8_t _ALU_DAA(register uint8_t byte) {
	// Uh gosh this is so much of a pain
	// reference: AMD64 general purpose and system instructions

	// lower nibble
	if( PSW.field.HC || ((byte & 0x0f) > 0x09) ) {
		byte += 0x06;
		PSW.field.HC = 1;
	}
	else {
		PSW.field.HC = 0;
	}
	// higher nibble
	if( PSW.field.C || ((byte & 0xf0) > 0x90) || (byte & 0x100) ) {
		byte += 0x60;
		PSW.field.C = 1;	// carry should always be set
	}
	else {
		PSW.field.C = 0;
	}
	PSW.field.S = SIGN8(byte);
	PSW.field.Z = IS_ZERO(byte);
	return byte;
}

// decimal adjustment for subtraction
static inline uint8_t _ALU_DAS(register uint8_t byte) {
	// This is even more confusing than DAA
	// reference: AMD64 general purpose and system instructions

	// lower nibble
	if( PSW.field.HC || ((byte & 0x0f) > 0x09) ) {
		byte -= 0x06;
		PSW.field.HC = 1;
	}
	else {
		PSW.field.HC = 0;
	}
	// higher nibble
	if( PSW.field.C || ((byte & 0xf0) > 0x90) || (byte & 0x100) ) {
		byte -= 0x60;
		PSW.field.C = 1;	// carry should always be set
	}
	else {
		PSW.field.C = 0;
	}
	// write back
	PSW.field.S = SIGN8(byte);
	PSW.field.Z = IS_ZERO(byte);
	return byte;
}

static inline uint8_t _ALU_NEG(register uint8_t byte) {
	PSW.field.HC = (byte & 0x0f)? 1 : 0;
	PSW.field.C = byte? 1 : 0;
	PSW.field.OV = (byte == 0x80)? 1 : 0;		// (int8_t)0x80 == -128
	byte = (0 - byte) & 0xff;
	PSW.field.S = SIGN8(byte);
	PSW.field.Z = IS_ZERO(byte);
	return byte;
}

// set bit
static uint8_t _ALU_SB(register uint8_t data, register uint8_t bit) {
	bit = 0x01 << (bit & 0x07);
	PSW.field.Z = IS_ZERO(data & bit);
	return (data | bit);
}

// test bit
static void _ALU_TB(register uint8_t data, register uint8_t bit) {
	bit = 0x01 << (bit & 0x07);
	PSW.field.Z = IS_ZERO(data & bit);
}

// reset bit
static uint8_t _ALU_RB(register uint8_t data, register uint8_t bit) {
	bit = 0x01 << (bit & 0x07);
	PSW.field.Z = IS_ZERO(data & bit);
	return (data & ~bit);
}


// Pushes a value onto U8 stack
// Note that it modifies SP
static void _pushValue(uint64_t value, uint8_t bytes) {
	int i = 0;
	bytes = ((bytes > 8)? 8 : bytes);
	if( bytes & 0x01 )
		--SP;
	SP -= bytes;
	while( bytes-- > 0 ) {
		memorySetData(0, SP + i++, 1, value);
		value >>= 8;
	}
}

// Pops a value from U8 stack
// Note that it modifies SP
static uint64_t _popValue(uint8_t bytes) {
	uint64_t retVal = 0;
	EA_t adj = (bytes + 1) & 0xfffe;
	bytes = ((bytes > 8)? 8 : bytes);
	while( bytes-- > 0 ) {
		retVal = (retVal << 8) | memoryGetData(0, SP + bytes, 1);
	}
	SP += adj;
	return retVal;
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
	SP = memoryGetCodeWord((SR_t)0, (PC_t)0x0000);

	// initialize PC
	PC = memoryGetCodeWord((SR_t)0, (PC_t)0x0002);

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

	return CORE_OK;
}


CORE_STATUS coreStep(void) {
	CORE_STATUS retVal = CORE_OK;
	CycleCount = 0;
	// xxxx_dddd_ssss_xxxx
	// x: decodeIndex; d: Dest, s: src
	uint8_t decodeIndex, regNumDest, regNumSrc, immNum;
	uint16_t codeWord;
	bool isEAInc = false;
	bool isDSRSet = false;

	uint64_t dest = 0, src = 0;

	if( IsMemoryInited == false ) {
		retVal = CORE_MEMORY_UNINITIALIZED;
		goto exit;
	}

	// fetch instruction
	codeWord = memoryGetCodeWord(CSR, PC);
	PC = (PC + 2) & 0xfffe;		// increment PC

	decodeIndex = ((codeWord >> 8) & 0xf0) | (codeWord & 0x0f);
	regNumDest = (codeWord >> 8) & 0x0f;
	regNumSrc = (codeWord >> 4) & 0x0f;
	immNum = codeWord & 0xff;
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
			GR.rs[regNumDest] = _ALU_ADD(dest, immNum);
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
			dest = GR.rs[(codeWord >> 8) & 0x0f];
			GR.rs[regNumDest] = _ALU_AND(dest, immNum);
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
			GR.rs[regNumDest] = _ALU_OR(dest, immNum);
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
			GR.rs[regNumDest] = _ALU_XOR(dest, immNum);
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
			_ALU_CMPC(dest, immNum);
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
			GR.rs[regNumDest] = _ALU_ADDC(dest, immNum);
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
			_ALU_CMP(dest, immNum);
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
			GR.rs[regNumDest] = _ALU_ADD(dest, src);
			break;

		case 0x82:
			// AND Rn, Rm
			CycleCount = 1;
			dest = GR.rs[regNumDest];
			src = GR.rs[regNumSrc];
			GR.rs[regNumDest] = _ALU_AND(dest, src);
			break;

		case 0x83:
			// OR Rn, Rm
			CycleCount = 1;
			dest = GR.rs[regNumDest];
			src = GR.rs[regNumSrc];
			GR.rs[regNumDest] = _ALU_OR(dest, src);
			break;

		case 0x84:
			// XOR Rn, Rm
			CycleCount = 1;
			dest = GR.rs[regNumDest];
			src = GR.rs[regNumSrc];
			GR.rs[regNumDest] = _ALU_XOR(dest, src);
			break;

		case 0x85:
			// CMPC Rn, Rm
			CycleCount = 1;
			dest = GR.rs[regNumDest];
			src = GR.rs[regNumSrc];
			_ALU_CMPC(dest, src);
			break;

		case 0x86:
			// ADDC Rn, Rm
			CycleCount = 1;
			dest = GR.rs[regNumDest];
			src = GR.rs[regNumSrc];
			GR.rs[regNumDest] = _ALU_ADDC(dest, src);
			break;

		case 0x87:
			// CMP Rn, Rm
			CycleCount = 1;
			dest = GR.rs[regNumDest];
			src = GR.rs[regNumSrc];
			_ALU_CMP(dest, src);
			break;

		case 0x88:
			// SUB Rn, Rm
			CycleCount = 1;
			dest = GR.rs[regNumDest];
			src = GR.rs[regNumSrc];
			GR.rs[regNumDest] = _ALU_SUB(dest, src);
			break;

		case 0x89:
			// SUBC Rn, Rm
			CycleCount = 1;
			dest = GR.rs[regNumDest];
			src = GR.rs[regNumSrc];
			GR.rs[regNumDest] = _ALU_SUBC(dest, src);
			break;

		case 0x8a:
			// SLL Rn, Rm
			CycleCount = 1 + EAIncDelay;
			dest = GR.rs[regNumDest];
			src = GR.rs[regNumSrc];
			GR.rs[regNumDest] = _ALU_SLL(dest, src);
			break;

		case 0x8b:
			// SLLC Rn, Rm
			CycleCount = 1 + EAIncDelay;
			src = GR.rs[regNumSrc] & 0x07;
			if( src == 0 ) {
				break;
			}
			dest = (GR.rs[regNumDest] << 8) | GR.rs[(regNumDest - 1) & 0x0f];

			dest >>= (8 - src);
			PSW.field.C = (dest & 0x100)? 1 : 0;

			GR.rs[regNumDest] = (dest & 0xff);

			break;

		case 0x8c:
			// SRL Rn, Rm
			CycleCount = 1 + EAIncDelay;
			dest = GR.rs[regNumDest];
			src = GR.rs[regNumSrc];
			GR.rs[regNumDest] = _ALU_SRL(dest, src);
			break;

		case 0x8d:
			// SRLC Rn, Rm
			CycleCount = 1 + EAIncDelay;
			src = GR.rs[regNumSrc] & 0x07;
			if( src == 0 ) {
				break;
			}
			dest = (GR.rs[(regNumDest + 1) & 0x0f] << 9) | (GR.rs[regNumDest] << 1);	// bit 0 for carry

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
			GR.rs[regNumDest] = _ALU_SRA(dest, src);
			break;

		case 0x8f:
			if( (codeWord & 0xf11f) == 0x810f ) {
				//EXTBW ERn
				src = GR.rs[regNumSrc];

				PSW.field.S = SIGN8(src);
				PSW.field.Z = IS_ZERO(src);

				GR.rs[regNumDest] = PSW.field.S? 0xff : 0;
				break;
			}
			switch( codeWord & 0xf0ff ) {
				case 0x801f:
					// DAA Rn
					// Uh gosh this is so much of a pain
					// reference: AMD64 general purpose and system instructions
					CycleCount = 1;
					dest = GR.rs[regNumDest];
					GR.rs[regNumDest] = _ALU_DAA(dest);
					break;

				case 0x803f:
					// DAS Rn
					// This is even more confusing than DAA
					// reference: AMD64 general purpose and system instructions
					CycleCount = 1;
					dest = GR.rs[regNumDest];
					GR.rs[regNumDest] = _ALU_DAS(dest);
					break;

				case 0x805f:
					//NEG Rn
					CycleCount = 1;
					dest = GR.rs[regNumDest];
					GR.rs[regNumDest] = _ALU_NEG(dest);
					break;

				default:
					retVal = CORE_ILLEGAL_INSTRUCTION;
					break;
			}
			break;

		case 0x90:
			if( (codeWord & 0x0010) == 0x0000 ) {
				// L Rn, [ERm]
				src = GR.ers[regNumSrc >> 1];
				CycleCount += EAIncDelay;
			}
			else {
				switch( codeWord & 0xf0ff ) {
					case 0x9010:
						// L Rn, [adr]
						// fetch source address
						src = memoryGetCodeWord(CSR, PC);
						PC = (PC + 2) & 0xfffe;
						CycleCount += EAIncDelay;
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

			dest = memoryGetData(GET_DATA_SEG, src, 1);
			CycleCount += 1 + ROMWinAccessCount;

			PSW.field.S = SIGN8(dest);
			PSW.field.Z = IS_ZERO(dest);
			GR.rs[regNumDest] = dest;
			break;

		case 0x91:
			if( (codeWord & 0x0010) == 0x0000 ) {
				// ST Rn, [ERm]
				dest = GR.ers[regNumSrc >> 1];
				CycleCount += EAIncDelay;
			}
			else {
				switch( codeWord & 0xf0ff ) {
					case 0x9011:
						// ST Rn, [adr]
						// fetch destination address
						dest = memoryGetCodeWord(CSR, PC);
						PC = (PC + 2) & 0xfffe;
						CycleCount += EAIncDelay;
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

			memorySetData(GET_DATA_SEG, dest, 1, GR.rs[regNumDest]);
			CycleCount += 1;
			break;

		case 0x92:
			if( (codeWord & 0x0110) == 0x0000 ) {
				// L ERn, [ERm]
				src = GR.ers[regNumSrc >> 1];
				CycleCount += EAIncDelay;
			}
			else {
				switch( codeWord & 0xf1ff ) {
					case 0x9012:
						// L ERn, [adr]
						// fetch source address
						src = memoryGetCodeWord(CSR, PC);
						PC = (PC + 2) & 0xfffe;
						CycleCount += EAIncDelay;
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

			dest = memoryGetData(GET_DATA_SEG, src, 2);
			CycleCount += 2 + ROMWinAccessCount;

			PSW.field.S = SIGN16(dest);
			PSW.field.Z = IS_ZERO(dest);
			GR.ers[regNumDest >> 1] = dest;
			break;

		case 0x93:
			if( (codeWord & 0x0110) == 0x0000 ) {
				// ST ERn, [ERm]
				dest = GR.ers[regNumSrc >> 1];
				CycleCount += EAIncDelay;
			}
			else {
				switch( codeWord & 0xf1ff ) {
					case 0x9013:
						// ST ERn, [adr]
						// fetch destination address
						dest = memoryGetCodeWord(CSR, PC);
						PC = (PC + 2) & 0xfffe;
						CycleCount += EAIncDelay;
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

			memorySetData(GET_DATA_SEG, dest, 2, GR.ers[regNumDest >> 1]);
			CycleCount += 2;
			break;

		case 0x94:
			src = EA;
			switch( codeWord & 0xf3ff ) {
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

			dest = memoryGetData(GET_DATA_SEG, src, 4);
			CycleCount = 4 + ROMWinAccessCount;

			PSW.field.S = SIGN32(dest);
			PSW.field.Z = IS_ZERO(dest);
			GR.xrs[regNumDest >> 2] = dest;
			break;

		case 0x95:
			dest = EA;
			switch( codeWord & 0xf3ff ) {
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

			memorySetData(GET_DATA_SEG, dest, 4, GR.xrs[regNumDest >> 2]);
			CycleCount = 4;
			break;

		case 0x96:
			src = EA;
			switch( codeWord & 0xf7ff ) {
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

			dest = memoryGetData(GET_DATA_SEG, src, 8);
			CycleCount = 8 + ROMWinAccessCount;

			PSW.field.S = SIGN64(dest);
			PSW.field.Z = IS_ZERO(dest);
			GR.qrs[regNumDest >> 3] = dest;
			break;

		case 0x97:
			dest = EA;
			switch( codeWord & 0xf7ff ) {
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

			memorySetData(GET_DATA_SEG, dest, 8, GR.qrs[regNumDest >> 3]);
			CycleCount = 8;
			break;

		case 0x98:
			if( (codeWord & 0xf01f) != 0x9008 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// L Rn, d16[ERm]
			src = GR.ers[regNumSrc >> 1];
			src = (src + memoryGetCodeWord(CSR, PC)) & 0xffff;
			PC = (PC + 2) & 0xfffe;
			dest = memoryGetData(GET_DATA_SEG, src, 1);
			GR.rs[regNumDest] = dest;
			PSW.field.S = SIGN8(dest);
			PSW.field.Z = IS_ZERO(dest);
			CycleCount = 2 + ROMWinAccessCount + EAIncDelay;
			break;

		case 0x99:
			if( (codeWord & 0xf01f) != 0x9009 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// ST Rn, d16[ERm]
			dest = GR.ers[regNumSrc >> 1];
			dest = (dest + memoryGetCodeWord(CSR, PC)) & 0xffff;
			PC = (PC + 2) & 0xfffe;
			memorySetData(GET_DATA_SEG, dest, 1, GR.rs[regNumDest]);
			CycleCount = 2 + EAIncDelay;
			break;

		case 0x9a:
			if( (codeWord & 0x0080) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// SLL Rn, #width
			CycleCount = 1 + EAIncDelay;
			dest = GR.rs[regNumDest];
			src = regNumSrc;
			GR.rs[regNumDest] = _ALU_SLL(dest, src);
			break;

		case 0x9b:
			if( (codeWord & 0x0080) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// SLLC Rn, #width
			CycleCount = 1 + EAIncDelay;
			src = regNumSrc;
			if( src == 0 ) {
				break;
			}
			dest = (GR.rs[regNumDest] << 8) | GR.rs[(regNumDest - 1) & 0x0f];

			dest >>= (8 - src);
			PSW.field.C = (dest & 0x100)? 1 : 0;

			GR.rs[regNumDest] = (dest & 0xff);

			break;

		case 0x9c:
			if( (codeWord & 0x0080) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// SRL Rn, #width
			CycleCount = 1 + EAIncDelay;
			dest = GR.rs[regNumDest];
			src = regNumSrc;
			GR.rs[regNumDest] = _ALU_SRL(dest, src);
			break;

		case 0x9d:
			if( (codeWord & 0x0080) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// SRLC Rn, #width
			CycleCount = 1 + EAIncDelay;
			src = regNumSrc;
			if( src == 0 ) {
				break;
			}
			dest = (GR.rs[(regNumDest + 1) & 0x0f] << 9) | (GR.rs[regNumDest] << 1);	// bit 0 for carry

			dest >>= src;
			PSW.field.C = dest & 0x01;
			dest = (dest >> 1) & 0xff;

			GR.rs[regNumDest] = (dest & 0xff);

			break;

		case 0x9e:
			if( (codeWord & 0x0080) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// SRA Rn, #width
			CycleCount = 1 + EAIncDelay;
			dest = GR.rs[regNumDest];
			src = regNumSrc;
			GR.rs[regNumDest] = _ALU_SRA(dest, src);
			break;

		case 0x9f:
			if( (codeWord & 0x0f00) != 0x0000 ) {
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
			if( (codeWord & 0x0080) != 0x0000 ) {
				if( (codeWord & 0x0f80) != 0x0080 ) {
					retVal = CORE_ILLEGAL_INSTRUCTION;
					break;
				}
				// SB Dbitadr
				codeWord = memoryGetCodeWord(CSR, PC);	// sorry I don't have variable to hold the address
				PC = (PC + 2) & 0xfffe;
				dest = memoryGetData(GET_DATA_SEG, (EA_t)codeWord, 1);
				memorySetData(GET_DATA_SEG, codeWord, 1, _ALU_SB(dest, src));
				CycleCount = 2 + EAIncDelay;
				break;
			}
			// SB Rn.b
			GR.rs[regNumDest] = _ALU_SB(GR.rs[regNumDest], src);
			CycleCount = 1;
			break;

		case 0xa1:
			src = regNumSrc & 0x07;
			if( (codeWord & 0x0080) != 0x0000 ) {
				if( (codeWord & 0x0f80) != 0x0080 ) {
					retVal = CORE_ILLEGAL_INSTRUCTION;
					break;
				}
				// TB Dbitadr
				dest = memoryGetData(GET_DATA_SEG, (EA_t)memoryGetCodeWord(CSR, PC), 1);
				PC = (PC + 2) & 0xfffe;
				_ALU_TB(dest, src);
				CycleCount = 2 + ROMWinAccessCount + EAIncDelay;
				break;
			}
			// TB Rn.b
			_ALU_TB(GR.rs[regNumDest], src);
			CycleCount = 1;
			break;

		case 0xa2:
			src = regNumSrc & 0x07;
			if( (codeWord & 0x0080) != 0x0000 ) {
				if( (codeWord & 0x0f80) != 0x0080 ) {
					retVal = CORE_ILLEGAL_INSTRUCTION;
					break;
				}
				// RB Dbitadr
				codeWord = memoryGetCodeWord(CSR, PC);	// same as `SB Dbitadr`
				PC = (PC + 2) & 0xfffe;
				dest = memoryGetData(GET_DATA_SEG, (EA_t)codeWord, 1);
				memorySetData(GET_DATA_SEG, codeWord, 1, _ALU_RB(dest, src));
				CycleCount = 2 + EAIncDelay;
				break;
			}
			// RB Rn.b
			GR.rs[regNumDest] = _ALU_RB(GR.rs[regNumDest], src);
			CycleCount = 1;
			break;

		case 0xa3:
			if( (codeWord & 0x00f0) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// MOV Rn, PSW
			GR.rs[regNumDest] = PSW.raw;
			CycleCount = 1;
			break;

		case 0xa4:
			if( (codeWord & 0x00f0) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// MOV Rn, EPSW
			if( PSW.field.ELevel != 0 )
				GR.rs[regNumDest] = _getCurrEPSW()->raw;
			CycleCount = 2;
			break;

		case 0xa5:
			if( (codeWord & 0x01f0) != 0x0000 ) {
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
			if( (codeWord & 0x00f0) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// MOV Rn, ECSR
			GR.rs[regNumDest] = *_getCurrECSR();
			CycleCount = 2;
			break;

		case 0xa8:
			if( (codeWord & 0xf11f) != 0xa008 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// L ERn, d16[ERm]
			src = GR.ers[regNumSrc >> 1];
			src = (src + memoryGetCodeWord(CSR, PC)) & 0xffff;
			PC = (PC + 2) & 0xfffe;
			dest = memoryGetData(GET_DATA_SEG, src, 2);
			GR.ers[regNumDest >> 1] = dest;
			PSW.field.S = SIGN16(dest);
			PSW.field.Z = IS_ZERO(dest);
			CycleCount = 3 + ROMWinAccessCount + EAIncDelay;
			break;

		case 0xa9:
			if( (codeWord & 0xf11f) != 0xa009 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// ST ERn, d16[ERm]
			dest = GR.ers[regNumSrc >> 1];
			dest = (dest + memoryGetCodeWord(CSR, PC)) & 0xffff;
			PC = (PC + 2) & 0xfffe;
			memorySetData(GET_DATA_SEG, dest, 2, GR.ers[regNumDest >> 1]);
			CycleCount = 3 + EAIncDelay;
			break;

		case 0xaa:
			if( (codeWord & 0x01f0) == 0x0010 ) {
				// MOV ERn, SP
				GR.ers[regNumDest >> 1] = SP;
				CycleCount = 1;
				break;
			}
			if( (codeWord & 0x0f10) == 0x0100 ) {
				// MOV SP, ERm
				SP = GR.ers[regNumSrc >> 1];
				CycleCount = 1;
				break;
			}
			retVal = CORE_ILLEGAL_INSTRUCTION;
			break;

		case 0xab:
			if( (codeWord & 0x0f00) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// MOV PSW, Rm
			PSW.raw = GR.rs[regNumSrc];
			CycleCount = 1;
			break;

		case 0xac:
			if( (codeWord & 0x0f00) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// MOV EPSW, Rm
			_getCurrEPSW()->raw = GR.rs[regNumSrc];
			CycleCount = 2;
			break;

		case 0xad:
			if( (codeWord & 0x01f0) != 0x0000 ) {
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
			if( (codeWord & 0x0f00) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// MOV ECSR, Rm
			*_getCurrECSR() = GR.rs[regNumSrc] & 0x0f;
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
			switch( codeWord & 0x01c0 ) {
				case 0x0000:
					// L ERn, disp6[BP]
					src = GR.ers[12 >> 1];		// src = ER12
					src = (src + _signExtend(codeWord & 0x003f, 6)) & 0xffff;
					dest = memoryGetData(GET_DATA_SEG, src, 2);
					GR.ers[regNumDest >> 1] = dest;
					PSW.field.S = SIGN16(dest);
					PSW.field.Z = IS_ZERO(dest);
					CycleCount += ROMWinAccessCount;
					break;

				case 0x0040:
					// L ERn, disp6[FP]
					src = GR.ers[14 >> 1];		// src = ER14
					src = (src + _signExtend(codeWord & 0x003f, 6)) & 0xffff;
					dest = memoryGetData(GET_DATA_SEG, src, 2);
					GR.ers[regNumDest >> 1] = dest;
					PSW.field.S = SIGN16(dest);
					PSW.field.Z = IS_ZERO(dest);
					CycleCount += ROMWinAccessCount;
					break;

				case 0x0080:
					// ST ERn, disp6[BP]
					dest = GR.ers[12 >> 1];		// dest = ER12
					dest = (dest + _signExtend(codeWord & 0x003f, 6)) & 0xffff;
					memorySetData(GET_DATA_SEG, dest, 2, GR.ers[regNumDest >> 1]);
					break;

				case 0x00c0:
					// ST ERn, disp6[FP]
					dest = GR.ers[14 >> 1];		// dest = ER14
					dest = (dest + _signExtend(codeWord & 0x003f, 6)) & 0xffff;
					memorySetData(GET_DATA_SEG, dest, 2, GR.ers[regNumDest >> 1]);
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
			switch( codeWord & 0x0f00 ) {
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
			switch( codeWord & 0x00c0 ) {
				case 0x0000:
					// L Rn, disp6[BP]
					src = GR.ers[12 >> 1];		// src = ER12
					src = (src + _signExtend(codeWord & 0x003f, 6)) & 0xffff;
					dest = memoryGetData(GET_DATA_SEG, src, 1);
					GR.rs[regNumDest] = dest;
					PSW.field.S = SIGN8(dest);
					PSW.field.Z = IS_ZERO(dest);
					CycleCount += ROMWinAccessCount;
					break;

				case 0x0040:
					// L Rn, disp6[FP]
					src = GR.ers[14 >> 1];		// src = ER14
					src = (src + _signExtend(codeWord & 0x003f, 6)) & 0xffff;
					dest = memoryGetData(GET_DATA_SEG, src, 1);
					GR.rs[regNumDest] = dest;
					PSW.field.S = SIGN8(dest);
					PSW.field.Z = IS_ZERO(dest);
					CycleCount += ROMWinAccessCount;
					break;

				case 0x0080:
					// ST Rn, disp6[BP]
					dest = GR.ers[12 >> 1];		// dest = ER12
					dest = (dest + _signExtend(codeWord & 0x003f, 6)) & 0xffff;
					memorySetData(GET_DATA_SEG, dest, 1, GR.rs[regNumDest]);
					break;

				case 0x00c0:
					// ST Rn, disp6[FP]
					dest = GR.ers[14 >> 1];		// dest = ER14
					dest = (dest + _signExtend(codeWord & 0x003f, 6)) & 0xffff;
					memorySetData(GET_DATA_SEG, dest, 1, GR.rs[regNumDest]);
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
			if( (codeWord & 0x0180) == 0x0000 ) {
				// MOV ERn, #imm7
				src = _signExtend(codeWord & 0x007f, 7);
				GR.ers[regNumDest >> 1] = src;
				PSW.field.Z = IS_ZERO(src);
				PSW.field.S = SIGN16(src);
				CycleCount = 2;
				break;
			}
			if( (codeWord & 0x0180) == 0x0080 ) {
				// ADD ERn, #imm7
				src = _signExtend(codeWord & 0x007f, 7);
				GR.ers[regNumDest >> 1] = _ALU_ADD_W(GR.ers[regNumDest >> 1], src);
				CycleCount = 2;
				break;
			}
			switch( codeWord & 0x0f00 ) {
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
					if( codeWord == 0xeb7f ) {
						// RC
						PSW.field.C = 0;
						CycleCount = 1;
						break;
					}
					if( codeWord == 0xebf7 ) {
						// DI
						PSW.field.MIE = 0;
						CycleCount = 3;
						break;
					}

					retVal = CORE_ILLEGAL_INSTRUCTION;
					break;

				case 0x0d00:
					if( codeWord == 0xed08 ) {
						// EI
						PSW.field.MIE = 1;
						CycleCount = 1;
						// Todo: Disable maskable interrupts for 2 cycles
						break;
					}
					if( codeWord == 0xed80 ) {
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
			if( (codeWord & 0x00f0) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// B Cadr
			PC = memoryGetCodeWord(CSR, PC) & 0xfffe;
			CSR = regNumDest;
			CycleCount = 2 + EAIncDelay;
			break;

		case 0xf1:
			if( (codeWord & 0x00f0) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// BL Cadr
			LR = (PC + 2) & 0xfffe;
			LCSR = CSR;
			PC = memoryGetCodeWord(CSR, PC) & 0xfffe;
			CSR = regNumDest;
			CycleCount = 2 + EAIncDelay;
			break;

		case 0xf2:
			if( (codeWord & 0x0f10) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// B ERn
			PC = GR.ers[regNumSrc >> 1] & 0xfffe;
			CycleCount = 2 + EAIncDelay;
			break;

		case 0xf3:
			if( (codeWord & 0x0f10) != 0x0000 ) {
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
			if( (codeWord & 0x0100) != 0x0000 ) {
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
			if( (codeWord & 0x0110) != 0x0000 ) {
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
			if( (codeWord & 0x0110) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// ADD ERn, ERm
			CycleCount = 2;
			GR.ers[regNumDest >> 1] = _ALU_ADD_W(GR.ers[regNumDest >> 1], GR.ers[regNumSrc >> 1]);
			break;

		case 0xf7:
			if( (codeWord & 0x0110) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// CMP ERn, ERm
			CycleCount = 2;
			_ALU_CMP_W(GR.ers[regNumDest >> 1], GR.ers[regNumSrc >> 1]);
			break;

		case 0xf9:
			if( (codeWord & 0x0100) != 0x0000 ) {
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
			if( (codeWord & 0x0010) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// LEA [ERm]
			EA = GR.ers[regNumSrc >> 1];
			CycleCount = 1;
			break;

		case 0xfb:
			if( (codeWord & 0x0010) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// LEA disp16[ERm]
			dest = GR.ers[regNumSrc >> 1];
			EA = (memoryGetCodeWord(CSR, PC) + dest) & 0xffff;
			PC = (PC + 2) & 0xfffe;
			CycleCount = 2;
			break;

		case 0xfc:
			if( (codeWord & 0x0010) != 0x0000 ) {
				retVal = CORE_ILLEGAL_INSTRUCTION;
				break;
			}
			// LEA Dadr
			EA = memoryGetCodeWord(CSR, PC);
			PC = (PC + 2) & 0xfffe;
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
			switch( codeWord & 0x00f0 ) {
				case 0x0000:
					// POP Rn
					GR.rs[regNumDest] = _popValue(1);
					CycleCount = 2 + EAIncDelay;
					break;

				case 0x0010:
					// POP ERn
					if( (regNumDest & 0x01) != 0x00 ) {
						retVal = CORE_ILLEGAL_INSTRUCTION;
						break;
					}
					GR.ers[regNumDest >> 1] = _popValue(2);
					CycleCount = 2 + EAIncDelay;
					break;

				case 0x0020:
					// POP XRn
					if( (regNumDest & 0x03) != 0x00 ) {
						retVal = CORE_ILLEGAL_INSTRUCTION;
						break;
					}
					GR.xrs[regNumDest >> 2] = _popValue(4);
					CycleCount = 4 + EAIncDelay;
					break;

				case 0x0030:
					// POP QRn
					if( (regNumDest & 0x07) != 0x00 ) {
						retVal = CORE_ILLEGAL_INSTRUCTION;
						break;
					}
					GR.qrs[regNumDest >> 3] = _popValue(8);
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
						EA = _popValue(2);
						CycleCount += 2;
					}
					if( regNumDest & 0x08 ) {
						// LR
						LR = _popValue(2);
						LCSR = _popValue(1) & 0x0f;
						CycleCount += 4;
					}
					if( regNumDest & 0x04 ) {
						// PSW
						PSW.raw = _popValue(1);
						CycleCount += 2;
					}
					if( regNumDest & 0x02 ) {
						// PC
						PC = _popValue(2) & 0xfffe;
						CSR = _popValue(1) & 0x0f;
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
						_pushValue(LCSR, 1);
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
			switch( codeWord ) {
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
					// Yes, OKI decided that `INC [EA]` shouldn't affect carry flag
					dest = PSW.field.C;
					memorySetData(GET_DATA_SEG, EA, 1, _ALU_ADD(memoryGetData(GET_DATA_SEG, EA, 1), 1));
					PSW.field.C = dest;
					CycleCount = 2 + EAIncDelay;
					break;

				case 0xfe3f:
					// DEC [EA]
					// Same for `DEC [EA]`
					dest = PSW.field.C;
					memorySetData(GET_DATA_SEG, EA, 1, _ALU_SUB(memoryGetData(GET_DATA_SEG, EA, 1), 1));
					PSW.field.C = dest;
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
					// Actually this code should call a standard interrupt implementation
					if( PSW.field.ELevel > 1 ) {
						// reset if ELEVEL is 2 or 3
						coreReset();
					}
					else {
						ELR2 = (PC + 2) & 0xfffe;
						ECSR2 = CSR;
						EPSW2 = PSW;
						PSW.field.ELevel = 2;
						PC = memoryGetCodeWord((SR_t)0, (PC_t)0x0004);
					}
					CycleCount = 7 + EAIncDelay;
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
