// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cut.c: a cubin's listing cut into each kernel's, every label a kernel's lines name put as the offset it stands at
#include "../cubin_listing.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the offset a label holds before the instruction it stands before is read
#define CUBIN_LABEL_UNSET (~0ull)

// one line of a code section the cut keeps: where it lies in the listing, and the offset the vendor printed for it
typedef struct
{
    size_t begin;
    size_t end;
    unsigned long long offset;
} CubinLine;

// one label of a code section: its name where it lies in the listing, and the offset of the instruction after it
typedef struct
{
    size_t begin;
    size_t length;
    unsigned long long offset;
} CubinLabel;

// the lines and the labels of the section the cut is in, kept in memory grown as a section needs and kept over the
// whole cut, so that a section takes no memory of its own
typedef struct
{
    CubinLine *lines;
    size_t line_count;
    size_t line_room;
    CubinLabel *labels;
    size_t label_count;
    size_t label_room;
} CubinSection;

// `size` bytes at `bytes` added to the end of the cut's text: 1, or 0 where the system gave no memory
static int cubin_text_added(CubinListingCut *cut, const char *bytes, size_t size)
{
    if ((cut->text_size + size) > cut->text_room)
    {
        size_t grown = (cut->text_room != 0u) ? cut->text_room : 1048576u;
        while (grown < (cut->text_size + size))
        {
            grown *= 2u;
        }
        char *const larger = (char *)realloc(cut->text, grown);
        if (larger == NULL)
        {
            return 0;
        }
        cut->text = larger;
        cut->text_room = grown;
    }
    memcpy(cut->text + cut->text_size, bytes, size);
    cut->text_size += size;
    return 1;
}

// 1 where `c` is a space or a tab
static int cubin_blank(char c)
{
    return (c == ' ') || (c == '\t');
}

// the value of the hex digit `c`, or 16 where it is none
static unsigned int cubin_hex(char c)
{
    return ((c >= '0') && (c <= '9'))   ? (unsigned int)(c - '0')
           : ((c >= 'a') && (c <= 'f')) ? (unsigned int)(c - 'a' + 10)
           : ((c >= 'A') && (c <= 'F')) ? (unsigned int)(c - 'A' + 10)
                                        : 16u;
}

// 1 where the line from `begin` to `end` is an instruction, its first word past its space `/*<hex>*/`, the hex its
// offset through `offset`
static int cubin_instruction_line(const char *listing, size_t begin, size_t end, unsigned long long *offset)
{
    size_t at = begin;
    while ((at < end) && cubin_blank(listing[at]))
    {
        at += 1u;
    }
    if (((at + 2u) >= end) || (listing[at] != '/') || (listing[at + 1u] != '*'))
    {
        return 0;
    }
    at += 2u;
    unsigned long long value = 0ull;
    size_t digits = 0u;
    while ((at < end) && (cubin_hex(listing[at]) < 16u))
    {
        value = (value << 4u) | cubin_hex(listing[at]);
        digits += 1u;
        at += 1u;
    }
    if ((digits == 0u) || ((at + 1u) >= end) || (listing[at] != '*') || (listing[at + 1u] != '/'))
    {
        return 0;
    }
    *offset = value;
    return 1;
}

// 1 where the line from `begin` to `end` is a label: no space before it, no space in it, and a `:` its last
static int cubin_label_line(const char *listing, size_t begin, size_t end)
{
    if ((end <= (begin + 1u)) || (listing[end - 1u] != ':'))
    {
        return 0;
    }
    for (size_t at = begin; at < (end - 1u); at += 1u)
    {
        if (cubin_blank(listing[at]))
        {
            return 0;
        }
    }
    return 1;
}

