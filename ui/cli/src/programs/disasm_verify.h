// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// disasm_verify.h: `orior_cli run disasm-verify <module.c> [--function <name>]...`, the sound hardware and compiler
// analysis loop's verification of one C module of the tree: the tree's command line built with link-time optimization
// through ui/cli/build.sh, the module's bitcode object lifted to IR with llvm-dis, the functions the IR defines listed,
// their instructions in the linked program read with llvm-objdump and with Ghidra's headless analyzer running the
// plugin ghidra's script, the program executed, and the two readings compared function by function on their
// instructions' addresses and mnemonics (disasm_verify.c, disasm_verify_steps.c, disasm_verify_read.c). One row a
// function to the output, the function, how many instructions each reader gave, and "same" or where they first differ,
// each reader's index, address and mnemonic there; exit 0 where every function reads the same, 1 where one differs,
// CLI_NO_CODE where a tool is missing or a step fails, which is said to the errors
#ifndef DISASM_VERIFY_H
#define DISASM_VERIFY_H

#include "../command_line/cli_internal.h"

// the most functions of one module compared, the most units of one function read, the longest mnemonic, and the most
// bytes of one unit: the longest instruction of the architecture, and the most bytes of one raw run the plugin's
// script writes
#define CLI_DISASM_FUNCTIONS 512u

#define CLI_DISASM_INSTRUCTIONS 65536u

#define CLI_DISASM_MNEMONIC 24u

#define CLI_DISASM_BYTES 16u

// One unit of a function as a reader gave it: its address in the program, its bytes, `length` of them, and its
// mnemonic as the reader spells it; or, with an empty mnemonic, a run of bytes the reader did not disassemble, which
// Ghidra's flow never reached and which the comparison reads byte for byte against the other reader's instructions
typedef struct
{
    unsigned long long address;
    unsigned int length;
    unsigned char byte[CLI_DISASM_BYTES];
    char mnemonic[CLI_DISASM_MNEMONIC];
} CliDisasmInstruction;

// The units of one function as one reader gave them, `count` of them, in a cell of CLI_DISASM_INSTRUCTIONS
typedef struct
{
    CliDisasmInstruction *instruction;
    unsigned int count;
    int cut;
} CliDisasmReading;

// the command: its code to exit with
int cli_disasm_verify_words(const char *const *words, unsigned int count);

// the functions the IR text `ir`, `size` bytes, defines, each `define` of a function that is no declaration, their
// names into `names` up to `room`: how many, those past the room left out (disasm_verify_read.c)
unsigned int cli_disasm_functions_defined(const char *ir, unsigned long long size, char (*names)[CLI_NAME_LONGEST],
                                          unsigned int room);
// the symbol of `name` in the program's symbols as llvm-nm lists them, `text` of `size` bytes: `name` itself where the
// program defines it, else `name` with the suffix link-time optimization gives a promoted local, as name.llvm.1234;
// into `symbol`, which holds CLI_NAME_LONGEST: 1, or 0 where the program has no such symbol
int cli_disasm_symbol_of(const char *text, unsigned long long size, const char *name, char *symbol);
// the instructions of the function `symbol` read from llvm-objdump's output with each instruction's bytes shown, `text`
// of `size` bytes, into `reading`, whose cell holds CLI_DISASM_INSTRUCTIONS: 1, or 0 where the output holds no such
// function
int cli_disasm_objdump_read(const char *text, unsigned long long size, const char *symbol, CliDisasmReading *reading);
// the units of the function from the file the plugin's script wrote, `text` of `size` bytes: its instructions, and the
// runs of bytes Ghidra left raw
int cli_disasm_ghidra_read(const char *text, unsigned long long size, const char *symbol, CliDisasmReading *reading);
// whether the reading `one`, llvm-objdump's, and `other`, Ghidra's, give the same function: the same addresses in the
// same order and, at each, mnemonics that name the same instruction, the two readers' spellings of one instruction
// taken as one, with the same length where both give one; and the bytes of each raw run Ghidra gives the same, byte
// for byte, as the instructions llvm-objdump reads at those addresses, so that what Ghidra's flow never reached is
// read all the same. Raw runs Ghidra gives past the end of llvm-objdump's reading are bytes that reading made no
// claim on, and are passed over: 1, or 0 with the index of the first unit that differs in each reading into `at` and
// `other_at`, a reading's count where it ends first
int cli_disasm_same(const CliDisasmReading *one, const CliDisasmReading *other, unsigned int *at,
                    unsigned int *other_at);

// a path made of `parts`, each put after the one before, into `path`, which holds CLI_PATH_LONGEST: 1, or 0 where it
// does not fit (disasm_verify.c)
int cli_disasm_path(char *path, const char *const *parts);

// the steps, each a program run with the toolchains' PATH within its patience, and said with its errors' first bytes
// where it ends with a code other than 0 (disasm_verify_steps.c): the PATH read from the toolchains before the first
void cli_disasm_steps_begun(void);
// a program of `names` beside the compiler at `compiler`, into `path`: 1, or 0 with which said
int cli_disasm_beside(const char *compiler, const char *const *names, char *path, CliLine *said);
// the tree's command line built with link-time optimization for `target` by build.sh, run with `compiler` as its CC:
// 1, or 0 with why said
int cli_disasm_built(const char *top, const char *target, const char *compiler, CliLine *said);
// the module's functions listed from its IR: the object lifted with the lifter into the IR file, and the file read for
// the functions it defines, into `names`: how many; or 0 with why said. With GNU's lifter, `gnu`, the list it prints
// stands as the IR's definitions
unsigned int cli_disasm_functions(int gnu, const char *lifter, const char *object, const char *ir,
                                  char (*names)[CLI_NAME_LONGEST], CliLine *said);
// the functions' symbols in `program`, listed by `lister`, each of `names` into `symbols`, a function the program
// does not hold left as an empty symbol: how many hold, or 0 with why said where the lister did not run
unsigned int cli_disasm_symbols(const char *lister, const char *program, const char (*names)[CLI_NAME_LONGEST],
                                char (*symbols)[CLI_NAME_LONGEST], unsigned int count, CliLine *said);
// the symbols' instructions read from `program` by `disassembler`, Intel's syntax with each instruction's bytes, as
// text the readers take, written to `into`: what the run gave, or NULL with why said
CliRan *cli_disasm_disassembled(const char *disassembler, const char *program, const char (*symbols)[CLI_NAME_LONGEST],
                                unsigned int count, const char *into, CliLine *said);
// Ghidra's analyzer run headless over `program`, loaded as `loader` says and analyzed by the script alone, in a project
// under `folder`, the plugin's script read from the tree at `top` and run with the symbols' names, its lines written to `into`:
// the file's bytes, `size` of them, or NULL with why said
const unsigned char *cli_disasm_ghidra(const char *analyzer, const char *const *loader, const char *top,
                                       const char *folder, const char *program, const char (*symbols)[CLI_NAME_LONGEST],
                                       unsigned int count, const char *into, unsigned long long *size, CliLine *said);
// the program executed, with `help`: 1 where it ends with 0, or 0 with why said
int cli_disasm_executed(const char *program, CliLine *said);

#endif
