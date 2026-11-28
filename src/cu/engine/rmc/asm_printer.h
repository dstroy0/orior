// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef ASM_PRINTER_H
#define ASM_PRINTER_H

// The text of the code generator as a record program, the work of the code generator where the programs run. See
// engine_table.md item 11(a), the code generator on the device. The core decides the forms of the ruleset of the
// language that a lane uses (codegen_core.h, MachineInstr). The assembly printer writes their text from the own written
// forms of the ruleset, arranged as tables, one lane to a byte.
//
// Each lane reads one form, by the index, finds which piece of the form holds its byte, and writes that byte. The byte
// is a character of a piece of the form text, or of the written form of a bank around a register number. Else it is a
// character of a word that the ruleset holds whole, or a decimal digit of a number. A lane past the end of its form
// writes 0, which no text holds. The text is the bytes of the lanes in lane order, with the zeros left out.
//
// The host arranges once what belongs to the ruleset, the target and the header alone (asm_printer_ruleset_build). The
// functions below arrange the records of the forms from the items. The device runs them (codegen_device.h), and the
// host oracle runs them (asm_printer_host), so the two arrange them from one source

#include "../../../../includes/file_defs/krs/ruleset_core_words.h"
#include "codegen_core.h"
#include "target.h"

// the arguments that one record of the assembly printer holds. Each is a word that the ruleset holds whole, or a number
// between two words. Those are the written form of a bank around a register number. A form with more placeholders takes
// a record for each four
#define ASM_PRINTER_PLACEHOLDERS 4u

// the fields of a record: the part, the first lane, then four for each placeholder
#define ASM_PRINTER_FIELDS (2u + (4u * ASM_PRINTER_PLACEHOLDERS))

// the tables of the program (AsmPrinterTable)
#define ASM_PRINTER_PROGRAM_TABLES 6u

// the numbers the program writes are 32-bit words, ten digits at max
#define ASM_PRINTER_DIGITS 10u

// A record of the assembly printer is one form, or four placeholders of one, as a part. It has the number of the part,
// and the first lane of the lanes that write it. It has the argument of each placeholder as the word before its
// number, the word after it, the number, and whether it has one.
//
// A part is the pieces of the form around its placeholders: the piece before each of its placeholders, then the piece
// after its last. That last piece is empty where the form goes on in the next part. Its bytes are seventeen atoms in
// order. They are a piece, then the word before of a placeholder, its number digits and its word after, four times,
// then the last piece
#define ASM_PRINTER_PIECES (ASM_PRINTER_PLACEHOLDERS + 1u)

#define ASM_PRINTER_ATOMS ((4u * ASM_PRINTER_PLACEHOLDERS) + 1u)

// the record's fields, their bits and where each begins
#define ASM_PRINTER_PART_BITS 16u
#define ASM_PRINTER_FIRST_AT 16u
#define ASM_PRINTER_FIRST_BITS 32u
#define ASM_PRINTER_PLACEHOLDER_AT 48u
#define ASM_PRINTER_PLACEHOLDER_BITS 80u
#define ASM_PRINTER_WORD_BITS 16u
#define ASM_PRINTER_NUMBER_BITS 32u
#define ASM_PRINTER_RECORD_BITS (ASM_PRINTER_PLACEHOLDER_AT + (ASM_PRINTER_PLACEHOLDERS * ASM_PRINTER_PLACEHOLDER_BITS))
#define ASM_PRINTER_RECORD_LIMBS ((ASM_PRINTER_RECORD_BITS + 31u) / 32u)

// the max lanes and records the assembly printer runs: a first lane and a record's number are 32-bit fields
#define ASM_PRINTER_LANES_MAX 0x80000000ULL

// the assembly printer: its key and layout, as keymath and key_schedule leave them, and what they took. The program
// keeps these so that the device arranges it again (layout_device, codegen_device.h). They are its steps, the widths
// and offsets of the fields, and its tables, each with a pointer at its values. Then the step that it outputs, a list
// of words in one cell of the pool, and their count
struct AsmPrinterWords
{
    unsigned int *words;
    unsigned long long count;
};

struct AsmPrinterProgram
{
    EngineRecordKey key;
    EngineRecordLayout layout;
    EngineRecordStep *steps;
    unsigned int step_count;
    unsigned int field_bits[ASM_PRINTER_FIELDS];
    unsigned int field_offset[ASM_PRINTER_FIELDS];
    EngineRecordTable tables[ASM_PRINTER_PROGRAM_TABLES];
    AsmPrinterWords values[ASM_PRINTER_PROGRAM_TABLES];
    unsigned int output;
};

