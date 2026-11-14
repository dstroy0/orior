// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_disasm_verify.c: the verification's readers on the texts their tools print: the functions an IR text defines, a function's symbol
// among llvm-nm's lines, the instructions of a function as llvm-objdump prints them and as the ghidra plugin's script writes them, and two
// readings compared the same or reported where they first differ. The loop itself runs where ORIOR_DISASM_LOOP names a module
#include "ui/cli/src/command_line/command_line_internal.h"
#include "ui/cli/src/programs/disasm_verify.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

#include <stdlib.h>

static const char s_ir[] = "; ModuleID = 'cells.c'\n"
                           "declare void @cli_lock_take(ptr)\n"
                           "define internal ptr @cells_taken(ptr %owner, i64 %bytes) {\n"
                           "  ret ptr null\n"
                           "}\n"
                           "define dso_local void @cells_release(ptr noundef %cell) #0 {\n"
                           "  ret void\n"
                           "}\n";

static const char s_symbols[] = "0000000000401000 T main\n"
                                "0000000000401100 t cells_taken.llvm.1234567\n"
                                "0000000000401200 T cells_release\n";

static const char s_objdump[] = "\n/top/imp:\tfile format elf64-x86-64\n\nDisassembly of section .text:\n\n"
                                "0000000000401200 <cells_release>:\n"
                                "  401200: 55                           \tpush\trbp\n"
                                "  401201: 48 89 e5                     \tmov\trbp, rsp\n"
                                "  401204: 74 0a                        \tje\t0x401210 <cells_release+0x10>\n"
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

// Ghidra's units: the tail after cells_release's return and the fill after cells_report's jump raw, the latter in two runs that cut the
// ten-byte nop between them
static const char s_ghidra[] = "== cells_release\n"
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

static CliDisasmInstruction s_one_units[CLI_DISASM_INSTRUCTIONS];
static CliDisasmInstruction s_other_units[CLI_DISASM_INSTRUCTIONS];
static char s_names[CLI_DISASM_FUNCTIONS][CLI_NAME_MAX_LEN];
static CliDisasmReadState s_objdump_read_state;
static CliDisasmReadState s_ghidra_read_state;
static unsigned int s_at;
static unsigned int s_other_at;

// the function `name` read from both tools' texts
static void both_read(const char *name)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_disasm_verify_size.disasm_objdump_read, CliDisasmVerifySizeCfg, .text = s_objdump, .size = sizeof(s_objdump) - 1u, .symbol = name, .read_state = &s_objdump_read_state));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_disasm_verify_size.disasm_ghidra_read, CliDisasmVerifySizeCfg, .text = s_ghidra, .size = sizeof(s_ghidra) - 1u, .symbol = name, .read_state = &s_ghidra_read_state));
}

// whether the two readings are the same, where they differ in s_at and s_other_at
static int read_states_same(void)
{
    return EMBED_CALL(cli_disasm_verify_one.disasm_same, CliDisasmVerifyOneCfg, .one = &s_objdump_read_state, .other = &s_ghidra_read_state, .at = &s_at, .other_at = &s_other_at);
}

void setUp(void)
{
    cli_unity_begun();
    s_objdump_read_state = (CliDisasmReadState){s_one_units, 0u, 0};
    s_ghidra_read_state = (CliDisasmReadState){s_other_units, 0u, 0};
    s_at = 0u;
    s_other_at = 0u;
}

void tearDown(void)
{
}

void test_the_ir_defines_two_functions_and_skips_its_declaration(void)
{
    TEST_ASSERT_EQUAL_UINT(2u, EMBED_CALL(cli_disasm_verify_ir_size.disasm_functions_define, CliDisasmVerifyIrSizeCfg, .ir = s_ir, .size = sizeof(s_ir) - 1u, .names = s_names, .capacity = CLI_DISASM_FUNCTIONS));
    TEST_ASSERT_EQUAL_STRING("cells_taken", s_names[0]);
    TEST_ASSERT_EQUAL_STRING("cells_release", s_names[1]);
}

