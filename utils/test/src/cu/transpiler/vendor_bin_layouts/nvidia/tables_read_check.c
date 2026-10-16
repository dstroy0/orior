// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tables_read_check.c: the vendor's instruction set read: its classes, their opcodes and fields, and the class an
// encoding belongs to
//
//   classes    every CLASS of the text is read with its name and its opcode, the opcode's leading bits as the
//              OPCODES line gives them
//   fields     a field is its runs of bits high to low and the operand written in it, a value fixed by the class
//              read as such; a field of two runs reads and writes across both
//   constant   a FORMAT holding a `C:` operand names its bank and offset fields; one holding none names none
//   class of   an encoding belongs to the first class whose opcode field reads as its opcode, and to none where
//              no class's does
//
//     tables_read_check
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

// two classes as the vendor's text gives them: a move of an immediate, and a load off the constant bank
static const char s_text[] = "ARCHITECTURE \"Test\"\n"
                             "\n"
                             "CLASS \"mov__RI\"\n"
                             "FORMAT PREDICATE @[!]Predicate(PT):Pg Opcode\n"
                             "Register:Rd\n"
                             "','UImm(32)*:Sb\n"
                             "$( { '?' USCHED_INFO(\"DRAIN\"):usched_info } )$;\n"
                             "\n"
                             "CONDITIONS\n"
                             "\n"
                             "OOR_REG_ERROR\n"
                             " (((Rd)==`Register@RZ)||((Rd)<=(%MAX_REG_COUNT-1))) :\n"
                             " \"Register Rd is out of range\"\n"
                             "\n"
                             "OPCODES\n"
                             "        MOVint_pipe =  0b100000000010;\n"
                             "        MOV =  0b100000000010;\n"
                             "\n"
                             "ENCODING\n"
                             "!mov__RI_unused;\n"
                             "BITS_3_14_12_Pg = Pg;\n"
                             "BITS_13_91_91_11_0_opcode=Opcode;\n"
                             "BITS_8_23_16_Rd=Rd;\n"
                             "BITS_32_63_32_Ra_offset=Sb;\n"
                             "BITS_3_115_113_src_rel_sb=*7;\n"
                             "BITS_8_124_122_109_105_opex=TABLES_opex_1(batch_t,usched_info);\n"
                             "\n"
                             "\n"
                             "CLASS \"ldc__RaRZ\"\n"
                             "FORMAT PREDICATE @[!]Predicate(PT):Pg Opcode /SZ(\"32\"):sz\n"
                             "Register:Rd\n"
                             "','C:Sa[UImm(5/0*):Sa_bank]*   [ZeroRegister(\"RZ\"):Ra + SImm(17/0)*:Ra_offset]\n"
                             "$( { '&' REQ:req '=' BITSET(6/0x0000):req_bit_set } )$;\n"
                             "\n"
                             "OPCODES\n"
                             "        LDC =  0b101110000010;\n"
                             "\n"
                             "ENCODING\n"
                             "BITS_13_91_91_11_0_opcode=Opcode;\n"
                             "BITS_8_23_16_Rd=Rd;\n"
                             "BITS_5_58_54_Sc_bank,BITS_14_53_40_Sc_addr=ConstBankAddress2(Sa_bank,Ra_offset);\n"
                             "\n";

