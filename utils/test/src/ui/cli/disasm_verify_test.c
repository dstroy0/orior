// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// disasm_verify_test.c: the verification's readers checked on texts of the shapes their tools print: the functions an
// IR text defines, a function's symbol among llvm-nm's lines, plain or as link-time optimization promotes it, the
// instructions of a function as llvm-objdump prints them and as the plugin ghidra's script writes them, prefixes and
// the two readers' spellings taken as one, and two readings held the same or told where they first differ. The loop
// itself, which builds the tree and runs Ghidra, runs where ORIOR_DISASM_LOOP names a module, and is not run otherwise
#include "../../../../../ui/cli/src/command_line/command_line_internal.h"
#include "../../../../../ui/cli/src/programs/disasm_verify.h"
#include "cli_check.h"

#include <stdlib.h>

static CliDisasmInstruction s_one[CLI_DISASM_INSTRUCTIONS];
static CliDisasmInstruction s_other[CLI_DISASM_INSTRUCTIONS];
static char s_names[CLI_DISASM_FUNCTIONS][CLI_NAME_LONGEST];

static const char IR[] = "; ModuleID = 'cells.c'\n"
                         "declare void @cli_lock_take(ptr)\n"
                         "define internal ptr @cells_taken(ptr %tally, i64 %bytes) {\n"
                         "  ret ptr null\n"
                         "}\n"
                         "define dso_local void @cli_cell_given(ptr noundef %cell) #0 {\n"
                         "  ret void\n"
                         "}\n";

static const char SYMBOLS[] = "0000000000401000 T main\n"
                              "0000000000401100 t cells_taken.llvm.1234567\n"
                              "0000000000401200 T cli_cell_given\n";

static const char OBJDUMP[] = "\n/top/orior_cli:\tfile format elf64-x86-64\n\nDisassembly of section .text:\n\n"
                              "0000000000401200 <cli_cell_given>:\n"
                              "  401200: 55                           \tpush\trbp\n"
                              "  401201: 48 89 e5                     \tmov\trbp, rsp\n"
                              "  401204: 74 0a                        \tje\t0x401210 <cli_cell_given+0x10>\n"
                              "  401206: f0 48 0f b1 37               \tlock\t\tcmpxchg\tqword ptr [rdi], rsi\n"
                              "  40120b: 48 b8 89 67 45 23 01 00 00 00\tmovabs\trax, 0x123456789\n"
                              "  401215: c3                           \tret\n"
                              "  401216: cc                           \tint3\n"
                              "  401217: cc                           \tint3\n"
                              "\n0000000000401100 <cells_taken.llvm.1234567>:\n"
                              "  401100: 90                           \tnop\n"
                              "\n0000000000401300 <cells_report>:\n"
                              "  401300: eb 0e                        \tjmp\t0x401310 <cells_report+0x10>\n"
                              "  401302: 66 2e 0f 1f 84 00 00 00 00 00\tnop\tword ptr cs:[rax + rax]\n"
                              "  40130c: 0f 1f 40 00                  \tnop\tdword ptr [rax]\n"
                              "  401310: b8 01 00 00 00               \tmov\teax, 0x1\n"
                              "  401315: c3                           \tret\n";

// Ghidra's units: the tail after cli_cell_given's return and the fill after cells_report's jump raw, the latter in two
// runs that cut the ten-byte nop between them
static const char GHIDRA[] = "== cli_cell_given\n"
                             "4198912 1 PUSH\n"
                             "4198913 3 MOV\n"
                             "4198916 2 JZ\n"
                             "4198918 5 CMPXCHG.LOCK\n"
                             "4198923 10 MOV\n"
                             "4198933 1 RET\n"
                             "4198934 2 ? cccc\n"
                             "== cells_taken.llvm.1234567\n"
                             "4198656 1 NOP\n"
                             "== cells_report\n"
                             "4199168 2 JMP\n"
                             "4199170 8 ? 662e0f1f84000000\n"
                             "4199178 6 ? 00000f1f4000\n"
                             "4199184 5 MOV\n"
                             "4199189 1 RET\n";

