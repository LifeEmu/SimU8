# SimU8

An nX-U8/100 simulator written in C. This has nothing to do with CASIO's `SimU8.dll`.

Not finished yet.

~~Now the source files and the header files are separated. To use the code in your project, include the header in your source, and include corresponding source file when compiling.~~

~~Example: If you want to use features from "core", include `inc/core.h` in your source file, and add `src/core.c` and `src/mmu.c`(`core.c` relies on MMU features) to input files of your C compiler.~~

**Compile test result:**
- Compiler: MinGW GCC 13.2.0
- Options: `-std=c99 -Wall -Os`
- Result: 4 Warnings, 0 error(s)

## Dependencies
I want to reduce the dependencies as much as possible, so it would be easier to port to other platforms. Here are the dependencies of each "module":
- `mmu.c`
	- `<stdint.h>`: Integer types
	- `<stdbool.h>`: Boolean values
	- `"/inc/mmustub.h"`: U8 memory initialization/save/load
- `core.c`
	- `<stdbool.h>`: Boolean values
	- `<stdint.h>`: Integer types
	- `"/inc/mmu.h"`: U8 memory space
- `lcd.c` (technically a peripheral)
	- `<stdint.h>`: Integer types
	- `void setPix(int x, int y, int c)`: You need to implement it to use the LCD "module"
	- `void updateDisp(void)`: Same as above

## Notes
- **MMU functions does not support watchpoints _yet_.** Coming soon~
- **LCD shouldn't be a part of the core emulator**. It will eventually be removed.
- **There is no way to save/load core states _yet_**. Coming soon~
- **Clean up of header files will happen in the near future**. I'll try to make the headers less entangled.

## Changelog for this commit
- **MMU supports arbitrary mapping for data memory accesses now!** I stole the idea from fraserbc :P

## Special Thanks
- [Fraserbc](https://github.com/Fraserbc)
	- He has been helping me understand nX-U8/100 architecture
	- I'm using [his emulator(check it out!)](https://github.com/Fraserbc/u8_emu) as the reference implementation
- [gamingwithevets](https://github.com/gamingwithevets)
	- He reported a lot of bugs
	- He made [a graphical frontend for SimU8](https://github.com/gamingwithevets/simu8-frontend)
