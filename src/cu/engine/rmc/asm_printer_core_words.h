// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// asm_printer_core_words.h: the build's types, words, parts and steps (asm_printer_core.h includes the parts in order)
#ifndef ASM_PRINTER_CORE_WORDS_H
#define ASM_PRINTER_CORE_WORDS_H

// the work of asm_printer_ruleset_build past the schema and the scratch, word for word that of the host
// (asm_printer_layout.cu and asm_printer_program.cu). It is every word and part that a lane can use, numbered in the
// order that the host numbers them. Then the lists that the layout of the records reads, and the steps, fields and
// tables of the program.
//
// The map of words of the host becomes a search of the words stored, which gives each word the same number. The device
// runs it in one thread (codegen_rules.cu), because the number of each word depends on every word before it. The
// encoding of keymath and the layout of the program by key_schedule follow it, as they do on the host. The memory
// belongs to the caller. Where a count passes its capacity, the build ends full, and the caller grows the capacity and
// runs it again

#include "asm_printer.h"

// the fields as the program reads them: the part, the first lane, then each placeholder's word before, word after,
// number and whether it has one
#define ASM_PRINTER_FIELD_PART 0u
#define ASM_PRINTER_FIELD_FIRST 1u
#define ASM_PRINTER_FIELD_PLACEHOLDER(placeholder_, which_) (2u + (4u * (placeholder_)) + (which_))

// the tables: the pieces of each part by `part * 8 + piece`, and the length and first character of each word by its
// number. Then the characters of every word end to end, ten to each power that a digit comes from, and a byte as itself
enum AsmPrinterTable
{
    ASM_PRINTER_TABLE_PIECE = 0,
    ASM_PRINTER_TABLE_LENGTH = 1,
    ASM_PRINTER_TABLE_START = 2,
    ASM_PRINTER_TABLE_CHARACTER = 3,
    ASM_PRINTER_TABLE_POWER = 4,
    ASM_PRINTER_TABLE_BYTE = 5,
    ASM_PRINTER_TABLES = ASM_PRINTER_PROGRAM_TABLES
};

// the rows a part takes in the pieces table, a power of two past its pieces
#define ASM_PRINTER_PART_ROWS 8u

// the widest word number, length and character offset the tables contain, and the widest index a table takes from a
// 16-bit field or entry
#define ASM_PRINTER_TABLE_BITS_MAX 16u

// the rows the power and byte tables take
#define ASM_PRINTER_POWER_ROWS 16u
#define ASM_PRINTER_BYTE_ROWS 256u

// the digits of the target's hash as the note writes it, sixteen hexadecimal digits
#define ASM_PRINTER_HASH_DIGITS 16u

// how the build ended:
//
// - every word, part, step and table arranged
// - a form given as a construct
// - a header that holds a byte 0
// - a bank that writes its register with other than one number
// - more words, characters or parts than the tables hold
// - a count past the capacity of the caller
enum AsmPrinterCoreEnd
{
    ASM_PRINTER_CORE_OK = 0,
    ASM_PRINTER_CORE_CONSTRUCT = 1,
    ASM_PRINTER_CORE_HEADER = 2,
    ASM_PRINTER_CORE_BANK = 3,
    ASM_PRINTER_CORE_TABLES = 4,
    ASM_PRINTER_CORE_FULL = 5
};

// a text of the ruleset: `length` characters from `first` of the ruleset's characters
struct AsmPrinterCoreText
{
    unsigned int first;
    unsigned int length;
};

// the ruleset as the build reads it:
//
// - the characters of every text end to end, the texts, and the placeholders
// - the pieces of each form, from `form_piece_first[form]` to `form_piece_first[form + 1]` of the texts
// - its placeholders, from `form_placeholder_first[form]` to `form_placeholder_first[form + 1]` of the placeholders,
//   and 1 where the ruleset gives it as a construct
// - the pieces of each bank, from `bank_piece_first[bank]`, and the count of its placeholders
// - the text of each fixed register, and that of the header
// - the hash of the target, its compute capability and the version of NVRTC, as the note writes them
struct AsmPrinterCoreRules
{
    const unsigned char *characters;
    const AsmPrinterCoreText *texts;
    const unsigned int *placeholders;
    unsigned int form_piece_first[OPCODE_COUNT + 1u];
    unsigned int form_placeholder_first[OPCODE_COUNT + 1u];
    unsigned int form_construct[OPCODE_COUNT];
    unsigned int bank_piece_first[REGCLASS_COUNT];
    unsigned int bank_placeholders[REGCLASS_COUNT];
    unsigned int fixed_text[PHYSREG_COUNT];
    unsigned int header_text;
    unsigned long long block_hash;
    unsigned int target_numbers[4];
};