void test_a_defined_function_has_its_own_name_as_its_symbol(void)
{
    char symbol[CLI_NAME_MAX_LEN];
    TEST_ASSERT_TRUE(EMBED_CALL(cli_disasm_verify_size_text.disasm_symbol_get, CliDisasmVerifySizeTextCfg, .text = s_symbols, .size = sizeof(s_symbols) - 1u, .name = "cells_release", .symbol_out = symbol));
    TEST_ASSERT_EQUAL_STRING("cells_release", symbol);
}

void test_a_promoted_locals_symbol_has_the_link_time_suffix(void)
{
    char symbol[CLI_NAME_MAX_LEN];
    TEST_ASSERT_TRUE(EMBED_CALL(cli_disasm_verify_size_text.disasm_symbol_get, CliDisasmVerifySizeTextCfg, .text = s_symbols, .size = sizeof(s_symbols) - 1u, .name = "cells_taken", .symbol_out = symbol));
    TEST_ASSERT_EQUAL_STRING("cells_taken.llvm.1234567", symbol);
}

void test_a_function_the_program_lacks_has_no_symbol(void)
{
    char symbol[CLI_NAME_MAX_LEN];
    TEST_ASSERT_FALSE(EMBED_CALL(cli_disasm_verify_size_text.disasm_symbol_get, CliDisasmVerifySizeTextCfg, .text = s_symbols, .size = sizeof(s_symbols) - 1u, .name = "cells_given", .symbol_out = symbol));
}

void test_objdump_reads_six_instructions_and_two_of_fill(void)
{
    both_read("cells_release");
    TEST_ASSERT_EQUAL_UINT(8u, s_objdump_read_state.count);
}

void test_an_instruction_stores_its_bytes(void)
{
    both_read("cells_release");
    TEST_ASSERT_EQUAL_UINT(3u, s_objdump_read_state.instruction[1].length);
    TEST_ASSERT_EQUAL_HEX8(0x48u, s_objdump_read_state.instruction[1].byte[0]);
    TEST_ASSERT_EQUAL_HEX8(0xe5u, s_objdump_read_state.instruction[1].byte[2]);
}

void test_the_first_instruction_is_at_the_functions_address(void)
{
    both_read("cells_release");
    TEST_ASSERT_EQUAL_HEX64(0x401200ull, s_objdump_read_state.instruction[0].address);
}

void test_a_lock_prefix_is_skipped_for_its_instruction(void)
{
    both_read("cells_release");
    TEST_ASSERT_EQUAL_STRING("cmpxchg", s_objdump_read_state.instruction[3].mnemonic);
}

void test_ghidra_reads_six_instructions_and_one_raw_run(void)
{
    both_read("cells_release");
    TEST_ASSERT_EQUAL_UINT(7u, s_ghidra_read_state.count);
}

void test_a_raw_run_has_no_mnemonic_and_its_bytes(void)
{
    both_read("cells_release");
    TEST_ASSERT_EQUAL_CHAR('\0', s_ghidra_read_state.instruction[6].mnemonic[0]);
    TEST_ASSERT_EQUAL_UINT(2u, s_ghidra_read_state.instruction[6].length);
    TEST_ASSERT_EQUAL_HEX8(0xccu, s_ghidra_read_state.instruction[6].byte[0]);
    TEST_ASSERT_EQUAL_HEX8(0xccu, s_ghidra_read_state.instruction[6].byte[1]);
}

void test_a_ghidra_row_stores_the_length_it_states(void)
{
    both_read("cells_release");
    TEST_ASSERT_EQUAL_UINT(3u, s_ghidra_read_state.instruction[1].length);
}

void test_the_two_read_states_of_a_function_are_the_same(void)
{
    both_read("cells_release");
    TEST_ASSERT_TRUE(read_states_same());
}

