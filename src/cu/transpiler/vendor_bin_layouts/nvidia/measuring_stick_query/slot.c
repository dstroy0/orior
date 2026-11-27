// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// slot.c: the slot: whether the vendor read an illegal instruction, every register the slot's lines reach and write,
// the container's code, and the distinct readings a run takes once each
#include "../../../../../platform/platform.h"
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "msq_internal.h"

// 1 where the vendor's output reports it found an illegal instruction
int msq_illegal_found(const char *text, size_t length)
{
    static const char s_illegal_marker[] = "Illegal instruction found";
    const size_t marker_length = sizeof(s_illegal_marker) - 1u;
    for (size_t at = 0u; (at + marker_length) <= length; at += 1u)
    {
        if (EMBED_CALL(memor.cmp, MemoriaCfg, .src = text + at, .other = s_illegal_marker, .bytes = marker_length) == 0)
        {
            return 1;
        }
    }
    return 0;
}

static void msq_register_add(MsqRegisters *set, unsigned long long number)
{
    if (number < MSQ_REGISTERS)
    {
        set->bits[number / 64u] |= 1ULL << (number % 64u);
    }
}

// The general registers the line `raw` reads an address from, added to `set`: each `[` that opens no constant bank, one
// not after `]`, `c` or `C`, then space, then `R<number>`, and the register after it where it reads `.64`. 1 where the
// line is an EXIT with no guard, the end of what is read
static int msq_addresses_added(const char *raw, MsqRegisters *set)
{
    // each open bracket, reached from the one before it
    for (size_t at =
             EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = raw, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "[");
         raw[at] != '\0'; at += 1u + EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &raw[at + 1u],
                                                .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "["))
    {
        if ((at > 0u) && ((raw[at - 1u] == ']') || (raw[at - 1u] == 'c') || (raw[at - 1u] == 'C')))
        {
            continue;
        }
        const size_t walk = at + 1u +
                            EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &raw[at + 1u],
                                       .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = MSQ_SPACES);
        // Explicit cast reads the byte as its code point
        if ((raw[walk] != 'R') ||
            !EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_NUM, .byte = (uint8_t)raw[walk + 1u]))
        {
            continue;
        }
        const char *after = NULL;
        const unsigned long long number =
            EMBED_CALL(cellul.to_ulong, TransfiguroCfg, .src = &raw[walk + 1u], .end = &after);
        msq_register_add(set, number);
        if (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = after, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = ".64",
                       .ci = EMBED_FALSE))
        {
            msq_register_add(set, number + 1ULL);
        }
    }
    return (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = raw, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "EXIT",
                       .ci = EMBED_FALSE)) &&
           !msq_word(raw[4]);
}

// every register the lines `first` to `end` of `listing` read an address from, to the first EXIT with no guard
void msq_addresses(const MsqListing *listing, size_t first, size_t end, MsqRegisters *set)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = set, .bytes = sizeof(*set), .val = 0);
    for (size_t at = first; at < end; at += 1u)
    {
        if (msq_addresses_added(listing->lines[at].raw, set))
        {
            break;
        }
    }
}

// the widest a register the operation `operation` names reaches past itself: 3 for .128, 1 for .WIDE or .64, else 0
static unsigned int msq_widened(const char *operation, size_t length)
{
    char retained[96];
    const size_t copied = (length < (sizeof(retained) - 1u)) ? length : (sizeof(retained) - 1u);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = retained, .src = operation, .bytes = copied);
    retained[copied] = '\0';
    return (EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = retained, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = ".128", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE) != NULL)
               ? 3u
               : (((EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = retained, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                               .other = ".WIDE", .other_cap = PLATFORM_TEXT_TO_TERMINATOR,
                               .ci = EMBED_FALSE) != NULL) ||
                   (EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = retained, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                               .other = ".64", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE) != NULL))
                      ? 1u
                      : 0u);
}

// the length of the first word of `text`, its spaces before skipped into `*start`
size_t msq_first_word(const char *text, size_t *start)
{
    *start =
        EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = MSQ_SPACES);
    return EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &text[*start], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                      .other = MSQ_SPACES);
}

