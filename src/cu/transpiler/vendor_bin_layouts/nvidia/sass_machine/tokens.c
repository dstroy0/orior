// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tokens.c: the tokens of an instruction's text: spaces, a token taken, digits, the high half, an operand's kind and the operand taken, an instruction read, and two forms the same
#include "sass_machine_internal.h"

static int sass_is_space(char letter)
{
    return (letter == ' ') || (letter == '\t');
}

// `length` letters of `text` copied into `token`, with the spaces at either end cut and the whole kept below
// SASS_MACHINE_TOKEN letters
static void sass_token_take(char *token, const char *text, size_t length)
{
    while ((length != 0u) && sass_is_space(*text))
    {
        text += 1;
        length -= 1u;
    }
    while ((length != 0u) && sass_is_space(text[length - 1u]))
    {
        length -= 1u;
    }
    const size_t kept = (length < (SASS_MACHINE_TOKEN - 1u)) ? length : (SASS_MACHINE_TOKEN - 1u);
    memcpy(token, text, kept);
    token[kept] = '\0';
}

// 1 where every letter of `text` from `at` is a digit, and there is one
static int sass_all_digits(const char *text, size_t at)
{
    const size_t first = at;
    while ((text[at] >= '0') && (text[at] <= '9'))
    {
        at += 1u;
    }
    return (at != first) && (text[at] == '\0');
}

// 1 where `text` ends in the .hi a ruleset writes for the second register of a 64-bit pair. A ruleset names a
// register by its number alone, and the pair beginning at R14 has no other way to name R15 (sass.krs)
int sass_high_half(const char *text)
{
    const size_t length = strlen(text);
    return (length > 3u) && (strcmp(text + length - 3u, ".hi") == 0);
}

// what kind of thing `text` is, with any mark already cut off it
static unsigned int sass_operand_kind(const char *text)
{
    if (strchr(text, ' ') != NULL)
    {
        return SASS_OPERAND_UNKNOWN;
    }
    // the second register of a pair is a register, and is classified by the one it is named from
    if (sass_high_half(text))
    {
        char stem[SASS_MACHINE_TOKEN];
        const size_t length = strlen(text) - 3u;
        const size_t kept = (length < (SASS_MACHINE_TOKEN - 1u)) ? length : (SASS_MACHINE_TOKEN - 1u);
        memcpy(stem, text, kept);
        stem[kept] = '\0';
        return sass_operand_kind(stem);
    }
    if ((text[0] == 'R') && ((strcmp(text, "RZ") == 0) || sass_all_digits(text, 1u)))
    {
        return SASS_OPERAND_REGISTER;
    }
    if ((text[0] == 'P') && ((strcmp(text, "PT") == 0) || sass_all_digits(text, 1u)))
    {
        return SASS_OPERAND_PREDICATE;
    }
    // URZ is to the uniform registers what RZ is to the numbered ones, and reads the same way
    if ((text[0] == 'U') && (text[1] == 'R') && ((strcmp(text, "URZ") == 0) || sass_all_digits(text, 2u)))
    {
        return SASS_OPERAND_UNIFORM;
    }
    if (strncmp(text, "SR_", 3u) == 0)
    {
        return SASS_OPERAND_SYSTEM;
    }
    if ((text[0] == 'c') && (text[1] == '['))
    {
        return SASS_OPERAND_CONSTANT;
    }
    // an address, and an address that names the uniform register its memory descriptor is read from: desc[URn]
    // where the instruction's bit 101 is set, as the disassembler prints it, and term[URn] where it is clear and the
    // field still holds the register
    if ((text[0] == '[') || (strncmp(text, "desc[", 5u) == 0) || (strncmp(text, "term[", 5u) == 0))
    {
        return SASS_OPERAND_ADDRESS;
    }
    if (text[0] == '`')
    {
        return SASS_OPERAND_LABEL;
    }
    if ((text[0] == '0') && (text[1] == 'x'))
    {
        return SASS_OPERAND_IMMEDIATE;
    }
    // a number, with its sign where it carries one. A listing prints these in hex and a ruleset writes them in
    // decimal, and a negative decimal is a number the same as a negative hex: reading the digits from index 0 would
    // put the sign among them and leave -1 as nothing the reader knows
    if (((text[0] == '-') && (text[1] == '0') && (text[2] == 'x')) || sass_all_digits(text, 0u) ||
        ((text[0] == '-') && sass_all_digits(text, 1u)))
    {
        return SASS_OPERAND_IMMEDIATE;
    }
    return SASS_OPERAND_UNKNOWN;
}

