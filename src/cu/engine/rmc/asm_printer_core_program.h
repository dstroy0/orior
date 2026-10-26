// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// asm_printer_core_program.h: the program's steps, its tables and the ruleset build (asm_printer_core.h includes it)
#ifndef ASM_PRINTER_CORE_PROGRAM_H
#define ASM_PRINTER_CORE_PROGRAM_H

#include "asm_printer_core_words.h"

// the assembly printer's steps, a lane a byte; its one output is the byte, a table's entry of 8 bits
CODEGEN_CORE unsigned int asm_printer_core_program(AsmPrinterCoreBuild *build)
{
    build->one = asm_printer_core_constant(build, 1u);
    build->two = asm_printer_core_constant(build, 2u);
    const unsigned int ten = asm_printer_core_constant(build, 10u);
    const unsigned int zero_letter = asm_printer_core_constant(build, (unsigned int)'0');
    const unsigned int rows = asm_printer_core_constant(build, ASM_PRINTER_PART_ROWS);
    const unsigned int nothing = asm_printer_core_constant(build, 0u);
    // the record's fields
    const unsigned int part = asm_printer_core_step(build, ENGINE_RECORD_FIELD, ASM_PRINTER_FIELD_PART, 0u);
    const unsigned int first = asm_printer_core_step(build, ENGINE_RECORD_FIELD, ASM_PRINTER_FIELD_FIRST, 0u);
    unsigned int before[ASM_PRINTER_PLACEHOLDERS];
    unsigned int after[ASM_PRINTER_PLACEHOLDERS];
    unsigned int number[ASM_PRINTER_PLACEHOLDERS];
    unsigned int numbered[ASM_PRINTER_PLACEHOLDERS];
    for (unsigned int placeholder = 0u; placeholder < ASM_PRINTER_PLACEHOLDERS; placeholder += 1u)
    {
        before[placeholder] =
            asm_printer_core_step(build, ENGINE_RECORD_FIELD, ASM_PRINTER_FIELD_PLACEHOLDER(placeholder, 0u), 0u);
        after[placeholder] =
            asm_printer_core_step(build, ENGINE_RECORD_FIELD, ASM_PRINTER_FIELD_PLACEHOLDER(placeholder, 1u), 0u);
        number[placeholder] =
            asm_printer_core_step(build, ENGINE_RECORD_FIELD, ASM_PRINTER_FIELD_PLACEHOLDER(placeholder, 2u), 0u);
        numbered[placeholder] =
            asm_printer_core_step(build, ENGINE_RECORD_FIELD, ASM_PRINTER_FIELD_PLACEHOLDER(placeholder, 3u), 0u);
    }
    // the byte's place in its part, below 2^31 as the layout bounds the lanes, limited to one word
    const unsigned int lane = asm_printer_core_step(build, ENGINE_RECORD_LANE, 0u, 0u);
    const unsigned int apart = asm_printer_core_step(build, ENGINE_RECORD_DIFFERENCE, lane, first);
    const unsigned int place = asm_printer_core_step(build, ENGINE_RECORD_WRAP, apart, 32u);
    // the part's pieces
    const unsigned int part_row = asm_printer_core_step(build, ENGINE_RECORD_PRODUCT, part, rows);
    unsigned int piece[ASM_PRINTER_PIECES];
    for (unsigned int at = 0u; at < ASM_PRINTER_PIECES; at += 1u)
    {
        const unsigned int row = (at == 0u) ? part_row
                                            : asm_printer_core_step(build, ENGINE_RECORD_SUM, part_row,
                                                                    asm_printer_core_constant(build, at));
        piece[at] = asm_printer_core_step(build, ENGINE_RECORD_TABLE, row, ASM_PRINTER_TABLE_PIECE);
    }
    // each placeholder's number's digits: none where it has no number, else 1 and one more for each power of ten at or
    // below it
    unsigned int digits[ASM_PRINTER_PLACEHOLDERS];
    for (unsigned int placeholder = 0u; placeholder < ASM_PRINTER_PLACEHOLDERS; placeholder += 1u)
    {
        unsigned int counted = build->one;
        unsigned int power = 1u;
        for (unsigned int more = 1u; more < ASM_PRINTER_DIGITS; more += 1u)
        {
            power *= 10u;
            const unsigned int reached =
                asm_printer_core_below(build, asm_printer_core_constant(build, power - 1u), number[placeholder]);
            counted = asm_printer_core_step(build, ENGINE_RECORD_SUM, counted, reached);
        }
        digits[placeholder] = asm_printer_core_step(build, ENGINE_RECORD_PRODUCT, numbered[placeholder], counted);
    }
    // the atoms in order, each a word by its number or a placeholder's digits, and its length
    unsigned int word[ASM_PRINTER_ATOMS];
    unsigned int length[ASM_PRINTER_ATOMS];
    unsigned int placeholder_of[ASM_PRINTER_ATOMS];
    int digited[ASM_PRINTER_ATOMS];
    for (unsigned int atom = 0u; atom < ASM_PRINTER_ATOMS; atom += 1u)
    {
        const unsigned int placeholder = atom / 4u;
        const unsigned int within = atom % 4u;
        placeholder_of[atom] = placeholder;
        digited[atom] = within == 2u;
        word[atom] = (within == 0u)
                         ? piece[placeholder]
                         : ((within == 1u) ? before[placeholder] : ((within == 3u) ? after[placeholder] : 0u));
        length[atom] = (digited[atom] != 0)
                           ? digits[placeholder]
                           : asm_printer_core_step(build, ENGINE_RECORD_TABLE, word[atom], ASM_PRINTER_TABLE_LENGTH);
    }
    // where each atom begins in the part's text, and past the last
    unsigned int start[ASM_PRINTER_ATOMS + 1u];
    start[0] = nothing;
    for (unsigned int atom = 0u; atom < ASM_PRINTER_ATOMS; atom += 1u)
    {
        start[atom + 1u] = asm_printer_core_step(build, ENGINE_RECORD_SUM, start[atom], length[atom]);
    }
    unsigned int below[ASM_PRINTER_ATOMS + 1u];
    for (unsigned int atom = 0u; atom <= ASM_PRINTER_ATOMS; atom += 1u)
    {
        below[atom] = asm_printer_core_below(build, place, start[atom]);
    }
    // the byte: the letter of the one atom the place lies in, 0 where it lies in none
    unsigned int byte = nothing;
    for (unsigned int atom = 0u; atom < ASM_PRINTER_ATOMS; atom += 1u)
    {
        const unsigned int reached = asm_printer_core_step(build, ENGINE_RECORD_DIFFERENCE, build->one, below[atom]);
        const unsigned int inside = asm_printer_core_step(build, ENGINE_RECORD_PRODUCT, reached, below[atom + 1u]);
        const unsigned int offset = asm_printer_core_step(build, ENGINE_RECORD_DIFFERENCE, place, start[atom]);
        unsigned int letter = 0u;
        if (digited[atom] == 0)
        {
            const unsigned int begins =
                asm_printer_core_step(build, ENGINE_RECORD_TABLE, word[atom], ASM_PRINTER_TABLE_START);
            const unsigned int at = asm_printer_core_step(build, ENGINE_RECORD_SUM, begins, offset);
            letter = asm_printer_core_step(build, ENGINE_RECORD_TABLE, at, ASM_PRINTER_TABLE_LETTER);
        }
        else
        {
            // the digit at `offset` from the left is the number over ten to the digits left after it, modulo ten; a
            // place outside the atom reads some power, never 0, and its letter is not taken
            const unsigned int placeholder = placeholder_of[atom];
            const unsigned int last =
                asm_printer_core_step(build, ENGINE_RECORD_DIFFERENCE, digits[placeholder], build->one);
            const unsigned int exponent = asm_printer_core_step(build, ENGINE_RECORD_DIFFERENCE, last, offset);
            const unsigned int power =
                asm_printer_core_step(build, ENGINE_RECORD_TABLE, exponent, ASM_PRINTER_TABLE_POWER);
            const unsigned int shifted =
                asm_printer_core_step(build, ENGINE_RECORD_QUOTIENT, number[placeholder], power);
            const unsigned int digit = asm_printer_core_step(build, ENGINE_RECORD_REMAINDER, shifted, ten);
            letter = asm_printer_core_step(build, ENGINE_RECORD_SUM, digit, zero_letter);
        }
        const unsigned int taken = asm_printer_core_step(build, ENGINE_RECORD_PRODUCT, inside, letter);
        byte = asm_printer_core_step(build, ENGINE_RECORD_SUM, byte, taken);
    }
    return asm_printer_core_step(build, ENGINE_RECORD_TABLE, byte, ASM_PRINTER_TABLE_BYTE);
}

