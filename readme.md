# SimU8

An nX-U8/100 emulator written in C.  
This emulator is mainly for emulating CASIO scientific calculators, and aims to be as portable as possible.  
This has nothing to do with CASIO's `SimU8.dll`.

### This branch has finished its job. It will be removed from this repository in the near future.

## Usage
See `readme.md` in branch `master`.

## Testing
**NOTE**: `testcore.c` uses `conio.h`, so it probably won't work unmodified under non-Windows system!

**NOTE #2**: _This_ test driver aims at emulating CASIO fx-ES PLUS hardware, so it would work the best with a ROM dump from a real calculator. Keyboard is not implemented in this branch. Check `sfr_drivers`, `cwi_test` and `cwii_test` instead.

- Put the binary you want to run into the root directory of this repository and rename it to `rom.bin`. (you can change the name of the file in `testcore.c`)
- `cd` to the repository in `cmd` and run `<your compiler> testcore.c src/*.c src/SFR/*.c <other compiler options>`.
- Run the binary that you've just got from the compiler.
- **There is an experimental version of `testcore.c`, that displays emulated calculator LCD using Braille characters. To use it, change the command above to** `<your compiler> `_**`BrailleDisplay.c testcore_braille.c`**_` src/*.c src/SFR/*.c <other compiler options>`


**Commands**:
| Char | Function                                    |
| ---- | ------------------------------------------- |
| `r`  | show **R**egisters                          |
| `a`  | show base **A**ddresses of allocated memory |
| `s`  | enable **S**ingle-step mode                 |
| `p`  | resume execution ("un**P**ause")            |
| `b`  | set & enable a **B**reakpoint               |
| `n`  | Disable the breakpoint ("**N**oBreak")      |
| `c`  | Reset core ("**C**lear")                    |
| `d`  | **D**isplay VRAM                            |
| `j`  | **J**ump to a new address                   |
| `m`  | show data **M**emory                        |
| `z`  | **Z**ero RAM                                |
| `w`  | **W**rite to savestate file (experimental)  |
| `e`  | **R**ead from savestate file (experimental) |
| `q`  | **Q**uit emulator                           |

## Credit
- [Fraserbc on GitHub](https://github.com/Fraserbc) for knowledge about pipeline and bug reports
- [gamingwithevets on GitHub](https://github.com/gamingwithevets) for many bug feedbacks
- Zeroko, Tari, calc84maniac and c4ooo on Cemetech that helped me figure out how to display Braille in cmd
- Z80 user manual and AMD architectural programmer's manual for information about flags and BCD instructions (why I put them here lol)
- OKI/Lapis for making this little RISC core