// The highest register the instruction `body` reaches at or past `registers`, the count its kernel allots: a pair
// reaches the register after the one named and a quad the third after it, as does every register of an operation read
// .WIDE, .64 or .128. Each `R<number>` with a word's edge before it and after it, past the operation, is a register. 1
// with
// `*reached` set, or 0 where every register it reaches lies below the count, or no count is given
int msq_register_reached(const char *body, unsigned int registers, unsigned long long *reached)
{
    if (registers == 0u)
    {
        return 0;
    }
    size_t start = 0u;
    const size_t operation = msq_first_word(body, &start);
    const unsigned int widened = msq_widened(body + start, operation);
    const char *const rest = body + start + operation;
    int found = 0;
    // each R, reached from the one before it
    for (size_t at = EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = rest, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                .other = "R");
         rest[at] != '\0'; at += 1u + EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &rest[at + 1u],
                                                 .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "R"))
    {
        // the start of what follows the operation is a word's edge, as is any place after a character no word contains.
        // Explicit cast reads the byte as its code point
        if (((at > 0u) && msq_word(rest[at - 1u])) ||
            !EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_NUM, .byte = (uint8_t)rest[at + 1u]))
        {
            continue;
        }
        const char *after = NULL;
        const unsigned long long number =
            EMBED_CALL(cellul.to_ulong, TransfiguroCfg, .src = &rest[at + 1u], .end = &after);
        // Explicit cast reads the place past the number as an offset into the operands
        const size_t walk = (size_t)(after - rest);
        // the width the operand names where `.64` or `.128` follows it and a word's edge after that, else the
        // operation's
        unsigned int width = widened;
        if ((EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = rest + walk, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = ".128", .ci = EMBED_FALSE)) &&
            !msq_word(rest[walk + 4u]))
        {
            width = 3u;
        }
        else if ((EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = rest + walk, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = ".64", .ci = EMBED_FALSE)) &&
                 !msq_word(rest[walk + 3u]))
        {
            width = 1u;
        }
        else if (msq_word(rest[walk]))
        {
            continue;
        }
        const unsigned long long reaching = number + width;
        if ((reaching >= registers) && (!found || (reaching > *reached)))
        {
            *reached = reaching;
            found = 1;
        }
    }
    return found;
}

// the general registers the instruction `body` writes: its first operand where that is one, `R<number>` and a word's
// edge, and the rest of a pair or a quad where the operation reads .WIDE, .64 or .128
void msq_registers_written(const char *body, MsqRegisters *set)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = set, .bytes = sizeof(*set), .val = 0);
    size_t start = 0u;
    const size_t operation = msq_first_word(body, &start);
    const char *const rest = body + start + operation;
    size_t operand = 0u;
    if (msq_first_word(rest, &operand) == 0u)
    {
        return;
    }
    const char *const first = rest + operand;
    // Explicit cast reads the byte as its code point
    if ((first[0] != 'R') || !EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_NUM, .byte = (uint8_t)first[1]))
    {
        return;
    }
    const char *after = NULL;
    const unsigned long long number = EMBED_CALL(cellul.to_ulong, TransfiguroCfg, .src = &first[1], .end = &after);
    // Explicit cast reads the place past the number as an offset into the operand
    const size_t walk = (size_t)(after - first);
    if (msq_word(first[walk]))
    {
        return;
    }
    const unsigned int width = msq_widened(body + start, operation);
    for (unsigned int place = 0u; place <= width; place += 1u)
    {
        msq_register_add(set, number + place);
    }
}

// 1 where the operands of `body`, the text past its first word, contain a memory operand: a `[` not after `]`, `c` or
// `C`
int msq_memory(const char *body)
{
    size_t start = 0u;
    const size_t operation = msq_first_word(body, &start);
    const char *const operands = body + start + operation;
    // each open bracket, reached from the one before it
    for (size_t at = EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = operands, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                .other = "[");
         operands[at] != '\0'; at += 1u + EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &operands[at + 1u],
                                                     .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "["))
    {
        if ((at == 0u) || ((operands[at - 1u] != ']') && (operands[at - 1u] != 'c') && (operands[at - 1u] != 'C')))
        {
            return 1;
        }
    }
    return 0;
}