// the tables laid out from the words and the parts, each row a word, each table's rows a power of two: 1 where each
// fits its capacity, else 0 with the build left full
CODEGEN_CORE int asm_printer_core_tables(AsmPrinterCoreBuild *build)
{
    const unsigned int part_bits =
        asm_printer_core_index_bits((unsigned long long)build->parts * ASM_PRINTER_PART_ROWS);
    const unsigned int word_bits = asm_printer_core_index_bits(build->words);
    const unsigned int letter_bits = asm_printer_core_index_bits(build->letters);
    const unsigned int rows[ASM_PRINTER_TABLES] = {1u << part_bits,   1u << word_bits,        1u << word_bits,
                                                   1u << letter_bits, ASM_PRINTER_POWER_ROWS, ASM_PRINTER_BYTE_ROWS};
    for (unsigned int table = 0u; table < ASM_PRINTER_TABLES; table += 1u)
    {
        if (rows[table] > build->table_capacity[table])
        {
            build->full = 1;
            return 0;
        }
    }
    unsigned int *const piece_values = build->values[ASM_PRINTER_TABLE_PIECE];
    for (unsigned int row = 0u; row < rows[ASM_PRINTER_TABLE_PIECE]; row += 1u)
    {
        piece_values[row] = 0u;
    }
    for (unsigned int part = 0u; part < build->parts; part += 1u)
    {
        for (unsigned int piece = 0u; piece < ASM_PRINTER_PIECES; piece += 1u)
        {
            piece_values[(part * ASM_PRINTER_PART_ROWS) + piece] =
                build->part_pieces[(part * ASM_PRINTER_PIECES) + piece];
        }
    }
    unsigned int *const length_values = build->values[ASM_PRINTER_TABLE_LENGTH];
    unsigned int *const start_values = build->values[ASM_PRINTER_TABLE_START];
    unsigned int *const letter_values = build->values[ASM_PRINTER_TABLE_LETTER];
    for (unsigned int row = 0u; row < rows[ASM_PRINTER_TABLE_LENGTH]; row += 1u)
    {
        length_values[row] = (row < build->words) ? build->word_length[row] : 0u;
        start_values[row] = (row < build->words) ? build->word_start[row] : 0u;
    }
    // the words' letters lie end to end in number order, as the host laid them out
    for (unsigned int row = 0u; row < rows[ASM_PRINTER_TABLE_LETTER]; row += 1u)
    {
        letter_values[row] = (row < build->letters) ? (unsigned int)build->word_letters[row] : 0u;
    }
    // ten to each power a 32-bit word has a digit at, and 1 at the rest, where no lane divides by 0
    unsigned int *const power_values = build->values[ASM_PRINTER_TABLE_POWER];
    for (unsigned int row = 0u; row < ASM_PRINTER_POWER_ROWS; row += 1u)
    {
        power_values[row] = 1u;
    }
    unsigned int power = 1u;
    for (unsigned int exponent = 0u; exponent < ASM_PRINTER_DIGITS; exponent += 1u)
    {
        power_values[exponent] = power;
        power = (exponent < (ASM_PRINTER_DIGITS - 1u)) ? (10u * power) : power;
    }
    unsigned int *const byte_values = build->values[ASM_PRINTER_TABLE_BYTE];
    for (unsigned int byte = 0u; byte < ASM_PRINTER_BYTE_ROWS; byte += 1u)
    {
        byte_values[byte] = byte;
    }
    build->tables[ASM_PRINTER_TABLE_PIECE] = {part_bits, ASM_PRINTER_WORD_BITS, piece_values};
    build->tables[ASM_PRINTER_TABLE_LENGTH] = {word_bits, ASM_PRINTER_WORD_BITS, length_values};
    build->tables[ASM_PRINTER_TABLE_START] = {word_bits, ASM_PRINTER_WORD_BITS, start_values};
    build->tables[ASM_PRINTER_TABLE_LETTER] = {letter_bits, 8u, letter_values};
    // 10^9 is below 2^30
    build->tables[ASM_PRINTER_TABLE_POWER] = {4u, 30u, power_values};
    build->tables[ASM_PRINTER_TABLE_BYTE] = {8u, 8u, byte_values};
    return 1;
}

