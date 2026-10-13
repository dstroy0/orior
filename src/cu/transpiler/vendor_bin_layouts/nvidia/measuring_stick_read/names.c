// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// names.c: the names: a name held, copied and numbered, a kernel named, a line read and spaced, a guard and a form's guard, and the operation of a line
#include "stick_internal.h"

// the table of names: each operation's name at its number, and a hash of the names to their numbers
char **s_name;

unsigned int s_names;
static unsigned int s_name_room;
static unsigned int s_hash[STICK_HASH];
StickKernel *s_kernel;
unsigned int s_kernels;
StickEngine *s_engine;
unsigned int s_engines;
KtextFile s_file;

// `pointer`, or the run ended where the system gave no memory
void *stick_held(void *pointer)
{
    if (pointer == NULL)
    {
        fputs("measuring_stick_read: the system gave no memory\n", stderr);
        exit(2);
    }
    return pointer;
}

// `length` letters at `text` copied into an allocation of their own, ended
char *stick_copy(const char *text, size_t length)
{
    char *const copy = (char *)stick_held(malloc(length + 1u));
    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

// `number` kept after every number `numbers` holds
void stick_number_add(StickNumbers *numbers, unsigned int number)
{
    if (numbers->count == numbers->room)
    {
        numbers->room = (numbers->room == 0u) ? 64u : (numbers->room * 2u);
        numbers->number = (unsigned int *)stick_held(realloc(numbers->number, numbers->room * sizeof(unsigned int)));
    }
    numbers->number[numbers->count] = number;
    numbers->count += 1u;
}

// the number of the name `length` letters at `text`, the name put in the table where it holds none
unsigned int stick_named(const char *text, size_t length)
{
    // FNV-1a over the name's letters
    unsigned int hash = 2166136261u;
    for (size_t at = 0u; at < length; at += 1u)
    {
        hash = (hash ^ (unsigned char)text[at]) * 16777619u;
    }
    unsigned int slot = hash & (STICK_HASH - 1u);
    while (s_hash[slot] != 0u)
    {
        const char *const held = s_name[s_hash[slot] - 1u];
        if ((strncmp(held, text, length) == 0) && (held[length] == '\0'))
        {
            return s_hash[slot] - 1u;
        }
        slot = (slot + 1u) & (STICK_HASH - 1u);
    }
    if (s_names == (STICK_HASH / 2u))
    {
        fputs("measuring_stick_read: more operations than the table holds\n", stderr);
        exit(2);
    }
    if (s_names == s_name_room)
    {
        s_name_room = (s_name_room == 0u) ? 256u : (s_name_room * 2u);
        s_name = (char **)stick_held(realloc(s_name, s_name_room * sizeof(char *)));
    }
    s_name[s_names] = stick_copy(text, length);
    s_names += 1u;
    s_hash[slot] = s_names;
    return s_names - 1u;
}

// the file at `path` read into s_file as the run holds it
void stick_read(const char *path)
{
    if (!ktext_read(&s_file, path))
    {
        fputs("measuring_stick_read: the system gave no memory\n", stderr);
        exit(2);
    }
}

// the place past the space at `text`
const char *stick_spaced(const char *text)
{
    return text + strspn(text, s_space);
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
    const size_t marks = strspn(&text[at], "T0123456789");
    const size_t space = (marks != 0u) ? strspn(&text[at + marks], s_space) : 0u;
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
    const size_t name = strspn(&text[at], "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_");
    if (name == 0u)
    {
        return 0u;
    }
    at += name;
    at += (text[at] == '}') ? 1u : 0u;
    const size_t space = strspn(&text[at], s_space);
    return (space != 0u) ? (at + space) : 0u;
}

// 1 with the number of the operation of a listing's instruction line `line`, its guard cut: `/*<hex>*/`, then the
// instruction up to its `;`, its first word the operation; 0 where the line holds none
int stick_operation_of(const char *line, unsigned int *operation)
{
    const char *at = stick_spaced(line);
    if (strncmp(at, "/*", 2u) != 0)
    {
        return 0;
    }
    at += 2;
    const size_t digits = strspn(at, "0123456789abcdef");
    if ((digits < 4u) || (strncmp(&at[digits], "*/", 2u) != 0))
    {
        return 0;
    }
    at += digits + 2u;
    const size_t space = strspn(at, s_space);
    if (space == 0u)
    {
        return 0;
    }
    at += space;
    const char *const end = strchr(at, ';');
    if (end == NULL)
    {
        return 0;
    }
    // the instruction, the space before its `;` cut
    size_t length = (size_t)(end - at);
    while ((length != 0u) && (strchr(s_space, at[length - 1u]) != NULL))
    {
        length -= 1u;
    }
    char *const instruction = stick_copy(at, length);
    const char *const text = stick_spaced(instruction + stick_guard(instruction));
    const size_t word = strcspn(text, s_space);
    const int found = (word != 0u);
    if (found)
    {
        *operation = stick_named(text, word);
    }
    free(instruction);
    return found;
}
