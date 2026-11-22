// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// names.c: the names: a name stored, copied and numbered, a kernel named, a line read and spaced, a guard and a form's
// guard, and the operation of a line
#include "stick_internal.h"
#include "../../../../../platform/platform.h"
#include "memoria_operor/memoria_linea.h"
#include "cellularum_laboro/cellularum_linea.h"

// the table of names: each operation's name at its number, and a hash of the names to their numbers
char **s_name;

unsigned int s_names;
static unsigned int s_name_room;
static unsigned int s_hash[STICK_HASH];
StickKernel *s_kernel;
unsigned int s_kernels;
StickEngine *s_engine;
unsigned int s_engines;
KfileLines s_file;

// `pointer`, or the run ended where the system gave no memory
void *stick_allocated(void *pointer)
{
    if (pointer == NULL)
    {
        {
            const char *const error_text = "orior_measuring_stick_read: the system gave no memory\n";
            platform_terminal_write(1, error_text,
                                    (unsigned long long)mmgr_text_length(error_text, PLATFORM_TEXT_TO_TERMINATOR));
        }
        platform_process_ended(2);
    }
    return pointer;
}

// `length` characters at `text` copied into an allocation of their own, ended
char *stick_copy(const char *text, size_t length)
{
    char *const copy = (char *)stick_allocated(platform_cell_taken(length + 1u, platform_place_g_program_owner, 0));
    mmgr_bytes_copy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

// `number` added after every number `numbers` contains
void stick_number_add(StickNumbers *numbers, unsigned int number)
{
    if (numbers->count == numbers->room)
    {
        numbers->room = (numbers->room == 0u) ? 64u : (numbers->room * 2u);
        numbers->number = (unsigned int *)stick_allocated(
            platform_cell_grown(numbers->number, numbers->count * sizeof(unsigned int),
                                numbers->room * sizeof(unsigned int), platform_place_g_program_owner, 0));
    }
    numbers->number[numbers->count] = number;
    numbers->count += 1u;
}

// the number of the name `length` characters at `text`, the name put in the table where it contains none
unsigned int stick_named(const char *text, size_t length)
{
    // FNV-1a over the name's characters
    unsigned int hash = 2166136261u;
    for (size_t at = 0u; at < length; at += 1u)
    {
        hash = (hash ^ (unsigned char)text[at]) * 16777619u;
    }
    unsigned int slot = hash & (STICK_HASH - 1u);
    while (s_hash[slot] != 0u)
    {
        const char *const name_text = s_name[s_hash[slot] - 1u];
        if ((mmgr_text_order(name_text, text, (size_t)(length)) == 0) && (name_text[length] == '\0'))
        {
            return s_hash[slot] - 1u;
        }
        slot = (slot + 1u) & (STICK_HASH - 1u);
    }
    if (s_names == (STICK_HASH / 2u))
    {
        {
            const char *const error_text = "orior_measuring_stick_read: more operations than the table supports\n";
            platform_terminal_write(1, error_text,
                                    (unsigned long long)mmgr_text_length(error_text, PLATFORM_TEXT_TO_TERMINATOR));
        }
        platform_process_ended(2);
    }
    if (s_names == s_name_room)
    {
        s_name_room = (s_name_room == 0u) ? 256u : (s_name_room * 2u);
        s_name = (char **)stick_allocated(platform_cell_grown(
            s_name, s_names * sizeof(char *), s_name_room * sizeof(char *), platform_place_g_program_owner, 0));
    }
    s_name[s_names] = stick_copy(text, length);
    s_names += 1u;
    s_hash[slot] = s_names;
    return s_names - 1u;
}

// the file at `path` read into s_file as the run stores it
void stick_read(const char *path)
{
    if (!kfile_lines_read(&s_file, path))
    {
        {
            const char *const error_text = "orior_measuring_stick_read: the system gave no memory\n";
            platform_terminal_write(1, error_text,
                                    (unsigned long long)mmgr_text_length(error_text, PLATFORM_TEXT_TO_TERMINATOR));
        }
        platform_process_ended(2);
    }
}

// the place past the space at `text`
const char *stick_spaced(const char *text)
{
    return text + mmgr_text_span(text, s_space, PLATFORM_TEXT_TO_TERMINATOR);
}

// the length of a guard at `text`, `@!?U?P[T0-9]+` and the space after it, or 0 where none is there
size_t stick_guard(const char *text)
{
    size_t at = 0u;
    if (text[at] != '@')
    {
        return 0u;
    }
    at += 1u;
    at += (text[at] == '!') ? 1u : 0u;
    at += (text[at] == 'U') ? 1u : 0u;
    if (text[at] != 'P')
    {
        return 0u;
    }
    at += 1u;
    const size_t marks = mmgr_text_span(&text[at], "T0123456789", PLATFORM_TEXT_TO_TERMINATOR);
    const size_t space = (marks != 0u) ? mmgr_text_span(&text[at + marks], s_space, PLATFORM_TEXT_TO_TERMINATOR) : 0u;
    return (space != 0u) ? (at + marks + space) : 0u;
}

// the length of a form's guard at `text`, `@!?{?[A-Za-z0-9_]+}?` and the space after it, or 0 where none is there
size_t stick_form_guard(const char *text)
{
    size_t at = 0u;
    if (text[at] != '@')
    {
        return 0u;
    }
    at += 1u;
    at += (text[at] == '!') ? 1u : 0u;
    at += (text[at] == '{') ? 1u : 0u;
    const size_t name = mmgr_text_span(&text[at], "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_",
                                       PLATFORM_TEXT_TO_TERMINATOR);
    if (name == 0u)
    {
        return 0u;
    }
    at += name;
    at += (text[at] == '}') ? 1u : 0u;
    const size_t space = mmgr_text_span(&text[at], s_space, PLATFORM_TEXT_TO_TERMINATOR);
    return (space != 0u) ? (at + space) : 0u;
}

// 1 with the number of the operation of a listing's instruction line `line`, its guard cut: `/*<hex>*/`, then the
// instruction up to its `;`, its first word the operation; 0 where the line contains none
int stick_operation_of(const char *line, unsigned int *operation)
{
    const char *at = stick_spaced(line);
    if (!mmgr_text_starts(at, "/*", PLATFORM_TEXT_TO_TERMINATOR))
    {
        return 0;
    }
    at += 2;
    const size_t digits = mmgr_text_span(at, "0123456789abcdef", PLATFORM_TEXT_TO_TERMINATOR);
    if ((digits < 4u) || (!mmgr_text_starts(&at[digits], "*/", PLATFORM_TEXT_TO_TERMINATOR)))
    {
        return 0;
    }
    at += digits + 2u;
    const size_t space = mmgr_text_span(at, s_space, PLATFORM_TEXT_TO_TERMINATOR);
    if (space == 0u)
    {
        return 0;
    }
    at += space;
    const char *const end = mmgr_text_char(at, ';', PLATFORM_TEXT_TO_TERMINATOR);
    if (end == NULL)
    {
        return 0;
    }
    // the instruction, the space before its `;` cut
    size_t length = (size_t)(end - at);
    while ((length != 0u) && (mmgr_text_char(s_space, at[length - 1u], PLATFORM_TEXT_TO_TERMINATOR) != NULL))
    {
        length -= 1u;
    }
    char *const instruction = stick_copy(at, length);
    const char *const text = stick_spaced(instruction + stick_guard(instruction));
    const size_t word = mmgr_text_span_not(text, s_space, PLATFORM_TEXT_TO_TERMINATOR);
    const int found = (word != 0u);
    if (found)
    {
        *operation = stick_named(text, word);
    }
    platform_cell_released(instruction, platform_place_g_program_owner);
    return found;
}