// The build: the ruleset that it reads, the capacities of the memory of the caller, and what it arranges there:
//
// - the words, each once, by their numbers, with 0 the empty word. The first character and length of each word, and
//   its characters end to end.
// - the parts, each once, ASM_PRINTER_PIECES words each, and the lanes of each part
// - the lists as AsmPrinterRuleset stores them
// - the steps, fields, table values and tables of the program, and the step that it outputs
// - how it ended, and where
struct AsmPrinterCoreBuild
{
    AsmPrinterCoreRules rules;
    unsigned int word_capacity;
    unsigned int character_capacity;
    unsigned int part_capacity;
    unsigned int form_part_capacity;
    unsigned int placeholder_capacity;
    unsigned int step_capacity;
    unsigned int table_capacity[ASM_PRINTER_TABLES];
    unsigned int words;
    unsigned int characters;
    unsigned int *word_start;
    unsigned int *word_length;
    unsigned char *word_characters;
    unsigned int parts;
    unsigned int *part_pieces;
    unsigned int *part_lanes;
    unsigned int form_part_first[OPCODE_COUNT + 1u];
    unsigned int form_part_count;
    unsigned int *form_parts;
    unsigned int form_placeholder_first[OPCODE_COUNT + 1u];
    unsigned int placeholder_count;
    unsigned int *placeholder_parameters;
    unsigned int bank_before[REGCLASS_COUNT];
    unsigned int bank_after[REGCLASS_COUNT];
    unsigned int fixed_word[PHYSREG_COUNT];
    unsigned int minus_word;
    unsigned int hash_word;
    unsigned int header_part;
    unsigned int target_numbers[4];
    unsigned int step_count;
    EngineRecordStep *steps;
    unsigned int one;
    unsigned int two;
    unsigned int output;
    unsigned int field_bits[ASM_PRINTER_FIELDS];
    unsigned int field_offset[ASM_PRINTER_FIELDS];
    unsigned int *values[ASM_PRINTER_TABLES];
    EngineRecordTable tables[ASM_PRINTER_TABLES];
    unsigned int end;
    int full;
};

// the number of a word among the words, `length` characters at `characters`, added where they are new. The return is 0
// where the words or characters are past their capacities, with the build left full
CODEGEN_CORE unsigned int asm_printer_core_word(AsmPrinterCoreBuild *build, const unsigned char *characters,
                                                unsigned int length)
{
    for (unsigned int number = 0u; number < build->words; number += 1u)
    {
        if (build->word_length[number] != length)
        {
            continue;
        }
        const unsigned char *const stored_characters = &build->word_characters[build->word_start[number]];
        if ((length == 0u) ||
            (EMBED_CALL(memor.cmp, MemoriaCfg, .src = stored_characters, .other = characters, .bytes = length) == 0))
        {
            return number;
        }
    }
    if ((build->words >= build->word_capacity) || ((build->character_capacity - build->characters) < length))
    {
        build->full = 1;
        return 0u;
    }
    const unsigned int number = build->words;
    build->word_start[number] = build->characters;
    build->word_length[number] = length;
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &build->word_characters[build->characters], .src = characters,
               .bytes = length);
    build->characters += length;
    build->words += 1u;
    return number;
}

// the word of the ruleset's text `text`
CODEGEN_CORE unsigned int asm_printer_core_text(AsmPrinterCoreBuild *build, unsigned int text)
{
    const AsmPrinterCoreText *const given = &build->rules.texts[text];
    return asm_printer_core_word(build, &build->rules.characters[given->first], given->length);
}

// a part's number among the parts, taken on where it is new; 0 where the parts are past their capacity
CODEGEN_CORE unsigned int asm_printer_core_part(AsmPrinterCoreBuild *build, const unsigned int *part)
{
    for (unsigned int number = 0u; number < build->parts; number += 1u)
    {
        const unsigned int *const stored_pieces = &build->part_pieces[number * ASM_PRINTER_PIECES];
        if (EMBED_CALL(memor.cmp, MemoriaCfg, .src = stored_pieces, .other = part,
                       .bytes = ASM_PRINTER_PIECES * sizeof(unsigned int)) == 0)
        {
            return number;
        }
    }
    if (build->parts >= build->part_capacity)
    {
        build->full = 1;
        return 0u;
    }
    const unsigned int number = build->parts;
    for (unsigned int piece = 0u; piece < ASM_PRINTER_PIECES; piece += 1u)
    {
        build->part_pieces[(number * ASM_PRINTER_PIECES) + piece] = part[piece];
    }
    build->parts += 1u;
    return number;
}

// the least count of index bits that represents `count` rows, at least 1
CODEGEN_CORE unsigned int asm_printer_core_index_bits(unsigned long long count)
{
    unsigned int bits = 1u;
    while ((1ULL << bits) < count)
    {
        bits += 1u;
    }
    return bits;
}

// a step added, with its number as the return. Past the capacity, the call writes nothing and leaves the build full
CODEGEN_CORE unsigned int asm_printer_core_step(AsmPrinterCoreBuild *build, EngineRecordOperation operation,
                                                unsigned int left, unsigned int right)
{
    const unsigned int number = build->step_count;
    if (number >= build->step_capacity)
    {
        build->full = 1;
        return number;
    }
    build->steps[number] = {operation, left, right, 0u};
    build->step_count += 1u;
    return number;
}

CODEGEN_CORE unsigned int asm_printer_core_constant(AsmPrinterCoreBuild *build, unsigned int value)
{
    return asm_printer_core_step(build, ENGINE_RECORD_CONSTANT, value, 0u);
}

// 1 where left < right, else 0: the compare's order is -1, 0 or 1, and 1 less it, halved toward zero, is 1 only at -1
CODEGEN_CORE unsigned int asm_printer_core_below(AsmPrinterCoreBuild *build, unsigned int left, unsigned int right)
{
    const unsigned int order = asm_printer_core_step(build, ENGINE_RECORD_COMPARE, left, right);
    const unsigned int lessened = asm_printer_core_step(build, ENGINE_RECORD_DIFFERENCE, build->one, order);
    return asm_printer_core_step(build, ENGINE_RECORD_QUOTIENT, lessened, build->two);
}

#endif
