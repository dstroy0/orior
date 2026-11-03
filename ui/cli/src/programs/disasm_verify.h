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
// CLI_NO_CODE where a tool is missing or a step fails, which is reported to the errors
#ifndef DISASM_VERIFY_H
#define DISASM_VERIFY_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliDisasmVerifyCountCfg CliDisasmVerifyCountCfg;
typedef struct CliDisasmVerifyCountErrMsgCfg CliDisasmVerifyCountErrMsgCfg;
typedef struct CliDisasmVerifyIrCfg CliDisasmVerifyIrCfg;
typedef struct CliDisasmVerifyCountErrMsgListerCfg CliDisasmVerifyCountErrMsgListerCfg;
typedef struct CliDisasmVerifyPathCfg CliDisasmVerifyPathCfg;
typedef struct CliDisasmVerifyCompilerCfg CliDisasmVerifyCompilerCfg;
typedef struct CliDisasmVerifyIrSizeCfg CliDisasmVerifyIrSizeCfg;
typedef struct CliDisasmVerifySizeCfg CliDisasmVerifySizeCfg;
typedef struct CliDisasmVerifyOneCfg CliDisasmVerifyOneCfg;
typedef struct CliDisasmVerifySizeTextCfg CliDisasmVerifySizeTextCfg;
typedef struct CliDisasmVerifyCfg CliDisasmVerifyCfg;
typedef struct CliDisasmVerifyWordsCfg CliDisasmVerifyWordsCfg;

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
} CliDisasmReadState;

// the command: its code to exit with
int cli_disasm_verify_words(const CliDisasmVerifyWordsCfg *args);

// the functions the IR text `ir`, `size` bytes, defines, each `define` of a function that is no declaration, their
// names into `names` up to `capacity`: how many, those past the capacity left out (disasm_verify_read.c)
unsigned int cli_disasm_functions_define(const CliDisasmVerifyIrSizeCfg *args);
// the symbol of `name` in the program's symbols as llvm-nm lists them, `text` of `size` bytes: `name` itself where the
// program defines it, else `name` with the suffix link-time optimization gives a promoted local, as name.llvm.1234;
// into `symbol`, sized for CLI_NAME_MAX_LEN: 1, or 0 where the program has no such symbol
int cli_disasm_symbol_get(const CliDisasmVerifySizeTextCfg *args);
// the instructions of the function `symbol` read from llvm-objdump's output with each instruction's bytes shown, `text`
// of `size` bytes, into `read_state`, whose cell is sized for CLI_DISASM_INSTRUCTIONS: 1, or 0 where the output
// contains no such function
int cli_disasm_objdump_read(const CliDisasmVerifySizeCfg *args);
// the units of the function from the file the plugin's script wrote, `text` of `size` bytes: its instructions, and the
// runs of bytes Ghidra left raw
int cli_disasm_ghidra_read(const CliDisasmVerifySizeCfg *args);
// whether the reading `one`, llvm-objdump's, and `other`, Ghidra's, give the same function: the same addresses in the
// same order and, at each, mnemonics that name the same instruction, the two readers' spellings of one instruction
// taken as one, with the same length where both give one; and the bytes of each raw run Ghidra gives the same, byte
// for byte, as the instructions llvm-objdump reads at those addresses, so that what Ghidra's flow never reached is
// read all the same. Raw runs Ghidra gives past the end of llvm-objdump's reading are bytes that reading made no
// claim on, and are passed over: 1, or 0 with the index of the first unit that differs in each reading into `at` and
// `other_at`, a reading's count where it ends first
int cli_disasm_same(const CliDisasmVerifyOneCfg *args);

// a path made of `parts`, each put after the one before, into `path`, sized for CLI_PATH_MAX_LEN: 1, or 0 where it
// does not fit (disasm_verify.c)
int cli_disasm_path(const CliDisasmVerifyCfg *args);