int main(void)
{
    TablesSet set = {NULL, 0u, 0u};
    check_that(tables_set_read(&set, (const unsigned char *)s_text, sizeof(s_text) - 1u), "classes: the text reads");
    check_that(set.count == 2u, "classes: two classes read");
    const TablesClass *const mov = &set.class_[0];
    const TablesClass *const ldc = &set.class_[1];
    check_that((strcmp(mov->name, "mov__RI") == 0) && (mov->opcode == 0x802ull), "classes: the move's name and opcode");
    check_that((strcmp(ldc->name, "ldc__RaRZ") == 0) && (ldc->opcode == 0xb82ull),
               "classes: the load's name and opcode");

    check_that(mov->fields == 6u, "fields: the move's six encoding lines are its fields");
    const TablesField *const rd = tables_field_named(mov, "Rd");
    check_that((rd != NULL) && (rd->runs == 1u) && (rd->run[0].first == 16u) && (rd->run[0].last == 23u) &&
                   (rd->width == 8u) && !rd->fixed && (strcmp(rd->operand, "Rd") == 0),
               "fields: Rd is bits 16 to 23, written from the operand Rd");
    const TablesField *const fixed = tables_field_named(mov, "src_rel_sb");
    check_that((fixed != NULL) && fixed->fixed && (fixed->value == 7ull), "fields: a field written *7 is fixed at 7");
    const TablesField *const opex = tables_field_named(mov, "opex");
    check_that((opex != NULL) && (opex->runs == 2u) && (opex->run[0].first == 122u) && (opex->run[0].last == 124u) &&
                   (opex->run[1].first == 105u) && (opex->run[1].last == 109u) && (opex->width == 8u),
               "fields: opex is two runs, bits 122 to 124 over bits 105 to 109");
    unsigned long long low = 0ull;
    unsigned long long high = 0ull;
    tables_field_write(opex, &low, &high, 0xa5ull);
    check_that((tables_field_read(opex, low, high) == 0xa5ull) && (((high >> (122u - 64u)) & 7ull) == 5ull) &&
                   (((high >> (105u - 64u)) & 31ull) == 5ull),
               "fields: a value written across two runs reads back, its high bits in the high run");
    check_that(tables_field_named(mov, "Sa_bank") == NULL, "fields: a name the class has no field of is NULL");

    check_that((mov->bank[0] == '\0') && (mov->offset[0] == '\0'), "constant: the move holds no constant operand");
    check_that((strcmp(ldc->bank, "Sa_bank") == 0) && (strcmp(ldc->offset, "Ra_offset") == 0),
               "constant: the load's bank and offset fields are named off its FORMAT");
    // the two fields joined on one encoding line each take the operand's argument in their place
    const TablesField *const bank = tables_field_by_operand(ldc, ldc->bank);
    const TablesField *const offset = tables_field_by_operand(ldc, ldc->offset);
    check_that((bank != NULL) && (strcmp(bank->name, "Sc_bank") == 0) && (bank->run[0].first == 54u) &&
                   (bank->run[0].last == 58u),
               "constant: the bank's field is the first of the joined line, bits 54 to 58");
    check_that((offset != NULL) && (strcmp(offset->name, "Sc_addr") == 0) && (offset->run[0].first == 40u) &&
                   (offset->run[0].last == 53u) && (offset->width == 14u),
               "constant: the offset's field is the second, bits 40 to 53");
    check_that(tables_field_by_operand(mov, "Sa_bank") == NULL, "constant: an operand no field takes is NULL");
    check_that((ldc->offset_bits == 17u) && (mov->offset_bits == 0u),
               "constant: the offset operand's width is read off the FORMAT, SImm(17), and a class with none holds 0");

    low = 0ull;
    high = 0ull;
    tables_field_write(tables_field_named(mov, "opcode"), &low, &high, 0x802ull);
    tables_field_write(rd, &low, &high, 7ull);
    check_that(tables_class_of(&set, low, high) == mov, "class of: an encoding with the move's opcode is the move's");
    low = 0ull;
    high = 0ull;
    tables_field_write(tables_field_named(ldc, "opcode"), &low, &high, 0xb82ull);
    check_that(tables_class_of(&set, low, high) == ldc, "class of: one with the load's opcode is the load's");
    tables_field_write(tables_field_named(ldc, "opcode"), &low, &high, 0x1b82ull);
    check_that(tables_class_of(&set, low, high) == NULL, "class of: one with bit 91 set is no class's");
    tables_set_freed(&set);
    check_that((set.class_ == NULL) && (set.count == 0u), "classes: the set freed holds nothing");

    printf("tables_read_check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