int main(void)
{
    const unsigned int defined = cli_disasm_functions_defined(IR, sizeof(IR) - 1u, s_names, CLI_DISASM_FUNCTIONS);
    cli_check(defined == 2u, "the IR defines two functions, its declaration left out");
    cli_check_text(s_names[0], 11ull, "cells_taken", "the first defined is cells_taken");
    cli_check_text(s_names[1], 14ull, "cli_cell_given", "the second is cli_cell_given");

    char symbol[CLI_NAME_LONGEST];
    cli_check(cli_disasm_symbol_of(SYMBOLS, sizeof(SYMBOLS) - 1u, "cli_cell_given", symbol),
              "a function the program defines has its symbol");
    cli_check_text(symbol, 14ull, "cli_cell_given", "which is its own name");
    cli_check(cli_disasm_symbol_of(SYMBOLS, sizeof(SYMBOLS) - 1u, "cells_taken", symbol),
              "a promoted local has its symbol");
    cli_check_text(symbol, 24ull, "cells_taken.llvm.1234567",
                   "which carries the suffix link-time optimization gave it");
    cli_check(!cli_disasm_symbol_of(SYMBOLS, sizeof(SYMBOLS) - 1u, "cells_given", symbol),
              "a function the program lacks has none");

    CliDisasmReading one = {s_one, 0u, 0};
    CliDisasmReading other = {s_other, 0u, 0};
    cli_check(cli_disasm_objdump_read(OBJDUMP, sizeof(OBJDUMP) - 1u, "cli_cell_given", &one),
              "llvm-objdump's text holds the function");
    cli_check(one.count == 8u, "with six instructions and the two of the fill after its return");
    cli_check((one.instruction[1].length == 3u) && (one.instruction[1].byte[0] == 0x48u) &&
                  (one.instruction[1].byte[2] == 0xe5u),
              "an instruction carries its bytes, three of them for the move");
    cli_check(one.instruction[0].address == 0x401200ull, "the first at the function's address");
    cli_check_text(one.instruction[3].mnemonic, 7ull, "cmpxchg", "a lock prefix is passed over for the instruction");
    cli_check(cli_disasm_ghidra_read(GHIDRA, sizeof(GHIDRA) - 1u, "cli_cell_given", &other),
              "Ghidra's text holds the function");
    cli_check(other.count == 7u, "with six instructions and one raw run");
    cli_check((other.instruction[6].mnemonic[0] == '\0') && (other.instruction[6].length == 2u) &&
                  (other.instruction[6].byte[0] == 0xccu) && (other.instruction[6].byte[1] == 0xccu),
              "the raw run has no mnemonic and its two bytes");
    cli_check(other.instruction[1].length == 3u, "an instruction row carries the length it states");
    unsigned int at = 0u;
    unsigned int other_at = 0u;
    cli_check(cli_disasm_same(&one, &other, &at, &other_at),
              "the two readings are the same, je as JZ, movabs as MOV, the lock as "
              "a suffix, the raw tail read byte for byte against the fill");
    other.count = 5u;
    cli_check(!cli_disasm_same(&one, &other, &at, &other_at) && (at == 5u),
              "a return past the shorter reading's end is no fill, and differs there");
    cli_check(cli_disasm_ghidra_read(GHIDRA, sizeof(GHIDRA) - 1u, "cli_cell_given", &other),
              "Ghidra's text read again");

    cli_check(cli_disasm_objdump_read(OBJDUMP, sizeof(OBJDUMP) - 1u, "cells_taken.llvm.1234567", &one),
              "a promoted local is read by its symbol");
    cli_check(one.count == 1u, "one instruction");
    other.count = 1u;
    other.instruction[0].address = 0x401100ull;
    other.instruction[0].mnemonic[0] = 'R';
    other.instruction[0].mnemonic[1] = 'E';
    other.instruction[0].mnemonic[2] = 'T';
    other.instruction[0].mnemonic[3] = '\0';
    cli_check(!cli_disasm_same(&one, &other, &at, &other_at) && (at == 0u),
              "a different mnemonic differs at its index");
    other.count = 0u;
    cli_check(!cli_disasm_same(&one, &other, &at, &other_at) && (at == 0u),
              "a reading that ends first differs at its end");

    cli_check(cli_disasm_objdump_read(OBJDUMP, sizeof(OBJDUMP) - 1u, "cells_report", &one) && (one.count == 5u),
              "a function with fill after a jump within it reads five instructions from llvm-objdump");
    cli_check(cli_disasm_ghidra_read(GHIDRA, sizeof(GHIDRA) - 1u, "cells_report", &other) && (other.count == 5u),
              "and five from Ghidra, two of them raw runs that cut the ten-byte nop");
    cli_check(cli_disasm_same(&one, &other, &at, &other_at),
              "the two readings are the same, the fill read byte for byte across the cut");
    other.instruction[2].byte[3] = 0x90u;
    cli_check(!cli_disasm_same(&one, &other, &at, &other_at) && (at == 2u) && (other_at == 2u),
              "a raw byte that differs from the instruction over it differs at both readers' units");
    cli_check(cli_disasm_ghidra_read(GHIDRA, sizeof(GHIDRA) - 1u, "cells_report", &other) && (other.count == 5u),
              "Ghidra's text read again");
    other.instruction[3].length = 4u;
    cli_check(!cli_disasm_same(&one, &other, &at, &other_at) && (at == 3u), "an instruction of another length differs");
    other.instruction[3].length = 5u;
    other.count = 3u;
    cli_check(!cli_disasm_same(&one, &other, &at, &other_at) && (at == 3u),
              "instructions past Ghidra's raw runs that Ghidra lacks differ");
    other.instruction[5].address = 0x401316ull;
    other.instruction[5].length = 1u;
    other.instruction[5].byte[0] = 0xccu;
    other.instruction[5].mnemonic[0] = '\0';
    other.count = 6u;
    cli_check(cli_disasm_same(&one, &other, &at, &other_at), "a raw run past llvm-objdump's reading is passed over");

    const char *const module = getenv("ORIOR_DISASM_LOOP");
    if ((module != NULL) && (module[0] != '\0'))
    {
        const char *const words[1] = {module};
        cli_check(cli_disasm_verify_words(words, 1u) == 0, "the loop over the module names every function the same");
    }
    else
    {
        cli_out("not run: the loop over a module, which ORIOR_DISASM_LOOP names where it runs");
    }
    return cli_checked();
}
