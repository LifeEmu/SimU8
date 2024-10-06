# SimU8

An nX-U8/100 emulator written in C.  
This emulator is mainly for emulating CASIO scientific calculators, and aims to be as portable as possible.  
This has nothing to do with CASIO's `SimU8.dll`.

Not finished yet.

**Compile test result:**
- Compiler:  GCC (Rev3, Built by MSYS2 project) 14.1.0
- Options: `-std=c99 -Wall -Os -fPIC -shared`
- Result: 0 Warning(s), 0 error(s)


## Dependencies
I want to reduce the dependencies as much as possible, so it would be easier to port to other platforms. Here are the dependencies of each "module":
- `mmu.c`
	- `<stdint.h>`: Integer types
	- `<stdbool.h>`: Boolean values
	- `<stddef.h>`: `size_t`
	- `"src/mmustub.h"`: U8 memory initialization/save/load
- `core.c`
	- `<stdint.h>`: Integer types
	- `<stdbool.h>`: Boolean values
	- `<stddef.h>`: `size_t`
- `lcd.c` (technically a peripheral)
	- `<stdint.h>`: Integer types
	- `void setPix(int x, int y, int c)`: You need to implement it to use the LCD "module"
	- `void updateDisp(void)`: Same as above


## Port it to your platform
Since this emulator is very bare-bone and relies on almost nothing, you can easily adapt the code to run on nearly any platform you like (I tried porting it to RP2 Pico and it runs fine).  
There are, however, some work for you to do:
- **Implement `extern` functions in `src/mmustub.h`**. "MMU" relies on these functions to allocate memory to emulate U8 memory spaces. There are comments for each function prototype, and you can refer to `src/mmustub_pc.c` too.
- **Implement `SFRHandler` in `src/memmap.h`** . This function is the interface between _core memory space_ and _peripherals_. You can either implement some of them yourself, or just make it call standard RAM handler.
	> NOTE: There are some experimental SFR code in branch `sfr_drivers`, `cwi_test` and `cwii_test`.  
	> You can refer to them, but _DO NOT_ rely on them - They're not stable and may be changed/deleted at any time.
- **Toggle some settings**. There are some macros/functions that you may want to adjust:
	- `src/mmustub.h`: type definitions for stub functions
	- `src/memmap.h`: ROM window size, data memory region count, code/data segment mask
	- `src/memmap.c`: memory regions, their behaviors and priorities
	- `src/core.h`: U8/U16 selection
- Finally, **Make a driver program**. Basically you only need to initialize the memory and reset the core, then you'll be ready to run the ROM by continuously stepping through it.

> The simplest way to get it output something on your non-PC device is:
> - Modify `src/mmustub_pc.c`, or delete it and implement your own stub functions, that returns pre-defined `const unsigned char[]` for ROM, and pre-allocated `unsigned char[0x10000 - ROM_WINDOW_SIZE]` for RAM+SFR area
> - Make `SFRHandler` call `RAMHandler`, or edit `DATA_MEMORY_MAP` (in `src/memmap.c`) directly, to make SFR area behave like ordinary RAM
> - Adjust the configurations so it matched the ROM you grabbed (The configurations in this branch emulates real ES+)
> - sketch up a driver program like this:
> ```c
> #include "src/mmu.h"	// memory initialization
> #include "src/core.h"	// core operations
> int main() {
>	if( memoryInit() != MEMORY_OK ) return -1;
>
>	coreReset();
>
>	while( coreStep() != CORE_ILLEGAL_INSTRUCTION )
>		;	// step until illegal instruction
>
>	// Here, you need to somehow display VRAM (usually at 0x0F800h) or display buffers yourself.
>	// Check `lcd.h` to see if it could be useful.
>
>	memoryFree();
> }
> ```
> - Finally, compile all the C files (including the driver you made) and run it. You should see something in VRAM/display buffer when it finishes.


## Notes
- **MMU functions does not support watchpoints _yet_**. I _may_ include hooking ability in the future, but it may slow down the code further... However, you can easily add it yourself if you want.
- **There is no way to save/load core states _yet_**.
- **_Headers have been rearranged_**.


## Changelog for this commit
- **LCD module has been removed from the emulator core**. They'll go to test branches instead.

## Special Thanks
- [Fraserbc](https://github.com/Fraserbc)
	- He has been helping me understand nX-U8/100 architecture
	- I'm using [his emulator(check it out!)](https://github.com/Fraserbc/u8_emu) as the reference implementation
	- The address translation logic is inspired by his emulator, too.

- [gamingwithevets](https://github.com/gamingwithevets)
	- He reported a lot of bugs
	- He made [a graphical frontend for SimU8](https://github.com/gamingwithevets/simu8-frontend)

- [user202729](https://github.com/user202729) and [Xyzstk](https://github.com/Xyzstk)
	- They've documented lots of SFRs
	- Their work is the base of nearly all discoveries after 2019
