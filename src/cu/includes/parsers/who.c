// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// who.c: the who of a write, its source and program numbered by the who table as the run holds it and its time on
// the run's one clock, its two words, its code and its name
#include "who.h"

#include "line_read.h"

#include "../file_defs/qry/writer/qry_member.h"
#include "verba_scribo/verba_scribo.h"

#include <string.h>

// the sources whose numbers were asked, each looked up in the table once
#define WHO_ASKED 64u

// the longest path a who names
#define WHO_PATH_LONGEST 1024u

static const char *s_who_asked[WHO_ASKED];
static unsigned int s_who_asked_number[WHO_ASKED];
static unsigned int s_who_asked_next;
static unsigned int s_who_program;
static int s_who_program_found;

static const char s_who_digits[] = "0123456789abcdef";

// the value of the hex digit `letter`: 0 to 15, or 16 where it is none
static unsigned int who_hex_digit(char letter)
{
    if ((letter >= '0') && (letter <= '9'))
    {
        return (unsigned int)(letter - '0');
    }
    if ((letter >= 'a') && (letter <= 'f'))
    {
        return (unsigned int)(letter - 'a') + 10u;
    }
    if ((letter >= 'A') && (letter <= 'F'))
    {
        return (unsigned int)(letter - 'A') + 10u;
    }
    return 16u;
}

// the `letters` hex digits at `text` read into `value`: 1, or 0 where one is no hex digit
static int who_hex(const char *text, unsigned int letters, unsigned long long *value)
{
    *value = 0ull;
    for (unsigned int at = 0u; at < letters; at += 1u)
    {
        const unsigned int digit = who_hex_digit(text[at]);
        if (digit == 16u)
        {
            return 0;
        }
        *value = (*value << 4u) | digit;
    }
    return 1;
}

// the who table as the run holds it: 1, or 0 where there is no run or no table
static int who_table(const unsigned char **table, unsigned long long *size)
{
    const char *const run_top = qry_member_top();
    char path[WHO_PATH_LONGEST + sizeof(WHO_TABLE) + 1u];
    const size_t top = (run_top != NULL) ? strlen(run_top) : 0u;
    if ((top == 0u) || (top >= WHO_PATH_LONGEST))
    {
        return 0;
    }
    memcpy(path, run_top, top);
    path[top] = '/';
    memcpy(path + top + 1u, WHO_TABLE, sizeof(WHO_TABLE));
    return qry_member_file_blob(path, table, size);
}

// the next row of the table at `*at` into `number` and `source`, which holds WHO_PATH_LONGEST: 1, or 0 where no row
// is left; a line that is no row gives number 0
static int who_row(const unsigned char *table, unsigned long long size, unsigned long long *at, unsigned int *number,
                   char *source)
{
    char line[WHO_PATH_LONGEST + 16u];
    if (!line_read(table, size, at, line, sizeof(line)))
    {
        return 0;
    }
    unsigned long long value = 0ull;
    const char *const tab = strchr(line, '\t');
    *number = 0u;
    source[0] = '\0';
    if ((line[0] != '0') || (line[1] != 'x') || (tab == NULL) || ((tab - line) != 6) || !who_hex(line + 2, 4u, &value))
    {
        return 1;
    }
    const size_t length = strcspn(tab + 1, "\r");
    if (length >= WHO_PATH_LONGEST)
    {
        return 1;
    }
    memcpy(source, tab + 1, length);
    source[length] = '\0';
    // four hex digits are below 0x10000, which an unsigned int holds
    *number = (unsigned int)value;
    return 1;
}

// 1 where the `length` letters at `one` and `other` are the same, a letter's case aside
static int who_same_letters(const char *one, const char *other, size_t length)
{
    for (size_t at = 0u; at < length; at += 1u)
    {
        const char left = ((one[at] >= 'A') && (one[at] <= 'Z')) ? (char)(one[at] + ('a' - 'A')) : one[at];
        const char right = ((other[at] >= 'A') && (other[at] <= 'Z')) ? (char)(other[at] + ('a' - 'A')) : other[at];
        if (left != right)
        {
            return 0;
        }
    }
    return 1;
}

// the number the table gives the source `file`, as __FILE__ gives it, its path from the top of the tree: 0 where it
// gives none or the path stands outside the top
static unsigned int who_source_number(const char *file)
{
    char plain[WHO_PATH_LONGEST];
    const size_t length = strlen(file);
    const char *const run_top = qry_member_top();
    if ((run_top == NULL) || (length >= WHO_PATH_LONGEST))
    {
        return 0u;
    }
    for (size_t at = 0u; at <= length; at += 1u)
    {
        plain[at] = (file[at] == '\\') ? '/' : file[at];
    }
    const size_t top = strlen(run_top);
    const char *name = plain;
    if ((length > top) && who_same_letters(plain, run_top, top) && (plain[top] == '/'))
    {
        name = plain + top + 1u;
    }
    else if ((plain[0] == '/') || ((plain[0] != '\0') && (plain[1] == ':')))
    {
        return 0u;
    }
    const unsigned char *table = NULL;
    unsigned long long size = 0ull;
    unsigned long long at = 0ull;
    unsigned int number = 0u;
    char source[WHO_PATH_LONGEST];
    if (!who_table(&table, &size))
    {
        return 0u;
    }
    while (who_row(table, size, &at, &number, source))
    {
        if ((number != 0u) && (strcmp(source, name) == 0))
        {
            return number;
        }
    }
    return 0u;
}