// the name of the code section a `.section` line from `begin` to `end` opens, past `.text.` and short of its first
// `,`, through `name` and `length`: 1, or 0 where it opens no code section
static int cubin_code_section(const char *listing, size_t begin, size_t end, size_t *name, size_t *length)
{
    static const char s_section[] = ".section";
    static const char s_code[] = ".text.";
    size_t at = begin;
    while ((at < end) && cubin_blank(listing[at]))
    {
        at += 1u;
    }
    // `.section` and a space: `.sectioninfo` and `.sectionflags` open no section
    if (((end - at) <= (sizeof(s_section) - 1u)) || (memcmp(listing + at, s_section, sizeof(s_section) - 1u) != 0) ||
        !cubin_blank(listing[at + (sizeof(s_section) - 1u)]))
    {
        return 0;
    }
    at += sizeof(s_section) - 1u;
    while ((at < end) && cubin_blank(listing[at]))
    {
        at += 1u;
    }
    if (((end - at) < (sizeof(s_code) - 1u)) || (memcmp(listing + at, s_code, sizeof(s_code) - 1u) != 0))
    {
        *length = 0u;
        return 1;
    }
    at += sizeof(s_code) - 1u;
    size_t last = at;
    while ((last < end) && (listing[last] != ',') && !cubin_blank(listing[last]))
    {
        last += 1u;
    }
    *name = at;
    *length = last - at;
    return 1;
}

// the place past the `*)` that ends a note begun before `from`, searched up to `end`, or NULL where none ends it
static const char *cubin_note_end(const char *listing, size_t from, size_t end)
{
    for (size_t at = from; (at + 1u) < end; at += 1u)
    {
        if ((listing[at] == '*') && (listing[at + 1u] == ')'))
        {
            return listing + at + 2u;
        }
    }
    return NULL;
}

// the section's kept lines written into the cut as kernel `kernel`'s listing, each label they name put as its offset
static int cubin_section_written(const char *listing, CubinSection *section, unsigned int kernel,
                                 CubinListingCut *cut)
{
    // a label after the last instruction stands at the end of the code
    const unsigned long long end_offset =
        (section->line_count != 0u) ? (section->lines[section->line_count - 1u].offset + 16ull) : 0ull;
    for (size_t at = 0u; at < section->label_count; at += 1u)
    {
        section->labels[at].offset =
            (section->labels[at].offset == CUBIN_LABEL_UNSET) ? end_offset : section->labels[at].offset;
    }
    cut->start[kernel] = cut->text_size;
    int held = 1;
    for (size_t line = 0u; held && (line < section->line_count); line += 1u)
    {
        size_t at = section->lines[line].begin;
        const size_t end = section->lines[line].end;
        while (held && (at < end))
        {
            const char *const open = (const char *)memchr(listing + at, '`', end - at);
            size_t mark = (open != NULL) ? (size_t)(open - listing) : end;
            // a note the cubin carries for an instruction, `(*"BRANCH_TARGETS ..."*)`, is the container's and no part
            // of the instruction: it stands where a raw read prints one space, and the note and the spaces before it
            // are put as that space
            const char *const note = (const char *)memchr(listing + at, '(', mark - at);
            const int noted = (note != NULL) && (((size_t)(note - listing) + 1u) < end) && (note[1] == '*');
            const char *const note_end =
                noted ? cubin_note_end(listing, (size_t)(note - listing) + 2u, end) : NULL;
            if (note_end != NULL)
            {
                size_t kept = (size_t)(note - listing);
                while ((kept > at) && cubin_blank(listing[kept - 1u]))
                {
                    kept -= 1u;
                }
                held = cubin_text_added(cut, listing + at, kept - at) && cubin_text_added(cut, " ", 1u);
                at = (size_t)(note_end - listing);
                continue;
            }
            held = cubin_text_added(cut, listing + at, mark - at);
            at = mark;
            if (!held || (at >= end))
            {
                break;
            }
            // `(<label>): the label put as its offset where the section sets it, the mark kept as printed otherwise
            const char *const close = ((at + 2u) < end) && (listing[at + 1u] == '(')
                                          ? (const char *)memchr(listing + at + 2u, ')', end - (at + 2u))
                                          : NULL;
            size_t found = section->label_count;
            const size_t name = at + 2u;
            const size_t length = (close != NULL) ? ((size_t)(close - listing) - name) : 0u;
            for (size_t label = 0u; (close != NULL) && (label < section->label_count); label += 1u)
            {
                if ((section->labels[label].length == length) &&
                    (memcmp(listing + section->labels[label].begin, listing + name, length) == 0))
                {
                    found = label;
                }
            }
            if (found == section->label_count)
            {
                held = cubin_text_added(cut, listing + at, 1u);
                at += 1u;
                continue;
            }
            char offset[32];
            const int written = snprintf(offset, sizeof(offset), "0x%llx", section->labels[found].offset);
            held = (written > 0) && cubin_text_added(cut, offset, (size_t)written);
            at = (size_t)(close - listing) + 1u;
        }
        held = held && cubin_text_added(cut, "\n", 1u);
    }
    cut->size[kernel] = cut->text_size - cut->start[kernel];
    cut->found[kernel] = 1;
    return held;
}