// the steps, each a program run with the toolchains' PATH within its patience, and reported with its errors' first
// bytes where it ends with a code other than 0 (disasm_verify_steps.c): the PATH read from the toolchains before the
// first
void cli_disasm_steps_begin(void);
// a program of `names` beside the compiler at `compiler`, into `path`: 1, or 0 with which given
int cli_disasm_adjacent(const CliDisasmVerifyPathCfg *args);
// the tree's command line built with link-time optimization for `target` by build.sh, run with `compiler` as its CC:
// 1, or 0 with why given
int cli_disasm_build(const CliDisasmVerifyCompilerCfg *args);
// the module's functions listed from its IR: the object lifted with the lifter into the IR file, and the file read for
// the functions it defines, into `names`: how many; or 0 with why given. With GNU's lifter, `gnu`, the list it prints
// stands as the IR's definitions
unsigned int cli_disasm_functions(const CliDisasmVerifyIrCfg *args);
// the functions' symbols in `program`, listed by `lister`, each of `names` into `symbols`, a function the program
// does not contain left as an empty symbol: how many contain, or 0 with why given where the lister did not run
unsigned int cli_disasm_symbols(const CliDisasmVerifyCountErrMsgListerCfg *args);
// the symbols' instructions read from `program` by `disassembler`, Intel's syntax with each instruction's bytes, as
// text the readers take, written to `into`: what the run gave, or NULL with why given
CliRunResult *cli_disasm_disassemble(const CliDisasmVerifyCountErrMsgCfg *args);
// Ghidra's analyzer run headless over `program`, loaded as `loader` specifies and analyzed by the script alone, in a
// project under `folder`, the plugin's script read from the tree at `top` and run with the symbols' names, its lines
// written to `into`: the file's bytes, `size` of them, or NULL with why given
const unsigned char *cli_disasm_ghidra(const CliDisasmVerifyCountCfg *args);
// the program executed, with `help`: 1 where it ends with 0, or 0 with why given
int cli_disasm_execute(const CliDisasmVerifyCountErrMsgCfg *args);

struct CliDisasmVerifyCountCfg
{
    unsigned int count;
    VerbaLinea *err_msg;
    const char *top;
    const char *program;
    const char (*symbols)[CLI_NAME_MAX_LEN];
    const char *into;
    const char *analyzer;
    const char *const *loader;
    const char *folder;
    unsigned long long *size_pointer;
};

struct CliDisasmVerifyCountErrMsgCfg
{
    unsigned int count;
    VerbaLinea *err_msg;
    const char *program;
    const char *disassembler;
    const char (*symbols)[CLI_NAME_MAX_LEN];
    const char *into;
};

struct CliDisasmVerifyIrCfg
{
    const char *ir;
    char (*names)[CLI_NAME_MAX_LEN];
    VerbaLinea *err_msg;
    int gnu;
    const char *lifter;
    const char *object;
};

struct CliDisasmVerifyCountErrMsgListerCfg
{
    unsigned int count;
    VerbaLinea *err_msg;
    const char *lister;
    const char *program;
    const char (*names_callback)[CLI_NAME_MAX_LEN];
    char (*symbols_callback)[CLI_NAME_MAX_LEN];
};

struct CliDisasmVerifyPathCfg
{
    char *path;
    const char *compiler;
    const char *const *names_letters;
    VerbaLinea *err_msg;
};

struct CliDisasmVerifyCompilerCfg
{
    const char *compiler;
    VerbaLinea *err_msg;
    const char *top;
    const char *target;
};

struct CliDisasmVerifyIrSizeCfg
{
    const char *ir;
    unsigned long long size;
    char (*names)[CLI_NAME_MAX_LEN];
    unsigned int capacity;
};

struct CliDisasmVerifySizeCfg
{
    unsigned long long size;
    const char *text;
    const char *symbol;
    CliDisasmReadState *read_state;
};

struct CliDisasmVerifyOneCfg
{
    const CliDisasmReadState *one;
    const CliDisasmReadState *other;
    unsigned int *at;
    unsigned int *other_at;
};

struct CliDisasmVerifySizeTextCfg
{
    unsigned long long size;
    const char *text;
    const char *name;
    char *symbol_out;
};

struct CliDisasmVerifyCfg
{
    char *path;
    const char *const *parts;
};

struct CliDisasmVerifyWordsCfg
{
    const char *const *words;
    unsigned int count;
};

typedef struct
{
    int (*words)(const CliDisasmVerifyWordsCfg *args);
} CliDisasmVerifyWordsNs;
EMBED_TABLE_LAYOUT(CliDisasmVerifyWordsNs, words);
EMBED_TABLE_STORAGE CliDisasmVerifyWordsNs cli_disasm_verify_words_table EMBED_UNUSED = {
    .words = cli_disasm_verify_words,
};

typedef struct
{
    unsigned int (*disasm_functions_define)(const CliDisasmVerifyIrSizeCfg *args);
} CliDisasmVerifyIrSizeNs;
EMBED_TABLE_LAYOUT(CliDisasmVerifyIrSizeNs, disasm_functions_define);
EMBED_TABLE_STORAGE CliDisasmVerifyIrSizeNs cli_disasm_verify_ir_size EMBED_UNUSED = {
    .disasm_functions_define = cli_disasm_functions_define,
};

typedef struct
{
    int (*disasm_symbol_get)(const CliDisasmVerifySizeTextCfg *args);
} CliDisasmVerifySizeTextNs;
EMBED_TABLE_LAYOUT(CliDisasmVerifySizeTextNs, disasm_symbol_get);
EMBED_TABLE_STORAGE CliDisasmVerifySizeTextNs cli_disasm_verify_size_text EMBED_UNUSED = {
    .disasm_symbol_get = cli_disasm_symbol_get,
};

