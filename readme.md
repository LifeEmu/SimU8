# SimU8

An nX-U8/100 simulator written in C. This has nothing to do with CASIO's `SimU8.dll`.

Not finished yet.

Now the source files and the header files are separated. To use the code in your project, include the header in your source, and include corresponding source file when compiling.

Example: If you want to use features from "core", include `inc/core.h` in your source file, and add `src/core.c` and `src/mmu.c`(`core.c` relies on MMU features) to input files of your C compiler.

**Compile test result:**
- Compiler: GCC 13.2.0 that came with w64devkit
- Options: `-std=c99 -Wall -Os`
- Result: 0 Warning(s), 0 error(s)

## Dependencies
I want to reduce the dependencies as much as possible, so it would be easier to port to other platforms. Here are the dependencies of each "module":
- `mmu.c`
	- `<stdio.h>`: File operations (including operations on `stdout`)
	- `<string.h>`: `memset`
	- `<stdlib.h>`: Memory management (`malloc` and `free`)
	- `<stdint.h>`: Integer types
	- `<stdbool.h>`: Boolean values
	- `<stddef.h>`: `size_t` type
- `core.c`
	- `<stdbool.h>`: Boolean values
	- `<stdint.h>`: Integer types
	- `"/inc/mmu.h"`: U8 memory space
- `lcd.c` (technically a peripheral)
	- `<stdint.h>`: Integer types
	- `void setPix(int x, int y, int c)`: You need to implement it to use the LCD "module"
	- `void updateDisp(void)`: Same as above

## Notes
- **MMU functions** that interfaces with U8 memory **are rewritten to return data in registers.** (That didn't give a speed boost though)
- **ALU operations are split into smaller functions** now. It used to be a big function that does different jobs based on the value of the 3rd parameter. Splitting it helps eliminating a `switch` statement and saves resources for passing a constant.
- **`coreDispRegs` is no longer a part of the emulator core.** That helps me eliminate the dependency on `stdio.h` for `core.c`.
- Current implementation of **MMU functions does not support watchpoints.** All the reads & writes are well-encapsulated into MMU functions, which is good on its own, but my implementation doesn't support hooking yet, which means there is no way to know where in the emulated memory space the user has accessed without checking manually everytime an MMU function is called.
- Current implementation of **core functions are inefficient.** Despite being written in pure C, the emulator runs at around 1/10 of instructions compared with real hardware, which is unbearable. Aside from the problem with the MMU functions, the core is far from perfection. My thought is to use lookup tables of function pointers & attributes for instruction implementations, to boost the execution speed & simplify the coding.
- **LCD shouldn't be a part of the core emulator**. It will eventually be removed.
- **There is no way to save/load core states yet**. I might take a more OOP-ish approach that stores the states of the core in a struct.

## Special Thanks
- [Fraserbc](https://github.com/Fraserbc)
	- He has been helping me understand nX-U8/100 architecture
	- I used [his emulator(check it out!)](https://github.com/Fraserbc/u8_emu) as the reference implementation
- [gamingwithevets](https://github.com/gamingwithevets)
	- He reported a lot of bugs
	- He made [a graphical frontend for SimU8](https://github.com/gamingwithevets/simu8-frontend)
