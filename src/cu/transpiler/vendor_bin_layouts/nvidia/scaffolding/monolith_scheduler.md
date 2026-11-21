# The scheduler's bits NVIDIA's compiler writes

Written by `monolith_scheduler.sh` whole on every run. Every CUDA source of the tree is built by NVIDIA's compiler for one part, each cubin's code sections are read from the ELF, each instruction is named by our reader through the machine file and its bits 105 to 127 read through the fields the machine file names. No listing is read and no disassembler is run.

18 cubins, 71488 instructions, 71416 no form reads, 94 operations.

An operation's soonest read is the fewest cycles, its stalls summed, between it and the first instruction that reads the register it writes, over every place it sets no write barrier and no later one blocks it. Readers waiting count the first readers of a register it writes with a write barrier where a wait on that barrier is between the two, at the reader or before it, against those where none does. A barrier counts its producers, and a wait on it blocks until all of them are back. Behind a later one counts the first readers of a register it writes with no barrier where a later instruction of the same operation sets one, waited on before the read. Behind an earlier one counts those where no later one does and an earlier instruction of the same operation sets one, waited on before the read, with the fewest cycles between that wait and the read. The machine file columns count the places whose form records a write barrier, and a read barrier, against the places a form was found.

| operation | written | stalls | write barrier | read barrier | waits | soonest read | readers waiting | behind a later one | behind an earlier one | machine file write | machine file read |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `no form, key 0xa12` | 28520 | 1 to 7, usually 2 | 0 | 0 | 1 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x212` | 15114 | 1 to 5, usually 2 | 0 | 0 | 8 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x219` | 12595 | 1 to 5, usually 2 | 0 | 0 | 45 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0xa10` | 5188 | 1 to 11, usually 2 | 0 | 0 | 2 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0xa24` | 2640 | 1 to 5, usually 3 | 0 | 0 | 16 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x224` | 803 | 1 to 12, usually 1 | 0 | 0 | 72 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x20c` | 669 | 1 to 13, usually 7 | 0 | 0 | 68 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x947` | 586 | 0 to 6, usually 5 | 0 | 0 | 4 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x810` | 456 | 1 to 11, usually 4 | 0 | 0 | 44 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x624` | 427 | 1 to 5, usually 2 | 0 | 0 | 2 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x824` | 387 | 1 to 8, usually 5 | 0 | 0 | 36 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x210` | 345 | 1 to 11, usually 2 | 0 | 0 | 21 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0xa0c` | 341 | 1 to 13, usually 2 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x812` | 285 | 1 to 13, usually 2 | 0 | 0 | 4 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x981` | 278 | 1 to 7, usually 1 | 278 | 94 | 26 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x986` | 261 | 1 to 7, usually 4 | 0 | 217 | 18 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x805` | 214 | 1 to 11, usually 2 | 0 | 0 | 79 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x918` | 202 | 0 to 0, usually 0 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x819` | 186 | 1 to 5, usually 2 | 0 | 0 | 6 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0xa11` | 141 | 1 to 6, usually 4 | 0 | 0 | 5 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x80c` | 122 | 1 to 13, usually 1 | 0 | 0 | 1 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x424` | 121 | 1 to 12, usually 2 | 0 | 0 | 10 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x227` | 117 | 1 to 8, usually 4 | 0 | 0 | 7 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x944` | 104 | 1 to 5, usually 1 | 0 | 0 | 14 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x625` | 97 | 1 to 6, usually 4 | 0 | 0 | 12 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x207` | 92 | 1 to 7, usually 2 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x941` | 79 | 5 to 5, usually 5 | 0 | 0 | 4 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x945` | 79 | 1 to 12, usually 1 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0xa25` | 78 | 1 to 5, usually 4 | 0 | 0 | 13 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x983` | 71 | 1 to 11, usually 4 | 71 | 34 | 4 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x225` | 56 | 1 to 5, usually 4 | 0 | 0 | 5 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0xab9` | 56 | 1 to 5, usually 1 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0xc24` | 54 | 1 to 5, usually 2 | 0 | 0 | 2 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `EXIT` | 51 | 5 to 5, usually 5 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 51 | 0 of 51 |
| `no form, key 0x807` | 48 | 1 to 7, usually 4 | 0 | 0 | 1 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0xc0c` | 42 | 1 to 13, usually 13 | 0 | 0 | 4 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x81c` | 39 | 1 to 13, usually 2 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x308` | 37 | 1 to 8, usually 2 | 31 | 0 | 37 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0xc25` | 33 | 1 to 4, usually 4 | 0 | 0 | 1 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x305` | 31 | 1 to 11, usually 2 | 31 | 26 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0xe24` | 30 | 1 to 3, usually 2 | 0 | 0 | 2 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0xc10` | 28 | 1 to 5, usually 2 | 0 | 0 | 5 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x202` | 27 | 1 to 5, usually 1 | 0 | 0 | 1 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x919` | 27 | 1 to 4, usually 1 | 27 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x387` | 25 | 1 to 4, usually 2 | 0 | 25 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x211` | 24 | 1 to 4, usually 2 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x802` | 22 | 1 to 5, usually 1 | 0 | 0 | 3 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0xb06` | 21 | 1 to 8, usually 1 | 21 | 0 | 1 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x801` | 20 | 2 to 3, usually 3 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `S2R` | 14 | 1 to 5, usually 2 | 14 | 0 | 0 | - | 0 of 0 | 0 | 0 | 14 of 14 | 0 of 14 |
| `no form, key 0x825` | 14 | 4 to 4, usually 4 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x98e` | 14 | 1 to 7, usually 1 | 0 | 2 | 2 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x803` | 12 | 1 to 3, usually 1 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0xc12` | 12 | 1 to 5, usually 2 | 0 | 0 | 1 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0xc82` | 11 | 1 to 3, usually 1 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x419` | 10 | 2 to 4, usually 2 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x817` | 10 | 2 to 4, usually 4 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x890` | 10 | 1 to 6, usually 1 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x290` | 9 | 1 to 6, usually 1 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x2a4` | 9 | 1 to 4, usually 1 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0xd06` | 8 | 1 to 8, usually 1 | 8 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x311` | 6 | 2 to 2, usually 2 | 6 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x950` | 6 | 5 to 6, usually 6 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `ST` | 5 | 1 to 5, usually 1 | 0 | 1 | 1 | - | 0 of 0 | 0 | 0 | 0 of 5 | 0 of 5 |
| `no form, key 0x882` | 5 | 1 to 2, usually 2 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0xa02` | 5 | 1 to 11, usually 2 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0xc19` | 5 | 3 to 4, usually 4 | 0 | 0 | 5 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x2a5` | 4 | 1 to 2, usually 1 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x816` | 4 | 1 to 2, usually 1 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x217` | 3 | 1 to 4, usually 4 | 0 | 0 | 1 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x2bd` | 3 | 1 to 1, usually 1 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x312` | 3 | 8 to 8, usually 8 | 3 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x886` | 3 | 1 to 2, usually 1 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x899` | 3 | 1 to 12, usually 1 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x942` | 3 | 1 to 1, usually 1 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x948` | 3 | 1 to 1, usually 1 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x287` | 2 | 6 to 6, usually 6 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x28c` | 2 | 2 to 2, usually 2 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x306` | 2 | 1 to 1, usually 1 | 2 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x811` | 2 | 5 to 5, usually 5 | 0 | 0 | 2 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0xa07` | 2 | 1 to 4, usually 1 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0xd09` | 2 | 1 to 5, usually 1 | 2 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0xd12` | 2 | 8 to 8, usually 8 | 2 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `IADD` | 1 | 5 to 5, usually 5 | 0 | 0 | 1 | - | 0 of 0 | 0 | 0 | 0 of 1 | 0 of 1 |
| `MOV` | 1 | 2 to 2, usually 2 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 1 | 0 of 1 |
| `no form, key 0x3c4` | 1 | 1 to 1, usually 1 | 1 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x887` | 1 | 4 to 4, usually 4 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x88c` | 1 | 4 to 4, usually 4 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x896` | 1 | 6 to 6, usually 6 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x958` | 1 | 5 to 5, usually 5 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x95c` | 1 | 5 to 5, usually 5 | 0 | 0 | 1 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0x9a8` | 1 | 1 to 1, usually 1 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0xabb` | 1 | 2 to 2, usually 2 | 0 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
| `no form, key 0xb12` | 1 | 8 to 8, usually 8 | 1 | 0 | 0 | - | 0 of 0 | 0 | 0 | 0 of 0 | 0 of 0 |