// the number of the program's own source: the last source the table gives whose name without its kind is the
// program's, a header after every other; 0 where none is. The table only grows, so a source given a new path stands
// after the row its old path keeps
static unsigned int who_program_number(void)
{
    char path[WHO_PATH_LONGEST];
    if (!qry_member_program(path, sizeof(path)))
    {
        return 0u;
    }
    const char *const slash = strrchr(path, '/');
    const char *const back = strrchr(path, '\\');
    const char *const base = (back > slash) ? (back + 1) : ((slash != NULL) ? (slash + 1) : path);
    const size_t stem = strcspn(base, ".");
    const unsigned char *table = NULL;
    unsigned long long size = 0ull;
    unsigned long long at = 0ull;
    unsigned int number = 0u;
    unsigned int found = 0u;
    unsigned int header = 0u;
    char source[WHO_PATH_LONGEST];
    if (!who_table(&table, &size))
    {
        return 0u;
    }
    while (who_row(table, size, &at, &number, source))
    {
        const char *const own = (strrchr(source, '/') != NULL) ? (strrchr(source, '/') + 1) : source;
        if ((number == 0u) || (strncmp(own, base, stem) != 0) || (own[stem] != '.'))
        {
            continue;
        }
        const char *const kind = own + stem;
        if ((strcmp(kind, ".h") != 0) && (strcmp(kind, ".cuh") != 0) && (strcmp(kind, ".hpp") != 0))
        {
            found = number;
        }
        else
        {
            header = number;
        }
    }
    return (found != 0u) ? found : header;
}

Who who_here(const char *file, unsigned int line)
{
    Who who = {0u, line, 0u, qry_member_clock()};
    if (!s_who_program_found)
    {
        s_who_program = who_program_number();
        s_who_program_found = 1;
    }
    who.program = s_who_program;
    int found = 0;
    for (unsigned int at = 0u; (at < WHO_ASKED) && !found; at += 1u)
    {
        found = (s_who_asked[at] == file);
        who.source = found ? s_who_asked_number[at] : who.source;
    }
    if (!found)
    {
        who.source = who_source_number(file);
        s_who_asked[s_who_asked_next] = file;
        s_who_asked_number[s_who_asked_next] = who.source;
        s_who_asked_next = (s_who_asked_next + 1u) % WHO_ASKED;
    }
    return who;
}

void who_words(Who who, unsigned long long *words)
{
    words[0] = (((unsigned long long)who.source & 0xffffull) << 48u) |
               (((unsigned long long)who.line & 0xffffull) << 32u) |
               (((unsigned long long)who.program & 0xffffull) << 16u);
    words[1] = who.time;
}

Who who_of_words(const unsigned long long *words)
{
    // each field is sixteen bits of the first word, which an unsigned int holds
    const Who who = {(unsigned int)((words[0] >> 48u) & 0xffffull), (unsigned int)((words[0] >> 32u) & 0xffffull),
                     (unsigned int)((words[0] >> 16u) & 0xffffull), words[1]};
    return who;
}

void who_code(Who who, char *code, size_t room)
{
    if (room <= WHO_CODE_LETTERS)
    {
        code[0] = (room != 0u) ? '\0' : code[0];
        return;
    }
    unsigned long long words[2];
    who_words(who, words);
    for (unsigned int at = 0u; at < 16u; at += 1u)
    {
        code[at] = s_who_digits[(words[0] >> (60u - (4u * at))) & 0xfull];
        code[16u + at] = s_who_digits[(words[1] >> (60u - (4u * at))) & 0xfull];
    }
    code[WHO_CODE_LETTERS] = '\0';
}

int who_read(const char *code, Who *who)
{
    unsigned long long words[2];
    if ((strlen(code) < WHO_CODE_LETTERS) || !who_hex(code, 16u, &words[0]) || !who_hex(code + 16, 16u, &words[1]))
    {
        return 0;
    }
    *who = who_of_words(words);
    return 1;
}

// the path the table gives the number `number` into `source`, which holds WHO_PATH_LONGEST: `-` where it gives none
static void who_source_path(unsigned int number, char *source)
{
    const unsigned char *table = NULL;
    unsigned long long size = 0ull;
    unsigned long long at = 0ull;
    unsigned int held = 0u;
    const int read = (number != 0u) && who_table(&table, &size);
    while (read && who_row(table, size, &at, &held, source))
    {
        if (held == number)
        {
            return;
        }
    }
    source[0] = '-';
    source[1] = '\0';
}

int who_named(Who who, char *text, size_t room)
{
    char source[WHO_PATH_LONGEST];
    who_source_path(who.source, source);
    size_t at = verba_textus.put(&(VerbaTextusCfg){.out = text, .cap = room, .at = 0u, .text = source});
    at = verba_littera.ch(&(VerbaLitteraCfg){.out = text, .cap = room, .at = at, .ch = ':'});
    at = verba_numerus.u64(&(VerbaNumerusCfg){.out = text, .cap = room, .at = at, .val = who.line});
    at = verba_littera.ch(&(VerbaLitteraCfg){.out = text, .cap = room, .at = at, .ch = ' '});
    who_source_path(who.program, source);
    at = verba_textus.put(&(VerbaTextusCfg){.out = text, .cap = room, .at = at, .text = source});
    at = verba_littera.ch(&(VerbaLitteraCfg){.out = text, .cap = room, .at = at, .ch = ' '});
    at = verba_numerus.u64(&(VerbaNumerusCfg){.out = text, .cap = room, .at = at, .val = who.time});
    return verba_finis.finish(&(VerbaFinisCfg){.out = text, .cap = room, .at = at}) != 0u;
}
