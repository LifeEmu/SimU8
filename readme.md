# SimU8

An nX-U8/100 simulator written in C. This has nothing to do with CASIO's `SimU8.dll`.

Not finished yet.
- Most instructions are implemented but not all of them are tested.
- `SWI` is not implemented.
- All the coprocessors instructions are not implemented.
- Interrupts are not implemented yet.

## Usage
To use the code in your project, include the header in your source, and include corresponding source file when compiling.

Example: If you want to use features from "core", include `inc/core.h` in your source file, and add `src/core.c` and `src/mmu.c`(`core.c` relies on MMU features) to input files of your C compiler (for example `gcc <your sources> src/core.c src/mmu.c -Wall -o simu8.exe` if you are using Windows).


## Testing
**NOTE**: `testcore.c` uses `conio.h`, so it probably won't work unmodified under non-Windows system!

**NOTE #2**: This test driver aims at emulating CASIO fx-ES PLUS hardware, so it would work the best with a ROM dump from a real calculator. With that being said, keyboard emulation is yet to be implemented.

- Put the binary you want to run into the root directory of this repository and rename it to `rom.bin`. (you can change the name of the file in `testcore.c`)
- `cd` to the repository in `cmd` and run `<your compiler> testcore.c src/mmu.c src/core.c src/lcd.c <other compiler options>`.
- Run the binary that you've just got from the compiler.
- **There is an experimental version of `testcore.c`, that displays emulated calculator LCD using Braille characters. To use it, change the command above to** `<your compiler> `_**`BrailleDisplay.c testcore_braille.c`**_` src/mmu.c src/core.c src/lcd.c <other compiler options>`


**Commands**:
| Char | Function                                |
| ---- | --------------------------------------- |
| `r`  | Show registers                          |
| `a`  | Show base addresses of allocated memory |
| `s`  | Enable single-step mode                 |
| `p`  | Resume execution ("unPause")            |
| `b`  | Set & enable a breakpoint               |
| `n`  | Disable the breakpoint                  |
| `c`  | Reset core                              |
| `d`  | Display VRAM                            |
| `j`  | Jump to a new address                   |
| `m`  | Show data memory                        |
| `z`  | Zero RAM                                |
| `w`  | Write to savestate file (experimental)  |
| `e`  | Read from savestate file (experimental) |

## Credit
- [Fraserbc on GitHub](https://github.com/Fraserbc) for knowledge about pipeline and bug reports
- [gamingwithevets on GitHub](https://github.com/gamingwithevets) for many bug feedbacks
- Z80 user manual and AMD architectural programmer's manual for information about flags and BCD instructions (why I put them here lol)
- OKI/Lapis for making this little RISC core
- Zeroko, Tari, calc84maniac and c4ooo on Cemetech that helped me figure out how to display Braille in cmd
