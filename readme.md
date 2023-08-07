# SimU8

An nX-U8/100 simulator written in C. This has nothing to do with CASIO's `SimU8.dll`.

Not finished yet.
- Most instructions are implemented but not all of them are tested.
- `SWI` and `BRK` are not implemented.
- All the instructions about coprocessors are not implemented.
- Interrupts are not implemented yet.

It compiles with no error with `-Wall -std=c99` using gcc 3.4.5 from MinGW64.


## Usage
To use the code in your project, include the header in your source, and include corresponding source file when compiling.

Example: If you want to use features from "core", include `inc/core.h` in your source file, and add `src/core.c` and `src/mmu.c`(`core.c` relies on MMU features) to input files of your C compiler (for example `gcc <your sources> src/core.c src/mmu.c -Wall -o simu8.exe` if you are using Windows).


## Testing
This branch has 3 C source files, containing (bad) example code that I wrote.

You can start with `testcore.c` first:
- Put the binary you want to run into the root directory of this repository and rename it to `rom.bin`. (you can change the name of the file in `testcore.c`)
- `cd` to the repository in `cmd` and run `<your compiler> testcore.c src/mmu.c src/core.c <other compiler options>`.
- Run the binary that you've just got from the compiler.
- Hit \[ENTER\] to step, type `q` to exit.


## Credit
- [Fraserbc on GitHub](https://github.com/Fraserbc) for knowledge about pipeline
- Z80 user manual and AMD architectural programmer's manual for information about flags and BCD instructions (why I put them here lol)
- OKI/Lapis for making this little RISC core
