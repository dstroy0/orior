// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// What the container_*.c pieces share: the places a layout gives, a part as the emitter holds it, and the
// functions one piece calls in another
#ifndef CONTAINER_WRITE_INTERNAL_H
#define CONTAINER_WRITE_INTERNAL_H

#include "container_write.h"

#include <stdio.h>
#include <string.h>

// what bounds the memory this takes. Neither is the format's to state: a format says where things are, and these
// say how much of it this will carry at once
#define CONTAINER_SECTIONS 64u
#define CONTAINER_ATTRIBUTE_ROOM 4096u

// every place this reads or writes, taken out of the layout once
typedef struct
{
    unsigned long long segment_table, section_table, segment_entry, segment_count;
    unsigned long long section_entry, section_count, strings_index;
    unsigned long long section_name, section_offset, section_size, section_info, section_align;
    unsigned long long segment_offset, segment_file_size, segment_memory_size;
    unsigned long long symbol_size, symbol_bytes;
    unsigned int segment_table_width, section_table_width, segment_entry_width, segment_count_width;
    unsigned int section_entry_width, section_count_width, strings_index_width;
    unsigned int section_name_width, section_offset_width, section_size_width, section_info_width;
    unsigned int section_align_width, segment_offset_width, segment_file_width, segment_memory_width;
    unsigned int symbol_size_width;
    unsigned long long header_bytes, instruction, table_align, table_entry;
    unsigned long long registers_shift, registers_symbol_mask;
    unsigned long long attribute_header, attribute_format_value, attribute_registers, attribute_exits;
} Places;

// a part of the container as this holds it: where it was, the bytes it takes now and how many, and where it goes
typedef struct
{
    unsigned long long was_at;
    const unsigned char *bytes;
    unsigned long long size;
    unsigned long long align;
    unsigned long long goes_at;
} Section;

unsigned long long container_value_read(const unsigned char *bytes, unsigned int width);

int container_places_read(const ContainerLayout *layout, Places *places);

int container_pattern_holds(const Places *places, const unsigned char *pattern, unsigned long long size);

unsigned long long container_sections_of(const Places *places, const unsigned char *pattern, unsigned int *count,
                                         unsigned long long *strings, unsigned long long *entry);

unsigned int container_section_named(const Places *places, const unsigned char *pattern, const Section *sections,
                                     unsigned int count, unsigned long long strings, const char *name);

const unsigned char *container_attribute_of(const Places *places, const unsigned char *bytes, unsigned long long size,
                                            unsigned long long attribute, unsigned long long *value_size);

#endif
