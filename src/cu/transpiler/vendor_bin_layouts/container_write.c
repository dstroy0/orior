// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// container_write.c: one container written from another, with new code in it for one part or many, driven by a layout
// file
#include "../../../../includes/platform/platform.h"
#include "container_write_internal.h"
#include "memoria_operor/memoria_operor.h"

static void value_put(unsigned char *bytes, unsigned int width, unsigned long long value)
{
    for (unsigned int at = 0u; at < width; at += 1u)
    {
        bytes[at] = (unsigned char)((value >> (8u * at)) & 0xffULL);
    }
}

static unsigned long long rounded(unsigned long long at, unsigned long long align)
{
    return ((align > 1ULL) && ((at % align) != 0ULL)) ? (at + (align - (at % align))) : at;
}

// The attributes of the part, written with the exits that this code carries, into `written`. The return is its size, or
// 0 where the section carries no exit attribute (`attribute_exits`) and the call wrote nothing
static unsigned long long attributes_written(const Places *places, const unsigned char *bytes, unsigned long long size,
                                             const unsigned int *exits, unsigned int exit_count, unsigned char *written,
                                             unsigned long long room)
{
    unsigned long long value_size = 0ULL;
    const unsigned char *const found =
        container_attribute_of(places, bytes, size, places->attribute_exits, &value_size);
    if (found == NULL)
    {
        // A section with no exit attribute belongs to something that the target calls and returns from, not to
        // something that it enters. An entry ends where the container says it ends, and the other ends on its own. To
        // carry the section over untouched is right for that case alone
        if (exit_count != 0u)
        {
            return 0ULL;
        }
        if (size > room)
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  container_write: the attributes take ");
                mmgr_line_decimal(output_line, size, 1u);
                mmgr_line_put(output_line, " bytes where the room is ");
                mmgr_line_decimal(output_line, room, 1u);
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            return 0ULL;
        }
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = written, .src = bytes, .bytes = size);
        return size;
    }
    const unsigned long long before = (unsigned long long)(found - bytes);
    const unsigned long long after = before + places->attribute_header + value_size;
    const unsigned long long whole = before + places->attribute_header + (4ULL * exit_count) + (size - after);
    if (whole > room)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  container_write: the attributes take ");
            mmgr_line_decimal(output_line, whole, 1u);
            mmgr_line_put(output_line, " bytes where the room is ");
            mmgr_line_decimal(output_line, room, 1u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0ULL;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = written, .src = bytes, .bytes = before);
    written[before] = (unsigned char)places->attribute_format_value;
    written[before + 1u] = (unsigned char)places->attribute_exits;
    value_put(&written[before + 2u], 2u, 4ULL * exit_count);
    for (unsigned int number = 0u; number < exit_count; number += 1u)
    {
        value_put(&written[before + places->attribute_header + (4ULL * number)], 4u, exits[number]);
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &written[before + places->attribute_header + (4ULL * exit_count)],
               .src = &bytes[after], .bytes = size - after);
    return whole;
}