// the build ended as `end`; 0, which the build returns
CODEGEN_CORE int asm_printer_core_ended(AsmPrinterCoreBuild *build, unsigned int end)
{
    build->end = end;
    return 0;
}

// Every word and part a lane the core decides can be written in, laid out before any lane is: each form's parts from
// its pieces, its placeholders four to a part and one part for a form of none, each bank's words around its number,
// each stored register's word, the minus, the target's hash and the header, whole; then the program, its fields and its
// tables. 1 where it is laid out, else 0 and how it ended in `end`, ASM_PRINTER_CORE_FULL where a capacity was short
CODEGEN_CORE int asm_printer_core_ruleset_build(AsmPrinterCoreBuild *build)
{
    const AsmPrinterCoreRules *const rules = &build->rules;
    build->end = ASM_PRINTER_CORE_OK;
    build->full = 0;
    build->words = 0u;
    build->letters = 0u;
    build->parts = 0u;
    build->form_part_count = 0u;
    build->placeholder_count = 0u;
    build->step_count = 0u;
    const AsmPrinterCoreText *const header = &rules->texts[rules->header_text];
    for (unsigned int at = 0u; at < header->length; at += 1u)
    {
        if (rules->letters[header->first + at] == 0u)
        {
            return asm_printer_core_ended(build, ASM_PRINTER_CORE_HEADER);
        }
    }
    asm_printer_core_word(build, rules->letters, 0u);
    for (unsigned int form = 0u; form < OPCODE_COUNT; form += 1u)
    {
        if (rules->form_construct[form] != 0u)
        {
            return asm_printer_core_ended(build, ASM_PRINTER_CORE_CONSTRUCT);
        }
        const unsigned int pieces = rules->form_piece_first[form];
        const unsigned int placeholder_first = rules->form_placeholder_first[form];
        const unsigned int placeholders = rules->form_placeholder_first[form + 1u] - placeholder_first;
        // the parts and the placeholders' parameters, a few hundred of each
        build->form_part_first[form] = build->form_part_count;
        build->form_placeholder_first[form] = build->placeholder_count;
        if ((build->placeholder_capacity - build->placeholder_count) < placeholders)
        {
            build->full = 1;
            return asm_printer_core_ended(build, ASM_PRINTER_CORE_FULL);
        }
        for (unsigned int placeholder = 0u; placeholder < placeholders; placeholder += 1u)
        {
            build->placeholder_parameters[build->placeholder_count + placeholder] =
                rules->placeholders[placeholder_first + placeholder];
        }
        build->placeholder_count += placeholders;
        for (unsigned int first_placeholder = 0u; (first_placeholder == 0u) || (first_placeholder < placeholders);
             first_placeholder += ASM_PRINTER_PLACEHOLDERS)
        {
            const unsigned int left = placeholders - first_placeholder;
            const unsigned int taken = (left < ASM_PRINTER_PLACEHOLDERS) ? left : ASM_PRINTER_PLACEHOLDERS;
            const int last = (first_placeholder + taken) == placeholders;
            unsigned int part[ASM_PRINTER_PIECES] = {0u};
            for (unsigned int placeholder = 0u; placeholder < taken; placeholder += 1u)
            {
                part[placeholder] = asm_printer_core_text(build, pieces + first_placeholder + placeholder);
            }
            part[taken] = last ? asm_printer_core_text(build, pieces + placeholders) : 0u;
            const unsigned int numbered = asm_printer_core_part(build, part);
            if (build->form_part_count >= build->form_part_capacity)
            {
                build->full = 1;
                return asm_printer_core_ended(build, ASM_PRINTER_CORE_FULL);
            }
            build->form_parts[build->form_part_count] = numbered;
            build->form_part_count += 1u;
            if (last)
            {
                break;
            }
        }
    }
    build->form_part_first[OPCODE_COUNT] = build->form_part_count;
    build->form_placeholder_first[OPCODE_COUNT] = build->placeholder_count;
    for (unsigned int bank = 0u; bank < REGCLASS_COUNT; bank += 1u)
    {
        if (rules->bank_placeholders[bank] != 1u)
        {
            return asm_printer_core_ended(build, ASM_PRINTER_CORE_BANK);
        }
        build->bank_before[bank] = asm_printer_core_text(build, rules->bank_piece_first[bank]);
        build->bank_after[bank] = asm_printer_core_text(build, rules->bank_piece_first[bank] + 1u);
    }
    for (unsigned int fixed = 0u; fixed < PHYSREG_COUNT; fixed += 1u)
    {
        build->fixed_word[fixed] = asm_printer_core_text(build, rules->fixed_text[fixed]);
    }
    // the hash as "%016llx" writes it
    unsigned char block[ASM_PRINTER_HASH_DIGITS];
    for (unsigned int digit = 0u; digit < ASM_PRINTER_HASH_DIGITS; digit += 1u)
    {
        const unsigned int nibble =
            (unsigned int)((rules->block_hash >> (4u * (ASM_PRINTER_HASH_DIGITS - 1u - digit))) & 0xFull);
        block[digit] = (unsigned char)((nibble < 10u) ? ('0' + nibble) : ('a' + (nibble - 10u)));
    }
    const unsigned char minus = (unsigned char)'-';
    build->minus_word = asm_printer_core_word(build, &minus, 1u);
    build->hash_word = asm_printer_core_word(build, block, ASM_PRINTER_HASH_DIGITS);
    unsigned int header_part[ASM_PRINTER_PIECES] = {0u};
    header_part[0] = asm_printer_core_text(build, rules->header_text);
    build->header_part = asm_printer_core_part(build, header_part);
    for (unsigned int number = 0u; number < 4u; number += 1u)
    {
        build->target_numbers[number] = rules->target_numbers[number];
    }
    if (build->full != 0)
    {
        return asm_printer_core_ended(build, ASM_PRINTER_CORE_FULL);
    }
    // each part's lanes, the lengths of its pieces, below 2^16 as the program's layout bounds the letters
    for (unsigned int part = 0u; part < build->parts; part += 1u)
    {
        unsigned int lanes = 0u;
        for (unsigned int piece = 0u; piece < ASM_PRINTER_PIECES; piece += 1u)
        {
            lanes += build->word_length[build->part_pieces[(part * ASM_PRINTER_PIECES) + piece]];
        }
        build->part_lanes[part] = lanes;
    }
    if ((build->letters >= (1u << ASM_PRINTER_TABLE_BITS_MAX)) || (build->words > (1u << ASM_PRINTER_TABLE_BITS_MAX)) ||
        (((unsigned long long)build->parts * ASM_PRINTER_PART_ROWS) > (1ull << ASM_PRINTER_TABLE_BITS_MAX)))
    {
        return asm_printer_core_ended(build, ASM_PRINTER_CORE_TABLES);
    }
    if (asm_printer_core_tables(build) == 0)
    {
        return asm_printer_core_ended(build, ASM_PRINTER_CORE_FULL);
    }
    build->output = asm_printer_core_program(build);
    if (build->full != 0)
    {
        return asm_printer_core_ended(build, ASM_PRINTER_CORE_FULL);
    }
    build->field_bits[ASM_PRINTER_FIELD_PART] = ASM_PRINTER_PART_BITS;
    build->field_offset[ASM_PRINTER_FIELD_PART] = 0u;
    build->field_bits[ASM_PRINTER_FIELD_FIRST] = ASM_PRINTER_FIRST_BITS;
    build->field_offset[ASM_PRINTER_FIELD_FIRST] = ASM_PRINTER_FIRST_AT;
    for (unsigned int placeholder = 0u; placeholder < ASM_PRINTER_PLACEHOLDERS; placeholder += 1u)
    {
        const unsigned int base = ASM_PRINTER_PLACEHOLDER_AT + (placeholder * ASM_PRINTER_PLACEHOLDER_BITS);
        const unsigned int bits[4] = {ASM_PRINTER_WORD_BITS, ASM_PRINTER_WORD_BITS, ASM_PRINTER_NUMBER_BITS, 1u};
        const unsigned int at[4] = {base, base + ASM_PRINTER_WORD_BITS, base + (2u * ASM_PRINTER_WORD_BITS),
                                    base + (2u * ASM_PRINTER_WORD_BITS) + ASM_PRINTER_NUMBER_BITS};
        for (unsigned int which = 0u; which < 4u; which += 1u)
        {
            build->field_bits[ASM_PRINTER_FIELD_PLACEHOLDER(placeholder, which)] = bits[which];
            build->field_offset[ASM_PRINTER_FIELD_PLACEHOLDER(placeholder, which)] = at[which];
        }
    }
    return 1;
}

#endif
