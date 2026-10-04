// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// container_pattern.c: a pattern's tables, sections and attributes read where the layout says, each place held inside
// the pattern
#include "container_write_internal.h"

unsigned long long container_value_read(const unsigned char *bytes, unsigned int width)
{
    unsigned long long held = 0ull;
    for (unsigned int at = 0u; at < width; at += 1u)
    {
        held |= ((unsigned long long)bytes[at]) << (8u * at);
    }
    return held;
}

// every place the emitter uses, taken out of `layout`. 1, or 0 with the missing row named by the layout itself
int container_places_read(const ContainerLayout *layout, Places *places)
{
    memset(places, 0, sizeof(*places));
    const int held =
        layout_field(layout, "header.segment_table", &places->segment_table, &places->segment_table_width) &&
        layout_field(layout, "header.section_table", &places->section_table, &places->section_table_width) &&
        layout_field(layout, "header.segment_entry", &places->segment_entry, &places->segment_entry_width) &&
        layout_field(layout, "header.segment_count", &places->segment_count, &places->segment_count_width) &&
        layout_field(layout, "header.section_entry", &places->section_entry, &places->section_entry_width) &&
        layout_field(layout, "header.section_count", &places->section_count, &places->section_count_width) &&
        layout_field(layout, "header.strings_index", &places->strings_index, &places->strings_index_width) &&
        layout_field(layout, "section.name", &places->section_name, &places->section_name_width) &&
        layout_field(layout, "section.offset", &places->section_offset, &places->section_offset_width) &&
        layout_field(layout, "section.size", &places->section_size, &places->section_size_width) &&
        layout_field(layout, "section.info", &places->section_info, &places->section_info_width) &&
        layout_field(layout, "section.align", &places->section_align, &places->section_align_width) &&
        layout_field(layout, "segment.offset", &places->segment_offset, &places->segment_offset_width) &&
        layout_field(layout, "segment.file_size", &places->segment_file_size, &places->segment_file_width) &&
        layout_field(layout, "segment.memory_size", &places->segment_memory_size, &places->segment_memory_width) &&
        layout_field(layout, "symbol.size", &places->symbol_size, &places->symbol_size_width);
    if (!held)
    {
        return 0;
    }
    places->header_bytes = layout_number(layout, "header", 0ull);
    places->symbol_bytes = layout_number(layout, "symbol", 0ull);
    places->instruction = layout_number(layout, "instruction", 0ull);
    places->table_align = layout_number(layout, "table.align", 1ull);
    places->table_entry = layout_number(layout, "table.entry", 8ull);
    places->registers_shift = layout_number(layout, "registers.shift", 0ull);
    places->registers_symbol_mask = layout_number(layout, "registers.symbol_mask", 0ull);
    places->attribute_header = layout_number(layout, "attribute.header", 0ull);
    places->attribute_format_value = layout_number(layout, "attribute.format_value", 0ull);
    places->attribute_registers = layout_number(layout, "attribute.registers", 0ull);
    places->attribute_exits = layout_number(layout, "attribute.exits", 0ull);
    if ((places->header_bytes == 0ull) || (places->symbol_bytes == 0ull) || (places->attribute_header == 0ull) ||
        (places->instruction == 0ull))
    {
        printf("  container_write: %s names no header, symbol, attribute or instruction size\n", layout->from);
        return 0;
    }
    return 1;
}

unsigned int container_endings_find(const ContainerLayout *layout, const unsigned char *code,
                                    unsigned long long code_size, unsigned long long ending, unsigned int *exits,
                                    unsigned int room)
{
    const unsigned long long instruction = layout_number(layout, "instruction", 0ull);
    const unsigned long long key = layout_number(layout, "operation.mask", 0xfffull);
    unsigned int found = 0u;
    if (instruction == 0ull)
    {
        return 0u;
    }
    for (unsigned long long at = 0ull; (at + instruction) <= code_size; at += instruction)
    {
        if (((container_value_read(&code[at], 8u) & key) == (ending & key)) && (found < room))
        {
            exits[found] = (unsigned int)at;
            found += 1u;
        }
    }
    return found;
}

// 1 where a place of `width` bytes at `at` lies inside an entry of `entry` bytes, and its value fits a 64-bit word
static int place_inside(unsigned long long at, unsigned int width, unsigned long long entry)
{
    return (width <= 8u) && (at <= entry) && ((unsigned long long)width <= (entry - at));
}