void test_a_return_end_the_shorter_read_states_end_differs_there(void)
{
    both_read("cells_release");
    s_ghidra_read_state.count = 5u;
    TEST_ASSERT_FALSE(read_states_same());
    TEST_ASSERT_EQUAL_UINT(5u, s_at);
}

void test_a_promoted_local_is_read_by_its_symbol(void)
{
    both_read("cells_taken.llvm.1234567");
    TEST_ASSERT_EQUAL_UINT(1u, s_objdump_read_state.count);
}

void test_a_different_mnemonic_differs_at_its_index(void)
{
    both_read("cells_taken.llvm.1234567");
    memor.cpy(&(MemoriaCfg){.dst = s_ghidra_read_state.instruction[0].mnemonic, .src = "RET", .bytes = 4u});
    TEST_ASSERT_FALSE(read_states_same());
    TEST_ASSERT_EQUAL_UINT(0u, s_at);
}

void test_a_read_state_that_ends_first_differs_at_its_end(void)
{
    both_read("cells_taken.llvm.1234567");
    s_ghidra_read_state.count = 0u;
    TEST_ASSERT_FALSE(read_states_same());
    TEST_ASSERT_EQUAL_UINT(0u, s_at);
}

void test_fill_after_a_jump_reads_five_instructions_from_objdump(void)
{
    both_read("cells_report");
    TEST_ASSERT_EQUAL_UINT(5u, s_objdump_read_state.count);
}

void test_fill_after_a_jump_reads_five_units_from_ghidra(void)
{
    both_read("cells_report");
    TEST_ASSERT_EQUAL_UINT(5u, s_ghidra_read_state.count);
}

void test_the_read_states_are_the_same_across_a_cut_in_the_fill(void)
{
    both_read("cells_report");
    TEST_ASSERT_TRUE(read_states_same());
}

void test_a_differing_raw_byte_is_reported_at_both_readers_units(void)
{
    both_read("cells_report");
    s_ghidra_read_state.instruction[2].byte[3] = 0x90u;
    TEST_ASSERT_FALSE(read_states_same());
    TEST_ASSERT_EQUAL_UINT(2u, s_at);
    TEST_ASSERT_EQUAL_UINT(2u, s_other_at);
}

void test_an_instruction_of_another_length_differs(void)
{
    both_read("cells_report");
    s_ghidra_read_state.instruction[3].length = 4u;
    TEST_ASSERT_FALSE(read_states_same());
    TEST_ASSERT_EQUAL_UINT(3u, s_at);
}

void test_instructions_ghidra_lacks_end_its_raw_runs_differ(void)
{
    both_read("cells_report");
    s_ghidra_read_state.count = 3u;
    TEST_ASSERT_FALSE(read_states_same());
    TEST_ASSERT_EQUAL_UINT(3u, s_at);
}

void test_a_raw_run_end_objdumps_read_state_is_skipped(void)
{
    both_read("cells_report");
    s_ghidra_read_state.instruction[5].address = 0x401316ull;
    s_ghidra_read_state.instruction[5].length = 1u;
    s_ghidra_read_state.instruction[5].byte[0] = 0xccu;
    s_ghidra_read_state.instruction[5].mnemonic[0] = '\0';
    s_ghidra_read_state.count = 6u;
    TEST_ASSERT_TRUE(read_states_same());
}

void test_the_loop_over_a_module_finds_every_function_the_same(void)
{
    const char *const module = getenv("ORIOR_DISASM_LOOP");
    if ((module == NULL) || (module[0] == '\0'))
    {
        TEST_IGNORE_MESSAGE("ORIOR_DISASM_LOOP names no module");
    }
    const char *const words[1] = {module};
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_disasm_verify_words_table.words, CliDisasmVerifyWordsCfg, .words = words, .count = 1u));
}
