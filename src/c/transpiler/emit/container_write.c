// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// container_write.c: one container written from another, with new code in it, driven by a layout file
#include "container_write_internal.h"

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
        // A section with no ending attribute belongs to something the target calls and returns from, not to
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

int container_write(const ContainerWrite *args, unsigned char *written, unsigned long long room,
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
    Section kept[CONTAINER_SECTIONS];
    for (unsigned int index = 0u; index < sections; index += 1u)
    {
        const unsigned long long at = section_table + ((unsigned long long)index * section_entry);
        kept[index].was_at = at;
        kept[index].size = container_value_read(&pattern[at + places.section_size], places.section_size_width);
        kept[index].goes_at = container_value_read(&pattern[at + places.section_offset], places.section_offset_width);
        kept[index].bytes = &pattern[kept[index].goes_at];
        kept[index].align = container_value_read(&pattern[at + places.section_align], places.section_align_width);
    }
    char named[256];
    unsigned int code_index = sections;
    unsigned int part_info = sections;
    unsigned int all_info = sections;
    unsigned int symbols = sections;
    if (layout_text(args->layout, "section.code", args->part, named, sizeof(named)))
    {
        code_index = container_section_named(&places, pattern, kept, sections, strings, named);
    }
    if (layout_text(args->layout, "section.part_info", args->part, named, sizeof(named)))
    {
        part_info = container_section_named(&places, pattern, kept, sections, strings, named);
    }
    if (layout_text(args->layout, "section.program_info", NULL, named, sizeof(named)))
    {
        all_info = container_section_named(&places, pattern, kept, sections, strings, named);
    }
    if (layout_text(args->layout, "section.symbols", NULL, named, sizeof(named)))
    {
        symbols = container_section_named(&places, pattern, kept, sections, strings, named);
    }
    if ((code_index == sections) || (part_info == sections) || (all_info == sections) || (symbols == sections))
    {
        printf("  container_write: the pattern holds no part named %s\n", args->part);
        return 0;
    }
    static unsigned char s_part[CONTAINER_ATTRIBUTE_ROOM];
    static unsigned char s_all[CONTAINER_ATTRIBUTE_ROOM];
    const unsigned long long part_size = attributes_written(&places, kept[part_info].bytes, kept[part_info].size,
                                                            args->exits, args->exit_count, s_part, sizeof(s_part));
    if (part_size == 0ull)
    {
        printf("  container_write: %s holds no endings to write\n", args->part);
        return 0;
    }
    if (kept[all_info].size > sizeof(s_all))
    {
        printf("  container_write: the container's attributes take %llu bytes where the room is %llu\n",
               kept[all_info].size, (unsigned long long)sizeof(s_all));
        return 0;
    }
    memcpy(s_all, kept[all_info].bytes, kept[all_info].size);
    const unsigned int symbol =
        (unsigned int)(container_value_read(&pattern[kept[code_index].was_at + places.section_info],
                                            places.section_info_width) &
                       places.registers_symbol_mask);
    // the symbol's size is written inside the symbol table
    if ((((unsigned long long)symbol * places.symbol_bytes) + places.symbol_size + places.symbol_size_width) >
        kept[symbols].size)
    {
        printf("  container_write: the part's symbol %u lies past the %llu bytes of its symbol table\n", symbol,
               kept[symbols].size);
        return 0;
    }
    registers_set(&places, s_all, kept[all_info].size, symbol, args->registers);
    kept[code_index].bytes = args->code;
    kept[code_index].size = args->code_size;
    kept[part_info].bytes = s_part;
    kept[part_info].size = part_size;
    kept[all_info].bytes = s_all;

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
            value_put(&header[places.section_size], places.section_size_width, kept[index].size);
        }
    }
    unsigned char *const code_header = &written[new_sections + ((unsigned long long)code_index * section_entry)];
    value_put(&code_header[places.section_info], places.section_info_width,
              ((unsigned long long)args->registers << places.registers_shift) | symbol);
    unsigned char *const symbol_table = &written[kept[symbols].goes_at];
    value_put(&symbol_table[((unsigned long long)symbol * places.symbol_bytes) + places.symbol_size],
              places.symbol_size_width, args->code_size);
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
    *size = whole;
    return 1;
}