// the argument of a placeholder as the program writes it. It has the word before its number and the word after, by
// their numbers among the words, and its number where it has one
struct AsmPrinterArgument
{
    unsigned int before;
    unsigned int after;
    unsigned int number;
    unsigned int numbered;
};

// what a ruleset writes, arranged once for a target and a header:
//
// - the assembly printer, and the scratch that the construct of each form takes (ruleset_scratch). That is none,
//   because the assembly printer gives an error on a ruleset of constructs.
// - the length of each word
// - the lanes of each part, and the lengths of its pieces
// - the parts of each form in its order, from `form_part_first[form]` to `form_part_first[form + 1]` of form_parts
// - the parameter that each of its placeholders takes, from `form_placeholder_first[form]` to
//   `form_placeholder_first[form + 1]` of placeholder_parameters
// - the words around the register number of each bank, and the word of each fixed register
// - the word "-" before the digits of a negative number, the hash of the target as the note writes it, and the part
//   of the header
// - the compute capability of the target and the version of NVRTC, as the note writes them
struct AsmPrinterRuleset
{
    AsmPrinterProgram program;
    AsmPrinterWords scratch;
    AsmPrinterWords word_lengths;
    AsmPrinterWords part_lanes;
    AsmPrinterWords form_part_first;
    AsmPrinterWords form_parts;
    AsmPrinterWords form_placeholder_first;
    AsmPrinterWords placeholder_parameters;
    unsigned int bank_before[REGCLASS_COUNT];
    unsigned int bank_after[REGCLASS_COUNT];
    unsigned int fixed_word[PHYSREG_COUNT];
    unsigned int minus_word;
    unsigned int hash_word;
    unsigned int header_part;
    unsigned int target_numbers[4];
};

// the same, as the layout of the records reads it, with its lists where the layout runs. They are in host memory for
// the host oracle, and in device memory for the device
struct AsmPrinterLists
{
    const unsigned int *word_lengths;
    const unsigned int *part_lanes;
    const unsigned int *form_part_first;
    const unsigned int *form_parts;
    const unsigned int *form_placeholder_first;
    const unsigned int *placeholder_parameters;
    unsigned int bank_before[REGCLASS_COUNT];
    unsigned int bank_after[REGCLASS_COUNT];
    unsigned int fixed_word[PHYSREG_COUNT];
    unsigned int minus_word;
    unsigned int hash_word;
    unsigned int header_part;
    unsigned int target_numbers[4];
};

// `bits` of `value` arranged into the record's words from bit `at`, which the record's words have clear
CODEGEN_CORE void asm_printer_bits(unsigned int *record, unsigned int at, unsigned int bits, unsigned int value)
{
    for (unsigned int bit = 0u; bit < bits; bit += 1u)
    {
        const unsigned int set = (value >> bit) & 1u;
        record[(at + bit) / 32u] |= set << ((at + bit) % 32u);
    }
}

// 1 where an item is one that the assembly printer writes: the header, or a form with as many arguments as it takes.
// That is as the write of the host requests it
CODEGEN_CORE int asm_printer_formed(const MachineInstr *item)
{
    return (item->form == ASM_PRINTER_ALL_ONES) ||
           ((item->form < OPCODE_COUNT) && (item->count == codegen_operand_count(item->form)));
}

// the placeholders a form's text takes arguments at
CODEGEN_CORE unsigned int asm_printer_placeholders(const AsmPrinterLists *forms, unsigned int form)
{
    return forms->form_placeholder_first[form + 1u] - forms->form_placeholder_first[form];
}

// the records a formed item takes: a form's placeholders four to a record and one for a form of none, and one for the
// header
CODEGEN_CORE unsigned int asm_printer_record_count(const AsmPrinterLists *forms, const MachineInstr *item)
{
    const unsigned int placeholders =
        (item->form == ASM_PRINTER_ALL_ONES) ? 0u : asm_printer_placeholders(forms, item->form);
    return (placeholders == 0u) ? 1u : ((placeholders + ASM_PRINTER_PLACEHOLDERS - 1u) / ASM_PRINTER_PLACEHOLDERS);
}