// the register count in the whole container's attributes, set for the part's symbol
static void registers_set(const Places *places, unsigned char *bytes, unsigned long long size, unsigned int symbol,
                          unsigned int registers)
{
    unsigned long long value_size = 0ULL;
    const unsigned char *const found =
        container_attribute_of(places, bytes, size, places->attribute_registers, &value_size);
    if ((found != NULL) && (value_size == 8ULL) &&
        (container_value_read(&found[places->attribute_header], 4u) == (unsigned long long)symbol))
    {
        value_put(&bytes[(unsigned long long)(found - bytes) + places->attribute_header + 4ULL], 4u, registers);
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
    if (!container_pattern_matches(&places, pattern, args->pattern_size))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  container_write: a table, a section or a name of the pattern lies past its ");
            mmgr_line_decimal(output_line, args->pattern_size, 1u);
            mmgr_line_put(output_line, " bytes");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    unsigned int sections = 0u;
    unsigned long long strings = 0ULL;
    unsigned long long section_entry = 0ULL;
    const unsigned long long section_table =
        container_sections_of(&places, pattern, &sections, &strings, &section_entry);
    const unsigned long long segment_table =
        container_value_read(&pattern[places.segment_table], places.segment_table_width);
    const unsigned long long segment_entry =
        container_value_read(&pattern[places.segment_entry], places.segment_entry_width);
    const unsigned int segments =
        (unsigned int)container_value_read(&pattern[places.segment_count], places.segment_count_width);
    // the sections as the write stores them, as many as CONTAINER_SECTIONS bounds, stored apart from the stack
    static Section s_sections[CONTAINER_SECTIONS];
    Section *const section_list = s_sections;
    for (unsigned int index = 0u; index < sections; index += 1u)
    {
        const unsigned long long at = section_table + ((unsigned long long)index * section_entry);
        section_list[index].was_at = at;
        section_list[index].type = container_value_read(&pattern[at + places.section_type], places.section_type_width);
        section_list[index].goes_at =
            container_value_read(&pattern[at + places.section_offset], places.section_offset_width);
        // the call arranges a section that takes no bytes in the file at no length, and it keeps the size that its
        // header gives
        const int takes_bytes = container_section_takes_bytes(&places, pattern, at, index);
        section_list[index].bytes = takes_bytes ? &pattern[section_list[index].goes_at] : pattern;
        section_list[index].size =
            takes_bytes ? container_value_read(&pattern[at + places.section_size], places.section_size_width) : 0ULL;
        section_list[index].align =
            container_value_read(&pattern[at + places.section_align], places.section_align_width);
    }
    char named[256];
    unsigned int all_info = sections;
    unsigned int symbols = sections;
    if (layout_text(args->layout, "section.program_info", NULL, named, sizeof(named)))
    {
        all_info = container_section_named(&places, pattern, section_list, sections, strings, named);
    }
    if (layout_text(args->layout, "section.symbols", NULL, named, sizeof(named)))
    {
        symbols = container_section_named(&places, pattern, section_list, sections, strings, named);
    }
    if ((all_info == sections) || (symbols == sections))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line,
                          "  container_write: the pattern contains no program attributes or no symbol table");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    // every part's place, and the room its rewritten attributes take: what they took, and four bytes an exit more
    PartPlace *const place = (PartPlace *)platform_cell_cleared((size_t)args->part_count + 1u, sizeof(PartPlace),
                                                                platform_place_g_program_owner, 0);
    if (place == NULL)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  container_write: no memory for the places of ");
            mmgr_line_decimal(output_line, args->part_count, 1u);
            mmgr_line_put(output_line, " parts");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    unsigned long long attribute_room = 0ULL;
    int found = 1;
    for (unsigned int part = 0u; found && (part < args->part_count); part += 1u)
    {
        const ContainerPart *const one = &args->parts[part];
        place[part].code_index = sections;
        place[part].part_info = sections;
        if (layout_text(args->layout, "section.code", one->part, named, sizeof(named)))
        {
            place[part].code_index = container_section_named(&places, pattern, section_list, sections, strings, named);
        }
        if (layout_text(args->layout, "section.part_info", one->part, named, sizeof(named)))
        {
            place[part].part_info = container_section_named(&places, pattern, section_list, sections, strings, named);
        }
        found = (place[part].code_index != sections) && (place[part].part_info != sections);
        if (!found)
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  container_write: the pattern contains no part named ");
                mmgr_line_put(output_line, one->part);
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            break;
        }
        place[part].symbol =
            (unsigned int)(container_value_read(
                               &pattern[section_list[place[part].code_index].was_at + places.section_info],
                               places.section_info_width) &
                           places.registers_symbol_mask);
        // the call writes the size of the symbol inside the symbol table
        found = (((unsigned long long)place[part].symbol * places.symbol_bytes) + places.symbol_size +
                 places.symbol_size_width) <= section_list[symbols].size;
        if (!found)
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  container_write: the symbol ");
                mmgr_line_decimal(output_line, place[part].symbol, 1u);
                mmgr_line_put(output_line, " of ");
                mmgr_line_put(output_line, one->part);
                mmgr_line_put(output_line, " lies past the ");
                mmgr_line_decimal(output_line, section_list[symbols].size, 1u);
                mmgr_line_put(output_line, " bytes of its symbol table");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            break;
        }
        attribute_room += section_list[place[part].part_info].size + (4ULL * one->exit_count);
    }
    // one block of memory for the program's attributes and every part's own, each part's after the last
    unsigned char *const block =
        found ? (unsigned char *)platform_cell_taken((size_t)(section_list[all_info].size + attribute_room + 1ULL),
                                                     platform_place_g_program_owner, 0)
              : NULL;
    if (block == NULL)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  container_write: no room was had for the attributes of ");
            mmgr_line_decimal(output_line, args->part_count, 1u);
            mmgr_line_put(output_line, " parts");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        platform_cell_released(place, platform_place_g_program_owner);
        return 0;
    }
    unsigned char *const all = block;
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = all, .src = section_list[all_info].bytes,
               .bytes = section_list[all_info].size);
    section_list[all_info].bytes = all;
    unsigned long long used = section_list[all_info].size;
    for (unsigned int part = 0u; found && (part < args->part_count); part += 1u)
    {
        const ContainerPart *const one = &args->parts[part];
        const unsigned long long part_room = section_list[place[part].part_info].size + (4ULL * one->exit_count);
        const unsigned long long part_size = attributes_written(&places, section_list[place[part].part_info].bytes,
                                                                section_list[place[part].part_info].size, one->exits,
                                                                one->exit_count, block + used, part_room);
        found = (part_size != 0ULL);
        if (!found)
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  container_write: ");
                mmgr_line_put(output_line, one->part);
                mmgr_line_put(output_line, " contains no endings to write");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            break;
        }
        if (one->registers != 0u)
        {
            registers_set(&places, all, section_list[all_info].size, place[part].symbol, one->registers);
        }
        section_list[place[part].code_index].bytes = one->code;
        section_list[place[part].code_index].size = one->code_size;
        section_list[place[part].part_info].bytes = block + used;
        section_list[place[part].part_info].size = part_size;
        used += part_size;
    }
    if (!found)
    {
        platform_cell_released(block, platform_place_g_program_owner);
        platform_cell_released(place, platform_place_g_program_owner);
        return 0;
    }

    unsigned long long at = places.header_bytes;
    for (unsigned int index = 1u; index < sections; index += 1u)
    {
        section_list[index].goes_at = rounded(at, section_list[index].align);
        at = section_list[index].goes_at + section_list[index].size;
    }
    const unsigned long long new_sections = rounded(at, places.table_align);
    const unsigned long long new_segments = new_sections + ((unsigned long long)sections * section_entry);
    const unsigned long long whole = new_segments + ((unsigned long long)segments * segment_entry);
    if (whole > room)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  container_write: the container takes ");
            mmgr_line_decimal(output_line, whole, 1u);
            mmgr_line_put(output_line, " bytes where the room is ");
            mmgr_line_decimal(output_line, room, 1u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        platform_cell_released(block, platform_place_g_program_owner);
        platform_cell_released(place, platform_place_g_program_owner);
        return 0;
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = written, .bytes = whole, .val = 0);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = written, .src = pattern, .bytes = places.header_bytes);
    value_put(&written[places.section_table], places.section_table_width, new_sections);
    value_put(&written[places.segment_table], places.segment_table_width, new_segments);
    for (unsigned int index = 0u; index < sections; index += 1u)
    {
        unsigned char *const header = &written[new_sections + ((unsigned long long)index * section_entry)];
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = header, .src = &pattern[section_list[index].was_at],
                   .bytes = section_entry);
        if (index != 0u)
        {
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &written[section_list[index].goes_at],
                       .src = section_list[index].bytes, .bytes = section_list[index].size);
            value_put(&header[places.section_offset], places.section_offset_width, section_list[index].goes_at);
            if (section_list[index].type != places.section_type_nobits)
            {
                value_put(&header[places.section_size], places.section_size_width, section_list[index].size);
            }
        }
    }
    unsigned char *const symbol_table = &written[section_list[symbols].goes_at];
    for (unsigned int part = 0u; part < args->part_count; part += 1u)
    {
        const ContainerPart *const one = &args->parts[part];
        // a part given a register count has it in its code section's header; one given none retains the pattern's
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
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = header, .src = &pattern[was], .bytes = segment_entry);
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
        unsigned long long last = 0ULL;
        for (unsigned int index_at = 1u; index_at < sections; index_at += 1u)
        {
            const unsigned long long was_section = container_value_read(
                &pattern[section_list[index_at].was_at + places.section_offset], places.section_offset_width);
            if ((was_section >= was_at) && (was_section < (was_at + was_size)))
            {
                first = (section_list[index_at].goes_at < first) ? section_list[index_at].goes_at : first;
                last = ((section_list[index_at].goes_at + section_list[index_at].size) > last)
                           ? (section_list[index_at].goes_at + section_list[index_at].size)
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
    platform_cell_released(block, platform_place_g_program_owner);
    platform_cell_released(place, platform_place_g_program_owner);
    *size = whole;
    return 1;
}

int container_write(const ContainerWrite *args, unsigned char *written, unsigned long long room,
                    unsigned long long *size)
{
    const ContainerPart one = {args->part, args->code, args->code_size, args->registers, args->exits, args->exit_count};
    const ContainerParts parts = {args->layout, args->pattern, args->pattern_size, &one, 1u};
    return container_parts_write(&parts, written, room, size);
}
