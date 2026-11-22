// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// slot.c: the slot: whether the vendor read an illegal instruction, every register the slot's lines reach and write,
// the container's code, and the distinct readings a run takes once each
#include "msq_internal.h"
#include "../../../../../platform/platform.h"
#include "memoria_operor/memoria_linea.h"
#include "cellularum_laboro/cellularum_linea.h"

// 1 where the vendor's output reports it found an illegal instruction
int msq_illegal_found(const char *text, size_t length)
{
    static const char s_illegal_marker[] = "Illegal instruction found";
    const size_t marker_length = sizeof(s_illegal_marker) - 1u;
    for (size_t at = 0u; (at + marker_length) <= length; at += 1u)
    {
        if (mmgr_bytes_compare(text + at, s_illegal_marker, marker_length) == 0)
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
    for (size_t at = 0u; raw[at] != '\0'; at += 1u)
    {
        if ((raw[at] != '[') ||
            ((at > 0u) && ((raw[at - 1u] == ']') || (raw[at - 1u] == 'c') || (raw[at - 1u] == 'C'))))
        {
            continue;
        }
        size_t walk = at + 1u;
        while (msq_space(raw[walk]))
        {
            walk += 1u;
        }
        if ((raw[walk] != 'R') || (raw[walk + 1u] < '0') || (raw[walk + 1u] > '9'))
        {
            continue;
        }
        walk += 1u;
        unsigned long long number = 0ULL;
        while ((raw[walk] >= '0') && (raw[walk] <= '9'))
        {
            number = (number * 10ULL) + (unsigned long long)(raw[walk] - '0');
            walk += 1u;
        }
        msq_register_add(set, number);
        if (mmgr_text_starts(raw + walk, ".64", PLATFORM_TEXT_TO_TERMINATOR))
        {
            msq_register_add(set, number + 1ULL);
        }
    }
    return (mmgr_text_starts(raw, "EXIT", PLATFORM_TEXT_TO_TERMINATOR)) && !msq_word(raw[4]);
}

// every register the lines `first` to `end` of `listing` read an address from, to the first EXIT with no guard
void msq_addresses(const MsqListing *listing, size_t first, size_t end, MsqRegisters *set)
{
    mmgr_bytes_set(set, 0, sizeof(*set));
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
    mmgr_bytes_copy(retained, operation, copied);
    retained[copied] = '\0';
    return (mmgr_text_find(retained, ".128", PLATFORM_TEXT_TO_TERMINATOR) != NULL)
               ? 3u
               : (((mmgr_text_find(retained, ".WIDE", PLATFORM_TEXT_TO_TERMINATOR) != NULL) ||
                   (mmgr_text_find(retained, ".64", PLATFORM_TEXT_TO_TERMINATOR) != NULL))
                      ? 1u
                      : 0u);
}

// the length of the first word of `text`, its spaces before skipped into `*start`
size_t msq_first_word(const char *text, size_t *start)
{
    size_t at = 0u;
    while ((text[at] != '\0') && msq_space(text[at]))
    {
        at += 1u;
    }
    *start = at;
    size_t end = at;
    while ((text[end] != '\0') && !msq_space(text[end]))
    {
        end += 1u;
    }
    return end - at;
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
    for (size_t at = 0u; rest[at] != '\0'; at += 1u)
    {
        // the start of what follows the operation is a word's edge, as is any place after a character no word contains
        if ((rest[at] != 'R') || ((at > 0u) && msq_word(rest[at - 1u])) || (rest[at + 1u] < '0') ||
            (rest[at + 1u] > '9'))
        {
            continue;
        }
        size_t walk = at + 1u;
        unsigned long long number = 0ULL;
        while ((rest[walk] >= '0') && (rest[walk] <= '9'))
        {
            number = (number * 10ULL) + (unsigned long long)(rest[walk] - '0');
            walk += 1u;
        }
        // the width the operand names where `.64` or `.128` follows it and a word's edge after that, else the
        // operation's
        unsigned int width = widened;
        if ((mmgr_text_starts(rest + walk, ".128", PLATFORM_TEXT_TO_TERMINATOR)) && !msq_word(rest[walk + 4u]))
        {
            width = 3u;
        }
        else if ((mmgr_text_starts(rest + walk, ".64", PLATFORM_TEXT_TO_TERMINATOR)) && !msq_word(rest[walk + 3u]))
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
    mmgr_bytes_set(set, 0, sizeof(*set));
    size_t start = 0u;
    const size_t operation = msq_first_word(body, &start);
    const char *const rest = body + start + operation;
    size_t operand = 0u;
    if (msq_first_word(rest, &operand) == 0u)
    {
        return;
    }
    const char *const first = rest + operand;
    if ((first[0] != 'R') || (first[1] < '0') || (first[1] > '9'))
    {
        return;
    }
    size_t walk = 1u;
    unsigned long long number = 0ULL;
    while ((first[walk] >= '0') && (first[walk] <= '9'))
    {
        number = (number * 10ULL) + (unsigned long long)(first[walk] - '0');
        walk += 1u;
    }
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
    for (size_t at = 0u; operands[at] != '\0'; at += 1u)
    {
        if ((operands[at] == '[') &&
            ((at == 0u) || ((operands[at - 1u] != ']') && (operands[at - 1u] != 'c') && (operands[at - 1u] != 'C'))))
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
    if ((size < 0x40ULL) || (mmgr_bytes_compare(data,
                                                "\x7f"
                                                "ELF",
                                                4u) != 0))
    {
        return 0;
    }
    unsigned long long section_offset = 0ULL;
    mmgr_bytes_copy(&section_offset, data + 0x28u, 8u);
    unsigned short entry_size = 0u;
    unsigned short entries = 0u;
    mmgr_bytes_copy(&entry_size, data + 0x3Au, 2u);
    mmgr_bytes_copy(&entries, data + 0x3Cu, 2u);
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
        mmgr_bytes_copy(&flags, data + header + 0x08u, 8u);
        mmgr_bytes_copy(&offset, data + header + 0x18u, 8u);
        mmgr_bytes_copy(&length, data + header + 0x20u, 8u);
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
    mmgr_bytes_set(set, 0, sizeof(*set));
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
            ((item->size == 0ULL) ||
             (mmgr_bytes_compare(set->items[stored].bytes, item->bytes, (size_t)item->size) == 0)))
        {
            return stored;
        }
    }
}

void msq_distinct_free(MsqDistinct *set)
{
    platform_cell_released(set->items, platform_place_g_program_owner);
    platform_cell_released(set->slots, platform_place_g_program_owner);
    mmgr_bytes_set(set, 0, sizeof(*set));
}