// the first executable section of the container `bytes`, its code: 1, or 0 where it is no ELF or contains none
int msq_container_code(const MsqBytes *container, MsqBytes *code)
{
    const unsigned char *const data = container->bytes;
    const unsigned long long size = container->size;
    if ((size < 0x40ULL) || (EMBED_CALL(memor.cmp, MemoriaCfg, .src = data,
                                        .other = "\x7f"
                                                 "ELF",
                                        .bytes = 4u) != 0))
    {
        return 0;
    }
    unsigned long long section_offset = 0ULL;
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &section_offset, .src = data + 0x28u, .bytes = 8u);
    unsigned short entry_size = 0u;
    unsigned short entries = 0u;
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &entry_size, .src = data + 0x3Au, .bytes = 2u);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &entries, .src = data + 0x3Cu, .bytes = 2u);
    for (unsigned int index = 0u; index < entries; index += 1u)
    {
        const unsigned long long header = section_offset + ((unsigned long long)index * entry_size);
        if ((header + 0x28ULL) > size)
        {
            return 0;
        }
        unsigned long long flags = 0ULL;
        unsigned long long offset = 0ULL;
        unsigned long long length = 0ULL;
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &flags, .src = data + header + 0x08u, .bytes = 8u);
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &offset, .src = data + header + 0x18u, .bytes = 8u);
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &length, .src = data + header + 0x20u, .bytes = 8u);
        if (((flags & MSQ_EXECUTABLE) != 0ULL) && (length != 0ULL))
        {
            // the section as far as the container contains it
            code->bytes = data + ((offset < size) ? offset : size);
            code->size = (offset < size) ? (((offset + length) <= size) ? length : (size - offset)) : 0ULL;
            return 1;
        }
    }
    return 0;
}

static unsigned long long msq_bytes_hash(const MsqBytes *item)
{
    unsigned long long hash = 0xcbf29ce484222325ULL;
    for (unsigned long long at = 0ULL; at < item->size; at += 1ULL)
    {
        hash = (hash ^ item->bytes[at]) * 0x100000001b3ULL;
    }
    return hash;
}

// the set made ready for as many as `max` distinct runs: 1, or 0 where the system gives no memory
int msq_distinct_made(MsqDistinct *set, size_t max)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = set, .bytes = sizeof(*set), .val = 0);
    size_t slots = 64u;
    while (slots < (2u * max))
    {
        slots *= 2u;
    }
    set->slots = (size_t *)platform_cell_taken(slots * sizeof(size_t), platform_place_g_program_owner, 0);
    set->items = (MsqBytes *)platform_cell_taken((max + 1u) * sizeof(MsqBytes), platform_place_g_program_owner, 0);
    if ((set->slots == NULL) || (set->items == NULL))
    {
        return 0;
    }
    for (size_t at = 0u; at < slots; at += 1u)
    {
        set->slots[at] = (size_t)-1;
    }
    set->slot_count = slots;
    set->room = max + 1u;
    return 1;
}

// the index of `item` in the set, added where it is not there
size_t msq_distinct_index(MsqDistinct *set, const MsqBytes *item)
{
    const size_t mask = set->slot_count - 1u;
    for (size_t probe = (size_t)msq_bytes_hash(item) & mask;; probe = (probe + 1u) & mask)
    {
        const size_t stored = set->slots[probe];
        if (stored == (size_t)-1)
        {
            set->slots[probe] = set->count;
            set->items[set->count] = *item;
            set->count += 1u;
            return set->count - 1u;
        }
        if ((set->items[stored].size == item->size) &&
            ((item->size == 0ULL) || (EMBED_CALL(memor.cmp, MemoriaCfg, .src = set->items[stored].bytes,
                                                 .other = item->bytes, .bytes = (size_t)item->size) == 0)))
        {
            return stored;
        }
    }
}

void msq_distinct_free(MsqDistinct *set)
{
    platform_cell_released(set->items, platform_place_g_program_owner);
    platform_cell_released(set->slots, platform_place_g_program_owner);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = set, .bytes = sizeof(*set), .val = 0);
}
