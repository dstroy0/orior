// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// container_write.c: one container written from another, with new code in it for one part or many, driven by a layout
// file
#include "container_write_internal.h"

#include <stdlib.h>

static void value_put(unsigned char *bytes, unsigned int width, unsigned long long value)
{
    for (unsigned int at = 0u; at < width; at += 1u)
    {
        bytes[at] = (unsigned char)((value >> (8u * at)) & 0xffull);
    }
}

static unsigned long long rounded(unsigned long long at, unsigned long long align)
{
    return ((align > 1ull) && ((at % align) != 0ull)) ? (at + (align - (at % align))) : at;
}

// The part's attributes written with the endings this code holds, into `written`; its size, or 0 where the section
// carries no ending attribute and nothing was written
static unsigned long long attributes_written(const Places *places, const unsigned char *bytes, unsigned long long size,
                                             const unsigned int *exits, unsigned int exit_count, unsigned char *written,
                                             unsigned long long room)
{
    unsigned long long value_size = 0ull;
    const unsigned char *const found =
        container_attribute_of(places, bytes, size, places->attribute_exits, &value_size);
    if (found == NULL)
    {
        // A section with no ending attribute belongs to something the target calls and returns from instead of to
        // something it enters: an entry ends where the container says it ends, and the other ends on its own.
        // Carrying the section over untouched is right for that and only for that
        if (exit_count != 0u)
        {
            return 0ull;
        }
        if (size > room)
        {
            printf("  container_write: the attributes take %llu bytes where the room is %llu\n", size, room);
            return 0ull;
        }
        memcpy(written, bytes, size);
        return size;
    }
    const unsigned long long before = (unsigned long long)(found - bytes);
    const unsigned long long after = before + places->attribute_header + value_size;
    const unsigned long long whole = before + places->attribute_header + (4ull * exit_count) + (size - after);
    if (whole > room)
    {
        printf("  container_write: the attributes take %llu bytes where the room is %llu\n", whole, room);
        return 0ull;
    }
    memcpy(written, bytes, before);
    written[before] = (unsigned char)places->attribute_format_value;
    written[before + 1u] = (unsigned char)places->attribute_exits;
    value_put(&written[before + 2u], 2u, 4ull * exit_count);
    for (unsigned int number = 0u; number < exit_count; number += 1u)
    {
        value_put(&written[before + places->attribute_header + (4ull * number)], 4u, exits[number]);
    }
    memcpy(&written[before + places->attribute_header + (4ull * exit_count)], &bytes[after], size - after);
    return whole;
}

// the register count in the whole container's attributes, set for the part's symbol
static void registers_set(const Places *places, unsigned char *bytes, unsigned long long size, unsigned int symbol,
                          unsigned int registers)
{
    unsigned long long value_size = 0ull;
    const unsigned char *const found =
        container_attribute_of(places, bytes, size, places->attribute_registers, &value_size);
    if ((found != NULL) && (value_size == 8ull) &&
        (container_value_read(&found[places->attribute_header], 4u) == (unsigned long long)symbol))
    {
        value_put(&bytes[(unsigned long long)(found - bytes) + places->attribute_header + 4ull], 4u, registers);
    }
}

// where one part of the many lies in the pattern, as the write finds it: its code section, its own attributes
// section and its symbol
typedef struct
{
    unsigned int code_index;
    unsigned int part_info;
    unsigned int symbol;
} PartPlace;