// room for one more of `size` bytes in `*items`, which holds `*room`: 1, or 0 where the system gave no memory
static int cubin_room(void **items, size_t *room, size_t count, size_t size)
{
    if (count < *room)
    {
        return 1;
    }
    const size_t grown = (*room != 0u) ? (*room * 2u) : 256u;
    void *const larger = realloc(*items, grown * size);
    if (larger == NULL)
    {
        return 0;
    }
    *items = larger;
    *room = grown;
    return 1;
}

int cubin_listing_cut(const char *listing, size_t length, const char *const *names, unsigned int count,
                      CubinListingCut *cut)
{
    memset(cut, 0, sizeof(*cut));
    cut->start = (size_t *)calloc((size_t)count + 1u, sizeof(size_t));
    cut->size = (size_t *)calloc((size_t)count + 1u, sizeof(size_t));
    cut->found = (int *)calloc((size_t)count + 1u, sizeof(int));
    if ((cut->start == NULL) || (cut->size == NULL) || (cut->found == NULL))
    {
        return 0;
    }
    CubinSection section;
    memset(&section, 0, sizeof(section));
    unsigned int kernel = count;
    int held = 1;
    size_t at = 0u;
    while (held && (at < length))
    {
        const char *const newline = (const char *)memchr(listing + at, '\n', length - at);
        size_t end = (newline != NULL) ? (size_t)(newline - listing) : length;
        const size_t next = end + 1u;
        end = ((end > at) && (listing[end - 1u] == '\r')) ? (end - 1u) : end;
        size_t name = 0u;
        size_t name_length = 0u;
        unsigned long long offset = 0ull;
        if (cubin_code_section(listing, at, end, &name, &name_length))
        {
            // a section opened: the section the cut was in written as its kernel's
            if (kernel != count)
            {
                held = cubin_section_written(listing, &section, kernel, cut);
            }
            section.line_count = 0u;
            section.label_count = 0u;
            kernel = count;
            for (unsigned int one = 0u; (name_length != 0u) && (one < count); one += 1u)
            {
                if ((strlen(names[one]) == name_length) && (memcmp(names[one], listing + name, name_length) == 0) &&
                    !cut->found[one])
                {
                    kernel = one;
                }
            }
        }
        else if ((kernel != count) && cubin_instruction_line(listing, at, end, &offset))
        {
            held = cubin_room((void **)&section.lines, &section.line_room, section.line_count, sizeof(CubinLine));
            if (held)
            {
                section.lines[section.line_count] = (CubinLine){at, end, offset};
                section.line_count += 1u;
            }
            // every label read since the last instruction stands before this one
            for (size_t label = section.label_count; label > 0u; label -= 1u)
            {
                if (section.labels[label - 1u].offset != CUBIN_LABEL_UNSET)
                {
                    break;
                }
                section.labels[label - 1u].offset = offset;
            }
        }
        else if ((kernel != count) && cubin_label_line(listing, at, end))
        {
            held = cubin_room((void **)&section.labels, &section.label_room, section.label_count, sizeof(CubinLabel));
            if (held)
            {
                section.labels[section.label_count] = (CubinLabel){at, (end - at) - 1u, CUBIN_LABEL_UNSET};
                section.label_count += 1u;
            }
        }
        at = next;
    }
    // the listing ended: the section the cut was in written as its kernel's
    if (held && (kernel != count))
    {
        held = cubin_section_written(listing, &section, kernel, cut);
    }
    free(section.lines);
    free(section.labels);
    return held;
}

void cubin_listing_cut_freed(CubinListingCut *cut)
{
    free(cut->text);
    free(cut->start);
    free(cut->size);
    free(cut->found);
    memset(cut, 0, sizeof(*cut));
}