// the operand at `place` of `parts` taken from `text`: its mark cut off the front, .reuse off the end, and its kind
// read. A minus before a number is the number's sign and no mark
static void sass_operand_take(SassInstructionParts *parts, unsigned int place, const char *text, size_t length)
{
    char token[SASS_MACHINE_TOKEN];
    sass_token_take(token, text, length);
    const size_t token_length = strlen(token);
    if ((token_length > 6u) && (strcmp(token + token_length - 6u, ".reuse") == 0))
    {
        token[token_length - 6u] = '\0';
    }
    unsigned int mark = (token[0] == '-')   ? SASS_MARK_NEGATE
                        : (token[0] == '~') ? SASS_MARK_INVERT
                        : (token[0] == '!') ? SASS_MARK_NOT
                                            : SASS_MARK_NONE;
    const unsigned int marked = sass_operand_kind(token + ((mark == SASS_MARK_NONE) ? 0u : 1u));
    // a minus before a number is the number's own sign, and the operand keeps it
    mark = ((mark == SASS_MARK_NEGATE) && (marked == SASS_OPERAND_IMMEDIATE)) ? SASS_MARK_NONE : mark;
    parts->mark[place] = mark;
    parts->kind[place] = (mark == SASS_MARK_NONE) ? sass_operand_kind(token) : marked;
    memcpy(parts->operand[place], token + ((mark == SASS_MARK_NONE) ? 0u : 1u),
           strlen(token + ((mark == SASS_MARK_NONE) ? 0u : 1u)) + 1u);
}

void sass_instruction_read(const char *text, SassInstructionParts *parts)
{
    memset(parts, 0, sizeof(*parts));
    while (sass_is_space(*text))
    {
        text += 1;
    }
    if (*text == '@')
    {
        const size_t length = strcspn(text, SASS_SPACES);
        sass_token_take(parts->guard, text, length);
        text += length;
        while (sass_is_space(*text))
        {
            text += 1;
        }
    }
    const size_t operation_length = strcspn(text, SASS_SPACES);
    sass_token_take(parts->operation, text, operation_length);
    text += operation_length;
    // the operands, split at each comma outside brackets
    int depth = 0;
    const char *start = text;
    for (const char *walk = text;; walk += 1)
    {
        const char letter = *walk;
        depth += ((letter == '[') || (letter == '(')) ? 1 : 0;
        depth -= ((letter == ']') || (letter == ')')) ? 1 : 0;
        if ((letter == '\0') || ((letter == ',') && (depth == 0)))
        {
            const size_t length = (size_t)(walk - start);
            if ((parts->operands < SASS_MACHINE_OPERANDS) && (strspn(start, SASS_SPACES) < length))
            {
                sass_operand_take(parts, parts->operands, start, length);
                parts->operands += 1u;
            }
            start = walk + 1;
        }
        if (letter == '\0')
        {
            break;
        }
    }
}

// 1 where the two hold the same operation and the same kind and mark at every operand, and the same text at every
// operand the assembler cannot turn into a number: a system register is named, not counted, and two instructions that
// name different ones are two forms, each with the encoding it was seen with
int sass_form_same(const SassForm *form, const SassInstructionParts *parts)
{
    int same = (strcmp(form->operation, parts->operation) == 0) && (form->operands == parts->operands);
    int by_text = 0;
    for (unsigned int place = 0u; same && (place < form->operands); place += 1u)
    {
        same = (form->kind[place] == parts->kind[place]) && (form->mark[place] == parts->mark[place]);
        by_text = by_text || (form->kind[place] == SASS_OPERAND_SYSTEM) ||
                  (form->kind[place] == SASS_OPERAND_UNKNOWN);
    }
    if (same && by_text)
    {
        SassInstructionParts seen;
        sass_instruction_read(form->text, &seen);
        for (unsigned int place = 0u; same && (place < form->operands); place += 1u)
        {
            same = ((form->kind[place] != SASS_OPERAND_SYSTEM) && (form->kind[place] != SASS_OPERAND_UNKNOWN)) ||
                   (strcmp(seen.operand[place], parts->operand[place]) == 0);
        }
    }
    return same;
}
