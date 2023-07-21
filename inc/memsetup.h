#ifndef MEMSETUP_H_INCLUDED
#define MEMSETUP_H_INCLUDED


// On segment 1 and above, code and data memory accesses access the same physical memories
// and an assumption is made here: I assume all pages above page 0 are read-only
#define CODE_PAGE_COUNT 2
#define DATA_PAGE_COUNT 2
#define ROM_WINDOW_SIZE 0x8000

// mask for page mirrowing
// If there're 3 code pages (0~3), mirrow mask should be set to 0x03
// real page < 3, mirrowed page > mask
/* Page	| Type
 * 0	| real
 * 1	| real
 * 2	| real
 * 3	| unmapped
 * 4	| mirrow
 * 5	| mirrow
 * 6	| mirrow
 * 7	| mirrow
 */
#define CODE_MIRROW_MASK 0x01
#define DATA_MIRROW_MASK 0x07


#endif