// 1 where every place the emitter reads in `pattern`, which is `size` bytes, lies inside it: every field inside its
// header, entry or symbol, both tables, the bytes of every section, and every section's name with its ending inside
// the names
int container_pattern_holds(const Places *places, const unsigned char *pattern, unsigned long long size)
{
    if ((places->header_bytes > size) ||
        !place_inside(places->segment_table, places->segment_table_width, places->header_bytes) ||
        !place_inside(places->section_table, places->section_table_width, places->header_bytes) ||
        !place_inside(places->segment_entry, places->segment_entry_width, places->header_bytes) ||
        !place_inside(places->segment_count, places->segment_count_width, places->header_bytes) ||
        !place_inside(places->section_entry, places->section_entry_width, places->header_bytes) ||
        !place_inside(places->section_count, places->section_count_width, places->header_bytes) ||
        !place_inside(places->strings_index, places->strings_index_width, places->header_bytes) ||
        !place_inside(places->symbol_size, places->symbol_size_width, places->symbol_bytes))
    {
        return 0;
    }
    const unsigned long long table = container_value_read(&pattern[places->section_table], places->section_table_width);
    const unsigned long long entry = container_value_read(&pattern[places->section_entry], places->section_entry_width);
    const unsigned long long count = container_value_read(&pattern[places->section_count], places->section_count_width);
    const unsigned long long names_index =
        container_value_read(&pattern[places->strings_index], places->strings_index_width);
    const unsigned long long segment_table =
        container_value_read(&pattern[places->segment_table], places->segment_table_width);
    const unsigned long long segment_entry =
        container_value_read(&pattern[places->segment_entry], places->segment_entry_width);
    const unsigned long long segments =
        container_value_read(&pattern[places->segment_count], places->segment_count_width);
    if (!place_inside(places->section_name, places->section_name_width, entry) ||
        !place_inside(places->section_offset, places->section_offset_width, entry) ||
        !place_inside(places->section_size, places->section_size_width, entry) ||
        !place_inside(places->section_info, places->section_info_width, entry) ||
        !place_inside(places->section_align, places->section_align_width, entry) || (names_index >= count) ||
        (count > CONTAINER_SECTIONS) || (entry == 0ull) || (table > size) || (count > ((size - table) / entry)))
    {
        return 0;
    }
    if ((segments != 0ull) &&
        (!place_inside(places->segment_offset, places->segment_offset_width, segment_entry) ||
         !place_inside(places->segment_file_size, places->segment_file_width, segment_entry) ||
         !place_inside(places->segment_memory_size, places->segment_memory_width, segment_entry) ||
         (segment_entry == 0ull) || (segment_table > size) || (segments > ((size - segment_table) / segment_entry))))
    {
        return 0;
    }
    const unsigned long long names_header = table + (names_index * entry);
    const unsigned long long names_at =
        container_value_read(&pattern[names_header + places->section_offset], places->section_offset_width);
    const unsigned long long names_size =
        container_value_read(&pattern[names_header + places->section_size], places->section_size_width);
    if ((names_at > size) || (names_size > (size - names_at)))
    {
        return 0;
    }
    for (unsigned long long index = 0ull; index < count; index += 1ull)
    {
        const unsigned long long at = table + (index * entry);
        const unsigned long long offset =
            container_value_read(&pattern[at + places->section_offset], places->section_offset_width);
        const unsigned long long length =
            container_value_read(&pattern[at + places->section_size], places->section_size_width);
        if ((index != 0ull) && ((offset > size) || (length > (size - offset))))
        {
            return 0;
        }
        const unsigned long long name =
            container_value_read(&pattern[at + places->section_name], places->section_name_width);
        if ((name >= names_size) || (memchr(&pattern[names_at + name], '\0', names_size - name) == NULL))
        {
            return 0;
        }
    }
    return 1;
}

// the section table of `pattern`, with the count and where the names lie
unsigned long long container_sections_of(const Places *places, const unsigned char *pattern, unsigned int *count,
                                         unsigned long long *strings, unsigned long long *entry)
{
    const unsigned long long table = container_value_read(&pattern[places->section_table], places->section_table_width);
    *entry = container_value_read(&pattern[places->section_entry], places->section_entry_width);
    *count = (unsigned int)container_value_read(&pattern[places->section_count], places->section_count_width);
    const unsigned long long names =
        table + (container_value_read(&pattern[places->strings_index], places->strings_index_width) * *entry);
    *strings = container_value_read(&pattern[names + places->section_offset], places->section_offset_width);
    return table;
}

unsigned int container_registers_read(const ContainerLayout *layout, const unsigned char *pattern,
                                      unsigned long long pattern_size, const char *part)
{
    Places places;
    char named[256];
    if (!container_places_read(layout, &places) || !container_pattern_holds(&places, pattern, pattern_size) ||
        !layout_text(layout, "section.code", part, named, sizeof(named)))
    {
        return 0u;
    }
    unsigned int count = 0u;
    unsigned long long strings = 0ull;
    unsigned long long entry = 0ull;
    const unsigned long long table = container_sections_of(&places, pattern, &count, &strings, &entry);
    unsigned int found = 0u;
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        const unsigned long long at = table + ((unsigned long long)index * entry);
        const unsigned long long name =
            strings + container_value_read(&pattern[at + places.section_name], places.section_name_width);
        if (strcmp((const char *)&pattern[name], named) == 0)
        {
            found =
                (unsigned int)(container_value_read(&pattern[at + places.section_info], places.section_info_width) >>
                               places.registers_shift);
        }
    }
    return found;
}

// the place of the section named `name`, or `count` where the pattern holds none
unsigned int container_section_named(const Places *places, const unsigned char *pattern, const Section *sections,
                                     unsigned int count, unsigned long long strings, const char *name)
{
    unsigned int found = count;
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        const unsigned long long at =
            strings +
            container_value_read(&pattern[sections[index].was_at + places->section_name], places->section_name_width);
        found = (strcmp((const char *)&pattern[at], name) == 0) ? index : found;
    }
    return found;
}

// the attribute `attribute` of these bytes, or NULL where they hold none; its value's size through `value_size`
const unsigned char *container_attribute_of(const Places *places, const unsigned char *bytes, unsigned long long size,
                                            unsigned long long attribute, unsigned long long *value_size)
{
    unsigned long long at = 0ull;
    while ((at + places->attribute_header) <= size)
    {
        const unsigned long long format = bytes[at];
        const unsigned long long named = bytes[at + 1u];
        const unsigned long long kept =
            (format == places->attribute_format_value) ? container_value_read(&bytes[at + 2u], 2u) : 0ull;
        // a value that runs past the section ends the read, and the section holds no attribute past it
        if (kept > (size - (at + places->attribute_header)))
        {
            break;
        }
        if (named == attribute)
        {
            *value_size = kept;
            return &bytes[at];
        }
        at += places->attribute_header + kept;
    }
    *value_size = 0ull;
    return NULL;
}