// argument `parameter` of a formed item as the program writes it, as the host code generator writes it
// (code_generator_text, code_generator.cu). A register is the words of its bank around its number, and a fixed register
// is its word. A number is its digits, and a negative one comes after the minus. The note comes from its step count and
// the target, and the resident from the layout of the launch
CODEGEN_CORE AsmPrinterArgument asm_printer_argument(const AsmPrinterLists *forms, const MachineInstr *item,
                                                     unsigned int parameter)
{
    AsmPrinterArgument argument = {0u, 0u, 0u, 1u};
    if (item->form == OPCODE_PROGRAM_NOTE)
    {
        argument.number = (parameter == 0u) ? item->arguments[0].number
                                            : ((parameter < 5u) ? forms->target_numbers[parameter - 1u] : 0u);
        if (parameter == 5u)
        {
            argument.before = forms->hash_word;
            argument.numbered = 0u;
        }
        return argument;
    }
    if (item->form == OPCODE_PROGRAM_UNIT)
    {
        // the launch's sizes and offsets and the program's commands and states are a few hundred at max
        argument.number = (unsigned int)codegen_unit(parameter);
        return argument;
    }
    const MachineOperand given = item->arguments[(parameter < MACHINE_INSTR_OPERANDS) ? parameter : 0u];
    if (given.type == OPERAND_REGISTER)
    {
        argument.before = forms->bank_before[given.which];
        argument.after = forms->bank_after[given.which];
        argument.number = given.number;
    }
    else if (given.type == OPERAND_PHYSREG)
    {
        argument.before = forms->fixed_word[given.which];
        argument.numbered = 0u;
    }
    else if ((given.type == OPERAND_SIGNED) && (given.number >= 0x80000000u))
    {
        argument.before = forms->minus_word;
        argument.number = 0u - given.number;
    }
    else
    {
        argument.number = given.number;
    }
    return argument;
}

// the records of a formed item, arranged from `record` on, as many as asm_printer_record_count gives, into the cleared
// `records`. Each record has its part and the arguments of its placeholders, and asm_printer_first arranges the first
// lane. The lanes that each record takes go into `record_lanes`: the characters of its pieces, the words of each
// placeholder, and ten digits for a number. The lanes past its text write those as 0
CODEGEN_CORE void asm_printer_records(const AsmPrinterLists *forms, const MachineInstr *item, unsigned long long record,
                                      unsigned int *records, unsigned long long *record_lanes)
{
    if (item->form == ASM_PRINTER_ALL_ONES)
    {
        asm_printer_bits(&records[record * ASM_PRINTER_RECORD_LIMBS], 0u, ASM_PRINTER_PART_BITS, forms->header_part);
        record_lanes[record] = forms->part_lanes[forms->header_part];
        return;
    }
    const unsigned int placeholders = asm_printer_placeholders(forms, item->form);
    const unsigned int placeholder_first = forms->form_placeholder_first[item->form];
    const unsigned int part_first = forms->form_part_first[item->form];
    const unsigned int chunks = asm_printer_record_count(forms, item);
    for (unsigned int chunk = 0u; chunk < chunks; chunk += 1u)
    {
        unsigned int *const chunk_record = &records[(record + chunk) * ASM_PRINTER_RECORD_LIMBS];
        const unsigned int part = forms->form_parts[part_first + chunk];
        asm_printer_bits(chunk_record, 0u, ASM_PRINTER_PART_BITS, part);
        unsigned long long lanes = forms->part_lanes[part];
        const unsigned int left = placeholders - (ASM_PRINTER_PLACEHOLDERS * chunk);
        const unsigned int taken = (left < ASM_PRINTER_PLACEHOLDERS) ? left : ASM_PRINTER_PLACEHOLDERS;
        for (unsigned int placeholder = 0u; placeholder < taken; placeholder += 1u)
        {
            const unsigned int place = (ASM_PRINTER_PLACEHOLDERS * chunk) + placeholder;
            const unsigned int parameter = forms->placeholder_parameters[placeholder_first + place];
            AsmPrinterArgument argument = asm_printer_argument(forms, item, parameter);
            if (RULESET_CORE_IS_SCRATCH(parameter))
            {
                // a scratch register, as the writer of the host takes it (ruleset_opcode_text). It is the own register
                // of the item from where the core left its bank. That is the k-th distinct one of the bank that the
                // text of the form names
                const unsigned int bank = RULESET_CORE_SCRATCH_BANK(parameter);
                const unsigned int bank_index =
                    (bank == REGCLASS_TEMPORARY)
                        ? 0u
                        : ((bank == REGCLASS_WIDE) ? 1u : ((bank == REGCLASS_PREDICATE) ? 2u : 3u));
                unsigned int first = place;
                for (unsigned int before = 0u; before < place; before += 1u)
                {
                    first =
                        ((first == place) && (forms->placeholder_parameters[placeholder_first + before] == parameter))
                            ? before
                            : first;
                }
                unsigned int k = 0u;
                for (unsigned int before = 0u; before < first; before += 1u)
                {
                    const unsigned int earlier = forms->placeholder_parameters[placeholder_first + before];
                    int seen = 0;
                    for (unsigned int again = 0u; again < before; again += 1u)
                    {
                        seen = seen || (forms->placeholder_parameters[placeholder_first + again] == earlier);
                    }
                    k += (RULESET_CORE_IS_SCRATCH(earlier) && (RULESET_CORE_SCRATCH_BANK(earlier) == bank) &&
                          (seen == 0))
                             ? 1u
                             : 0u;
                }
                argument.before = forms->bank_before[bank];
                argument.after = forms->bank_after[bank];
                argument.number = (bank_index < 3u) ? (item->scratch[bank_index] + k) : 0u;
                argument.numbered = 1u;
            }
            const unsigned int base = ASM_PRINTER_PLACEHOLDER_AT + (placeholder * ASM_PRINTER_PLACEHOLDER_BITS);
            asm_printer_bits(chunk_record, base, ASM_PRINTER_WORD_BITS, argument.before);
            asm_printer_bits(chunk_record, base + ASM_PRINTER_WORD_BITS, ASM_PRINTER_WORD_BITS, argument.after);
            asm_printer_bits(chunk_record, base + (2u * ASM_PRINTER_WORD_BITS), ASM_PRINTER_NUMBER_BITS,
                             argument.number);
            asm_printer_bits(chunk_record, base + (2u * ASM_PRINTER_WORD_BITS) + ASM_PRINTER_NUMBER_BITS, 1u,
                             argument.numbered);
            lanes += (unsigned long long)forms->word_lengths[argument.before] + forms->word_lengths[argument.after] +
                     ((argument.numbered != 0u) ? ASM_PRINTER_DIGITS : 0u);
        }
        record_lanes[record + chunk] = lanes;
    }
}

