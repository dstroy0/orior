// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// constant_check.c: rule 13, a read of the constant bank held before the part, read through the vendor's own
// instruction set
//
//   inside     a read of bank 0 whose word ends inside the bank passes
//   past       one whose word ends past the bank's size is held, and the last word of the bank passes
//   bank       a read of a bank other than 0 is held, whatever its offset
//   unknown    a bank size of 0 is unknown, and only the bank is held against
//   none       an encoding of a class with no constant operand, or of no class, passes
//
//     constant_check
#include "../../../../../../../src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_safe.h"
#include "../../../../../../../src/cu/transpiler/vendor_bin_layouts/nvidia/tables/read.h"

#include <stdio.h>
#include <string.h>

static unsigned int s_checks = 0u;
static unsigned int s_failed = 0u;

static void check_that(int held, const char *what)
{
    s_checks += 1u;
    if (held == 0)
    {
        s_failed += 1u;
        printf("  FAILED: %s\n", what);
    }
}

// a move off the constant bank and a move of an immediate, as the vendor's text gives them for the part
static const char s_text[] = "CLASS \"mov__RC\"\n"
                             "FORMAT PREDICATE @[!]Predicate(PT):Pg Opcode\n"
                             "Register:Rd\n"
                             "','C:Sb[UImm(5/0*):Sb_bank]*[SImm(17)*:Sb_addr]\n"
                             "$( { '?' USCHED_INFO(\"DRAIN\"):usched_info } )$;\n"
                             "OPCODES\n"
                             "        MOV =  0b101000000010;\n"
                             "ENCODING\n"
                             "BITS_13_91_91_11_0_opcode=Opcode;\n"
                             "BITS_3_14_12_Pg = Pg;\n"
                             "BITS_8_23_16_Rd=Rd;\n"
                             "BITS_5_58_54_Sc_bank,BITS_14_53_40_Sc_addr=ConstBankAddress2(Sb_bank,Sb_addr);\n"
                             "\n"
                             "CLASS \"mov__RI\"\n"
                             "FORMAT PREDICATE @[!]Predicate(PT):Pg Opcode\n"
                             "Register:Rd\n"
                             "','UImm(32)*:Sb;\n"
                             "OPCODES\n"
                             "        MOV =  0b100000000010;\n"
                             "ENCODING\n"
                             "BITS_13_91_91_11_0_opcode=Opcode;\n"
                             "BITS_8_23_16_Rd=Rd;\n"
                             "BITS_32_63_32_Ra_offset=Sb;\n"
                             "\n";

// a move off the constant bank: `MOV R7, c[<bank>][<words> * 4]`
static void constant_move(const TablesSet *set, unsigned long long bank, unsigned long long words,
                          unsigned long long *low, unsigned long long *high)
{
    const TablesClass *const class_ = &set->class_[0];
    *low = 0ull;
    *high = 0ull;
    tables_field_write(tables_field_named(class_, "opcode"), low, high, 0xa02ull);
    tables_field_write(tables_field_named(class_, "Pg"), low, high, 7ull);
    tables_field_write(tables_field_named(class_, "Rd"), low, high, 7ull);
    tables_field_write(tables_field_named(class_, "Sc_bank"), low, high, bank);
    tables_field_write(tables_field_named(class_, "Sc_addr"), low, high, words);
}

int main(void)
{
    TablesSet set = {NULL, 0u, 0u};
    check_that(tables_set_read(&set, (const unsigned char *)s_text, sizeof(s_text) - 1u) && (set.count == 2u),
               "the two classes read");
    unsigned long long low = 0ull;
    unsigned long long high = 0ull;
    constant_move(&set, 0ull, 0ull, &low, &high);
    check_that(low == 0x0000000000077a02ull, "inside: the move of c[0x0][0x0] encodes as the part's does");
    check_that(!cubin_constant_out_of_bank(&set, low, high, 0x160ull), "inside: c[0x0][0x0] passes a bank of 0x160");
    constant_move(&set, 0ull, 0x57ull, &low, &high);
    check_that(!cubin_constant_out_of_bank(&set, low, high, 0x160ull), "past: the bank's last word, 0x15c, passes");
    constant_move(&set, 0ull, 0x58ull, &low, &high);
    check_that(cubin_constant_out_of_bank(&set, low, high, 0x160ull), "past: the word at 0x160 is held");
    constant_move(&set, 0ull, 0x2000ull, &low, &high);
    check_that(cubin_constant_out_of_bank(&set, low, high, 0x160ull), "past: a word far past the bank is held");
    constant_move(&set, 3ull, 0ull, &low, &high);
    check_that(cubin_constant_out_of_bank(&set, low, high, 0x160ull), "bank: bank 3 is held");
    constant_move(&set, 0ull, 0x2000ull, &low, &high);
    check_that(!cubin_constant_out_of_bank(&set, low, high, 0ull), "unknown: a bank of unknown size holds no offset");
    constant_move(&set, 1ull, 0ull, &low, &high);
    check_that(cubin_constant_out_of_bank(&set, low, high, 0ull), "unknown: and holds a bank other than 0 still");
    low = 0ull;
    high = 0ull;
    tables_field_write(tables_field_named(&set.class_[1], "opcode"), &low, &high, 0x802ull);
    tables_field_write(tables_field_named(&set.class_[1], "Ra_offset"), &low, &high, 0xffffffffull);
    check_that(!cubin_constant_out_of_bank(&set, low, high, 0x160ull), "none: a move of an immediate passes");
    low = 0ull;
    high = 0ull;
    tables_field_write(tables_field_named(&set.class_[1], "opcode"), &low, &high, 0x7ffull);
    check_that(!cubin_constant_out_of_bank(&set, low, high, 0x160ull), "none: an encoding of no class passes");
    tables_set_freed(&set);

    printf("constant_check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