typedef struct
{
    int (*disasm_objdump_read)(const CliDisasmVerifySizeCfg *args);
    int (*disasm_ghidra_read)(const CliDisasmVerifySizeCfg *args);
} CliDisasmVerifySizeNs;
EMBED_TABLE_LAYOUT(CliDisasmVerifySizeNs, disasm_objdump_read, disasm_ghidra_read);
EMBED_TABLE_STORAGE CliDisasmVerifySizeNs cli_disasm_verify_size EMBED_UNUSED = {
    .disasm_objdump_read = cli_disasm_objdump_read,
    .disasm_ghidra_read = cli_disasm_ghidra_read,
};

typedef struct
{
    int (*disasm_same)(const CliDisasmVerifyOneCfg *args);
} CliDisasmVerifyOneNs;
EMBED_TABLE_LAYOUT(CliDisasmVerifyOneNs, disasm_same);
EMBED_TABLE_STORAGE CliDisasmVerifyOneNs cli_disasm_verify_one EMBED_UNUSED = {
    .disasm_same = cli_disasm_same,
};

typedef struct
{
    int (*disasm_path)(const CliDisasmVerifyCfg *args);
} CliDisasmVerifyNs;
EMBED_TABLE_LAYOUT(CliDisasmVerifyNs, disasm_path);
EMBED_TABLE_STORAGE CliDisasmVerifyNs cli_disasm_verify EMBED_UNUSED = {
    .disasm_path = cli_disasm_path,
};

typedef struct
{
    int (*disasm_adjacent)(const CliDisasmVerifyPathCfg *args);
} CliDisasmVerifyPathNs;
EMBED_TABLE_LAYOUT(CliDisasmVerifyPathNs, disasm_adjacent);
EMBED_TABLE_STORAGE CliDisasmVerifyPathNs cli_disasm_verify_path EMBED_UNUSED = {
    .disasm_adjacent = cli_disasm_adjacent,
};

typedef struct
{
    int (*disasm_build)(const CliDisasmVerifyCompilerCfg *args);
} CliDisasmVerifyCompilerNs;
EMBED_TABLE_LAYOUT(CliDisasmVerifyCompilerNs, disasm_build);
EMBED_TABLE_STORAGE CliDisasmVerifyCompilerNs cli_disasm_verify_compiler EMBED_UNUSED = {
    .disasm_build = cli_disasm_build,
};

typedef struct
{
    unsigned int (*disasm_functions)(const CliDisasmVerifyIrCfg *args);
} CliDisasmVerifyIrNs;
EMBED_TABLE_LAYOUT(CliDisasmVerifyIrNs, disasm_functions);
EMBED_TABLE_STORAGE CliDisasmVerifyIrNs cli_disasm_verify_ir EMBED_UNUSED = {
    .disasm_functions = cli_disasm_functions,
};

typedef struct
{
    unsigned int (*disasm_symbols)(const CliDisasmVerifyCountErrMsgListerCfg *args);
} CliDisasmVerifyCountErrMsgListerNs;
EMBED_TABLE_LAYOUT(CliDisasmVerifyCountErrMsgListerNs, disasm_symbols);
EMBED_TABLE_STORAGE CliDisasmVerifyCountErrMsgListerNs cli_disasm_verify_count_err_msg_lister EMBED_UNUSED = {
    .disasm_symbols = cli_disasm_symbols,
};

typedef struct
{
    CliRunResult *(*disasm_disassemble)(const CliDisasmVerifyCountErrMsgCfg *args);
    int (*disasm_execute)(const CliDisasmVerifyCountErrMsgCfg *args);
} CliDisasmVerifyCountErrMsgNs;
EMBED_TABLE_LAYOUT(CliDisasmVerifyCountErrMsgNs, disasm_disassemble, disasm_execute);
EMBED_TABLE_STORAGE CliDisasmVerifyCountErrMsgNs cli_disasm_verify_count_err_msg EMBED_UNUSED = {
    .disasm_disassemble = cli_disasm_disassemble,
    .disasm_execute = cli_disasm_execute,
};

typedef struct
{
    const unsigned char *(*disasm_ghidra)(const CliDisasmVerifyCountCfg *args);
} CliDisasmVerifyCountNs;
EMBED_TABLE_LAYOUT(CliDisasmVerifyCountNs, disasm_ghidra);
EMBED_TABLE_STORAGE CliDisasmVerifyCountNs cli_disasm_verify_count EMBED_UNUSED = {
    .disasm_ghidra = cli_disasm_ghidra,
};

#endif