// a record's first lane arranged, below ASM_PRINTER_LANES_MAX as the caller bounds the lanes
CODEGEN_CORE void asm_printer_first(unsigned int *records, unsigned long long record, unsigned long long first)
{
    asm_printer_bits(&records[record * ASM_PRINTER_RECORD_LIMBS], ASM_PRINTER_FIRST_AT, ASM_PRINTER_FIRST_BITS,
                     (unsigned int)first);
}

// a lane's byte, at the output's offset in its record of the program's out_limbs, below its sign bit
CODEGEN_CORE unsigned int asm_printer_byte(const unsigned int *record, unsigned int out_limbs, unsigned int offset)
{
    const unsigned long long pair =
        (unsigned long long)record[offset / 32u] |
        ((((offset / 32u) + 1u) < out_limbs) ? ((unsigned long long)record[(offset / 32u) + 1u] << 32u) : 0ULL);
    return (unsigned int)((pair >> (offset % 32u)) & 0xFFULL);
}

// The assembly printer and its tables for any lane that the core decides in `rules` for `target` under `header`. The
// return is 1 where the call arranged it, else 0 with the reason on `error`. A ruleset that gives a form as a construct
// gives an error, because the code generator takes the scratch of a construct. So does a bank that writes its register
// with other than one number, and more words or characters than the tables hold
int asm_printer_ruleset_build(const Ruleset *rules, const TargetInfo *target, const char *header,
                              AsmPrinterRuleset *text_rules, VerbaLinea *error);

void asm_printer_ruleset_release(AsmPrinterRuleset *text_rules);

// `count` words of `from` copied into a cell of their own in `words`: 1, or 0 where the pool cannot meet the cell
int asm_printer_words_taken(AsmPrinterWords *words, const unsigned int *from, unsigned long long count);

// the cell of `words` given back, and the list empty
void asm_printer_words_ended(AsmPrinterWords *words);

// 1 where two lists hold the same words
int asm_printer_words_same(const AsmPrinterWords *left, const AsmPrinterWords *right);

// the rules as the records' layout reads them in host memory, their lists `text_rules`' own
AsmPrinterLists asm_printer_lists(const AsmPrinterRuleset *text_rules);

// the host oracle. The functions above arrange the items of the lane (CodeGenerator::decided) as records, from the
// `count` items at `items`. The assembly printer writes them on the host (cycle_record_run_host). The return is 1 where
// it ran, with its text on `text`, else 0 with the reason on `error_message`
int asm_printer_host(const AsmPrinterRuleset *text_rules, const MachineInstr *items, unsigned long long count,
                     PlatformText *text, VerbaLinea *error_message);

#endif
