#include <stdint.h>


#ifndef REGTYPES_H_INCLUDED
#define REGTYPES_H_INCLUDED


typedef uint8_t SR_t;	// CSR, DSR
typedef uint16_t PC_t;	// PC, LR
typedef uint16_t EA_t;	// EA, SP
typedef union {
	uint8_t raw;
	struct {
		uint8_t ELevel	:2;
		uint8_t HC	:1;
		uint8_t MIE	:1;
		uint8_t OV	:1;
		uint8_t S	:1;
		uint8_t Z	:1;
		uint8_t C	:1;
	};
} PSW_t;
typedef union {
	uint64_t qrs[2];
	uint32_t xrs[4];
	uint16_t ers[8];
	uint8_t rs[16];
} GR_t;
// Note: This only works on little-endian machines

#endif
