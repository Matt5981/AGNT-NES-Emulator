# AGNT-NES-Emulator
[![CodeFactor](https://www.codefactor.io/repository/github/matt5981/agnt-nes-emulator/badge/main)](https://www.codefactor.io/repository/github/matt5981/agnt-nes-emulator/overview/main)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

[![forthebadge](https://forthebadge.com/images/badges/contains-tasty-spaghetti-code.svg)](https://forthebadge.com)
-----
An emulator for the Nintendo(R) Entertainment System, written in C with SDL2 for graphics. In **very** early development.

## Building and Running
Make sure SDL2 is installed through your weapon of choice, then run `make all`. Binaries are output in `bin`. Uses `unistd.h` for file I/O, so no Windows support for now.
### Controls (when added)
| NES button | Keyboard |
|-|-|
| D-Pad | arrow keys |
| A | X |
| B | Z |
| START | Enter/return |
| SELECT | Right shift |

## Attributions and References
The NESDev wiki, in particular this page for the implementation details/addresses of opcodes: https://www.nesdev.org/obelisk-6502-guide/reference.html

'Nintendo Entertainment System', 'Famicom', 'Nintendo', and any other associated trademarked names are the property of Nintendo Co., Ltd. No code made by Nintendo, 
whether internal or in game cartridges, has been or will be included in this project. Any references to Nintendo or the Nintendo Entertainment System are made purely 
without license, and this software does not constitute, represent or impersonate a licensed product. Any usage of intellectual property belonging to Nintendo Co., Ltd. 
is made purely without official license.