int container_parts_write(const ContainerParts *args, unsigned char *written, unsigned long long room,
                          unsigned long long *size)
{
    Places places;
    if (!container_places_read(args->layout, &places))
    {
        return 0;
    }
    const unsigned char *const pattern = args->pattern;
    if (!container_pattern_holds(&places, pattern, args->pattern_size))
    {
        printf("  container_write: a table, a section or a name of the pattern lies past its %llu bytes\n",
               args->pattern_size);
        return 0;
    }
    unsigned int sections = 0u;
    unsigned long long strings = 0ull;
    unsigned long long section_entry = 0ull;
    const unsigned long long section_table =
        container_sections_of(&places, pattern, &sections, &strings, &section_entry);
    const unsigned long long segment_table =
        container_value_read(&pattern[places.segment_table], places.segment_table_width);
    const unsigned long long segment_entry =
        container_value_read(&pattern[places.segment_entry], places.segment_entry_width);
    const unsigned int segments =
        (unsigned int)container_value_read(&pattern[places.segment_count], places.segment_count_width);
    // the sections as the write holds them, as many as CONTAINER_SECTIONS bounds, kept apart from the stack
    static Section s_kept[CONTAINER_SECTIONS];
    Section *const kept = s_kept;
    for (unsigned int index = 0u; index < sections; index += 1u)
    {
        const unsigned long long at = section_table + ((unsigned long long)index * section_entry);
        kept[index].was_at = at;
        kept[index].type = container_value_read(&pattern[at + places.section_type], places.section_type_width);
        kept[index].goes_at = container_value_read(&pattern[at + places.section_offset], places.section_offset_width);
        // a section that takes no bytes in the file is laid out at no length and keeps the size its header gives
        const int takes_bytes = container_section_takes_bytes(&places, pattern, at, index);
        kept[index].bytes = takes_bytes ? &pattern[kept[index].goes_at] : pattern;
        kept[index].size =
            takes_bytes ? container_value_read(&pattern[at + places.section_size], places.section_size_width) : 0ull;
        kept[index].align = container_value_read(&pattern[at + places.section_align], places.section_align_width);
    }
    char named[256];
    unsigned int all_info = sections;
    unsigned int symbols = sections;
    if (layout_text(args->layout, "section.program_info", NULL, named, sizeof(named)))
    {
        all_info = container_section_named(&places, pattern, kept, sections, strings, named);
    }
    if (layout_text(args->layout, "section.symbols", NULL, named, sizeof(named)))
    {
        symbols = container_section_named(&places, pattern, kept, sections, strings, named);
    }
    if ((all_info == sections) || (symbols == sections))
    {
        printf("  container_write: the pattern holds no program attributes or no symbol table\n");
        return 0;
    }
    // every part's place, and the room its rewritten attributes take: what they took, and four bytes an exit more
    PartPlace *const place = (PartPlace *)calloc((size_t)args->part_count + 1u, sizeof(PartPlace));
    if (place == NULL)
    {
        printf("  container_write: no memory for the places of %u parts\n", args->part_count);
        return 0;
    }
    unsigned long long attribute_room = 0ull;
    int found = 1;
    for (unsigned int part = 0u; found && (part < args->part_count); part += 1u)
    {
        const ContainerPart *const one = &args->parts[part];
        place[part].code_index = sections;
        place[part].part_info = sections;
        if (layout_text(args->layout, "section.code", one->part, named, sizeof(named)))
        {
            place[part].code_index = container_section_named(&places, pattern, kept, sections, strings, named);
        }
        if (layout_text(args->layout, "section.part_info", one->part, named, sizeof(named)))
        {
            place[part].part_info = container_section_named(&places, pattern, kept, sections, strings, named);
        }
        found = (place[part].code_index != sections) && (place[part].part_info != sections);
        if (!found)
        {
            printf("  container_write: the pattern holds no part named %s\n", one->part);
            break;
        }
        place[part].symbol =
            (unsigned int)(container_value_read(&pattern[kept[place[part].code_index].was_at + places.section_info],
                                                places.section_info_width) &
                           places.registers_symbol_mask);
        // the symbol's size is written inside the symbol table
        found = (((unsigned long long)place[part].symbol * places.symbol_bytes) + places.symbol_size +
                 places.symbol_size_width) <= kept[symbols].size;
        if (!found)
        {
            printf("  container_write: the symbol %u of %s lies past the %llu bytes of its symbol table\n",
                   place[part].symbol, one->part, kept[symbols].size);
            break;
        }
        attribute_room += kept[place[part].part_info].size + (4ull * one->exit_count);
    }
    // one block of memory for the program's attributes and every part's own, each part's after the last
    unsigned char *const block = found ? (unsigned char *)malloc((size_t)(kept[all_info].size + attribute_room + 1ull))
                                       : NULL;
    if (block == NULL)
    {
        printf("  container_write: no room was had for the attributes of %u parts\n", args->part_count);
        free(place);
        return 0;
    }
    unsigned char *const all = block;
    memcpy(all, kept[all_info].bytes, kept[all_info].size);
    kept[all_info].bytes = all;
    unsigned long long used = kept[all_info].size;
    for (unsigned int part = 0u; found && (part < args->part_count); part += 1u)
    {
        const ContainerPart *const one = &args->parts[part];
        const unsigned long long part_room = kept[place[part].part_info].size + (4ull * one->exit_count);
        const unsigned long long part_size =
            attributes_written(&places, kept[place[part].part_info].bytes, kept[place[part].part_info].size,
                               one->exits, one->exit_count, block + used, part_room);
        found = (part_size != 0ull);
        if (!found)
        {
            printf("  container_write: %s holds no endings to write\n", one->part);
            break;
        }
        if (one->registers != 0u)
        {
            registers_set(&places, all, kept[all_info].size, place[part].symbol, one->registers);
        }
        kept[place[part].code_index].bytes = one->code;
        kept[place[part].code_index].size = one->code_size;
        kept[place[part].part_info].bytes = block + used;
        kept[place[part].part_info].size = part_size;
        used += part_size;
    }
    if (!found)
    {
        free(block);
        free(place);
        return 0;
    }

    unsigned long long at = places.header_bytes;
    for (unsigned int index = 1u; index < sections; index += 1u)
    {
        kept[index].goes_at = rounded(at, kept[index].align);
        at = kept[index].goes_at + kept[index].size;
    }
    const unsigned long long new_sections = rounded(at, places.table_align);
    const unsigned long long new_segments = new_sections + ((unsigned long long)sections * section_entry);
    const unsigned long long whole = new_segments + ((unsigned long long)segments * segment_entry);
    if (whole > room)
    {
        printf("  container_write: the container takes %llu bytes where the room is %llu\n", whole, room);
        free(block);
        free(place);
        return 0;
    }
    memset(written, 0, whole);
    memcpy(written, pattern, places.header_bytes);
    value_put(&written[places.section_table], places.section_table_width, new_sections);
    value_put(&written[places.segment_table], places.segment_table_width, new_segments);
    for (unsigned int index = 0u; index < sections; index += 1u)
    {
        unsigned char *const header = &written[new_sections + ((unsigned long long)index * section_entry)];
        memcpy(header, &pattern[kept[index].was_at], section_entry);
        if (index != 0u)
        {
            memcpy(&written[kept[index].goes_at], kept[index].bytes, kept[index].size);
            value_put(&header[places.section_offset], places.section_offset_width, kept[index].goes_at);
            if (kept[index].type != places.section_type_nobits)
            {
                value_put(&header[places.section_size], places.section_size_width, kept[index].size);
            }
        }
    }
    unsigned char *const symbol_table = &written[kept[symbols].goes_at];
    for (unsigned int part = 0u; part < args->part_count; part += 1u)
    {
        const ContainerPart *const one = &args->parts[part];
        // a part given a register count has it in its code section's header; one given none keeps the pattern's
        if (one->registers != 0u)
        {
            unsigned char *const code_header =
                &written[new_sections + ((unsigned long long)place[part].code_index * section_entry)];
            value_put(&code_header[places.section_info], places.section_info_width,
                      ((unsigned long long)one->registers << places.registers_shift) | place[part].symbol);
        }
        value_put(&symbol_table[((unsigned long long)place[part].symbol * places.symbol_bytes) + places.symbol_size],
                  places.symbol_size_width, one->code_size);
    }
    for (unsigned int index = 0u; index < segments; index += 1u)
    {
        const unsigned long long was = segment_table + ((unsigned long long)index * segment_entry);
        unsigned char *const header = &written[new_segments + ((unsigned long long)index * segment_entry)];
        memcpy(header, &pattern[was], segment_entry);
        const unsigned long long was_at =
            container_value_read(&pattern[was + places.segment_offset], places.segment_offset_width);
        const unsigned long long was_size =
            container_value_read(&pattern[was + places.segment_file_size], places.segment_file_width);
        if (was_at == segment_table)
        {
            value_put(&header[places.segment_offset], places.segment_offset_width, new_segments);
            continue;
        }
        // a segment covers a run of sections, and covers the same run where they lie now
        unsigned long long first = whole;
        unsigned long long last = 0ull;
        for (unsigned int index_at = 1u; index_at < sections; index_at += 1u)
        {
            const unsigned long long was_section = container_value_read(
                &pattern[kept[index_at].was_at + places.section_offset], places.section_offset_width);
            if ((was_section >= was_at) && (was_section < (was_at + was_size)))
            {
                first = (kept[index_at].goes_at < first) ? kept[index_at].goes_at : first;
                last = ((kept[index_at].goes_at + kept[index_at].size) > last)
                           ? (kept[index_at].goes_at + kept[index_at].size)
                           : last;
            }
        }
        if (last > first)
        {
            value_put(&header[places.segment_offset], places.segment_offset_width, first);
            value_put(&header[places.segment_file_size], places.segment_file_width, last - first);
            value_put(&header[places.segment_memory_size], places.segment_memory_width, last - first);
        }
    }
    free(block);
    free(place);
    *size = whole;
    return 1;
}

int container_write(const ContainerWrite *args, unsigned char *written, unsigned long long room,
                    unsigned long long *size)
{
    const ContainerPart one = {args->part,      args->code,  args->code_size,
                               args->registers, args->exits, args->exit_count};
    const ContainerParts parts = {args->layout, args->pattern, args->pattern_size, &one, 1u};
    return container_parts_write(&parts, written, room, size);
}
