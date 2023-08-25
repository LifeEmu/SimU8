# SimU8

An nX-U8/100 simulator written in C. This has nothing to do with CASIO's `SimU8.dll`.

Not finished yet.

Now the source files and the header files are separated. To use the code in your project, include the header in your source, and include corresponding source file when compiling.

Example: If you want to use features from "core", include `inc/core.h` in your source file, and add `src/core.c` and `src/mmu.c`(`core.c` relies on MMU features) to input files of your C compiler.

Tested with gcc 3.4.5 from MinGW64, it compiles with no error.

## Notes
- Current implementation of **MMU functions are inelegant & inefficient.** They return the actual data in a global variable, which is slow and confusing. I need to rewrite them at some point to return the data in registers, which should be easier to use & run faster.
- Current implementation of **MMU functions does not support watchpoints.** All the reads & writes are well-encapsulated into MMU functions, which is good on its own, but my implementation doesn't support hooking yet, which means there is no way to know where in the emulated memory space the user has accessed without checking manually everytime an MMU function is called.
- Current implementation of **core functions are inefficient.** Despite being written in pure C, the emulator runs at around 1/10 of instructions compared with real hardware, which is unbearable. Aside from the problem with the MMU functions, the core is far from perfection. My thought is to use lookup tables of function pointers & attributes for instruction implementations, to boost the execution speed & simplify the coding.
