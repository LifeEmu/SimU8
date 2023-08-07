#include <stdio.h>

#include "inc/mmu.h"

int main(void) {
	if( memoryInit("rom.bin", NULL) != MEMORY_OK ) {
		puts("Unable to initialize memory!");
		exit(-1);
	}

	printf("CodePointer = %p, DataPointer = %p\nWaiting for a key...", CodeMemory, DataMemory);
	getchar();

	memoryFree();
	return 0;
}
