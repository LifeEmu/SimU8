# SimU8

An nX-U8/100 simulator written in C. This has nothing to do with CASIO's `SimU8.dll`.

Not finished yet.

<s>
If you want to use features in `./src/mmu.c`, include it; If you want to use features in `./src/core.c`, include **_only_** `./src/core.c`, as it already uses features in `mmu.c`.
</s>

Now the source files and the header files are separated. To use the code in your project, include the header in your source, and include corresponding source file when compiling.

Example: If you want to use features from "core", include `inc/core.h` in your source file, and add `src/core.c` and `src/mmu.c`(`core.c` relies on MMU features) to input files of your C compiler.

Tested with gcc 3.4.5 from MinGW64, and it compiles with no error/warning. 
