// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// who.c: the who of a write, its source and program numbered by the who table as the run stores it and its time on
// the run's one clock, its two words, its code and its name
#include "who.h"
#include "../platform/platform.h"
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

#include "line_read.h"

#include "../file_defs/qry/writer/qry_member.h"
#include "verba_scribo/verba_scribo.h"

// the order of the texts `left` and `right`, each read to its terminator or to `cap`: below 0 where `left`
// comes first, 0 where they are the same, above 0 where `right` does
static inline int who_text_order(const char *left, const char *right, size_t cap)
{
    const size_t left_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = left, .cap = cap);
    const size_t right_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = right, .cap = cap);
    const size_t shorter = (left_length < right_length) ? left_length : right_length;
    const size_t reach = (shorter < cap) ? (shorter + 1u) : cap;
    const size_t differs =
        EMBED_CALL(cellul.diff, CatenaFinitaCfg, .src = left, .cap = reach, .other = right, .ci = EMBED_FALSE);
    if (differs >= reach)
    {
        return 0;
    }
    // a byte of a text read as unsigned, and its order as a signed count
    return (int)(unsigned char)left[differs] - (int)(unsigned char)right[differs];
}

// the sources whose numbers were requested, each looked up in the table once
#define WHO_LOOKUPS 64u

// the longest path a who names
#define WHO_PATH_MAX_LEN 1024u

static const char *s_who_lookup[WHO_LOOKUPS];
static unsigned int s_who_lookup_number[WHO_LOOKUPS];
static unsigned int s_who_lookup_next;
static unsigned int s_who_program;
static int s_who_program_found;

static const char s_who_digits[] = "0123456789abcdef";

// the value of the hex digit `character`: 0 to 15, or 16 where it is none
static unsigned int who_hex_digit(char character)
{
    // Explicit casts read the byte as its code point, and the folded letter as a count from 'a'
    const uint8_t code = (uint8_t)character;
    if (!EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_HEX, .byte = code))
    {
        return 16u;
    }
    return EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_NUM, .byte = code)
               ? (unsigned int)(code - '0')
               : (unsigned int)(EMBED_CALL(ascii.lower, AsciiCfg, .byte = code) - 'a') + 10u;
}

// the `characters` hex digits at `text` read into `value`: 1, or 0 where one is no hex digit
static int who_hex(const char *text, unsigned int characters, unsigned long long *value)
{
    *value = 0ULL;
    for (unsigned int at = 0u; at < characters; at += 1u)
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

// the who table as the run stores it: 1, or 0 where there is no run or no table
static int who_table(const unsigned char **table, unsigned long long *size)
{
    const char *const run_top = qry_member_top();
    char path[WHO_PATH_MAX_LEN + sizeof(WHO_TABLE) + 1u];
    const size_t top = (run_top != NULL)
                           ? EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = run_top, .cap = PLATFORM_TEXT_TO_TERMINATOR)
                           : 0u;
    if ((top == 0u) || (top >= WHO_PATH_MAX_LEN))
    {
        return 0;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = path, .src = run_top, .bytes = top);
    path[top] = '/';
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = path + top + 1u, .src = WHO_TABLE, .bytes = sizeof(WHO_TABLE));
    return qry_member_file_blob(path, table, size);
}

// the next row of the table at `*at` into `number` and `source`, which has room for WHO_PATH_MAX_LEN: 1, or 0 where no
// row is left; a line that is no row gives number 0
static int who_row(const unsigned char *table, unsigned long long size, unsigned long long *at, unsigned int *number,
                   char *source)
{
    char line[WHO_PATH_MAX_LEN + 16u];
    if (!line_read(table, size, at, line, sizeof(line)))
    {
        return 0;
    }
    unsigned long long value = 0ULL;
    const char *const tab =
        EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '\t');
    *number = 0u;
    source[0] = '\0';
    if ((line[0] != '0') || (line[1] != 'x') || (tab == NULL) || ((tab - line) != 6) || !who_hex(line + 2, 4u, &value))
    {
        return 1;
    }
    const size_t length =
        EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = tab + 1, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "\r");
    if (length >= WHO_PATH_MAX_LEN)
    {
        return 1;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = source, .src = tab + 1, .bytes = length);
    source[length] = '\0';
    // four hex digits are below 0x10000, which an unsigned int represents
    *number = (unsigned int)value;
    return 1;
}

// 1 where the `length` characters at `one` and `other` are the same, a character's case aside
static int who_same_characters(const char *one, const char *other, size_t length)
{
    return (EMBED_CALL(cellul.diff, CatenaFinitaCfg, .src = one, .cap = length, .other = other, .ci = EMBED_TRUE) ==
            length)
               ? 1
               : 0;
}

// the number the table gives the source `file`, as __FILE__ gives it, its path from the top of the tree: 0 where it
// gives none or the path is outside the top
static unsigned int who_source_number(const char *file)
{
    char plain[WHO_PATH_MAX_LEN];
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = file, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    const char *const run_top = qry_member_top();
    if ((run_top == NULL) || (length >= WHO_PATH_MAX_LEN))
    {
        return 0u;
    }
    for (size_t at = 0u; at <= length; at += 1u)
    {
        plain[at] = (file[at] == '\\') ? '/' : file[at];
    }
    const size_t top = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = run_top, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    const char *name = plain;
    if ((length > top) && who_same_characters(plain, run_top, top) && (plain[top] == '/'))
    {
        name = plain + top + 1u;
    }
    else if ((plain[0] == '/') || ((plain[0] != '\0') && (plain[1] == ':')))
    {
        return 0u;
    }
    const unsigned char *table = NULL;
    unsigned long long size = 0ULL;
    unsigned long long at = 0ULL;
    unsigned int number = 0u;
    char source[WHO_PATH_MAX_LEN];
    if (!who_table(&table, &size))
    {
        return 0u;
    }
    while (who_row(table, size, &at, &number, source))
    {
        if ((number != 0u) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = source, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                          .other = name, .ci = EMBED_FALSE)))
        {
            return number;
        }
    }
    return 0u;
}

// the number of the program's own source: the last source the table gives whose name without its type is the
// program's, a header after every other; 0 where none is. The table only grows: a source given a new path is
// after the row its old path retains
static unsigned int who_program_number(void)
{
    char path[WHO_PATH_MAX_LEN];
    if (!qry_member_program(path, sizeof(path)))
    {
        return 0u;
    }
    const char *const slash =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = path, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '/');
    const char *const back =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = path, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '\\');
    const char *const base = (back > slash) ? (back + 1) : ((slash != NULL) ? (slash + 1) : path);
    const size_t stem =
        EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = base, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = ".");
    const unsigned char *table = NULL;
    unsigned long long size = 0ULL;
    unsigned long long at = 0ULL;
    unsigned int number = 0u;
    unsigned int found = 0u;
    unsigned int header = 0u;
    char source[WHO_PATH_MAX_LEN];
    if (!who_table(&table, &size))
    {
        return 0u;
    }
    while (who_row(table, size, &at, &number, source))
    {
        const char *const own = (EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = source,
                                            .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '/') != NULL)
                                    ? (EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = source,
                                                  .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '/') +
                                       1)
                                    : source;
        if ((number == 0u) || (who_text_order(own, base, (size_t)(stem)) != 0) || (own[stem] != '.'))
        {
            continue;
        }
        const char *const type = own + stem;
        if ((!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = type, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = ".h",
                         .ci = EMBED_FALSE)) &&
            (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = type, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = ".cuh",
                         .ci = EMBED_FALSE)) &&
            (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = type, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = ".hpp",
                         .ci = EMBED_FALSE)))
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
    for (unsigned int at = 0u; (at < WHO_LOOKUPS) && !found; at += 1u)
    {
        found = (s_who_lookup[at] == file);
        who.source = found ? s_who_lookup_number[at] : who.source;
    }
    if (!found)
    {
        who.source = who_source_number(file);
        s_who_lookup[s_who_lookup_next] = file;
        s_who_lookup_number[s_who_lookup_next] = who.source;
        s_who_lookup_next = (s_who_lookup_next + 1u) % WHO_LOOKUPS;
    }
    return who;
}

void who_words(Who who, unsigned long long *words)
{
    words[0] = (((unsigned long long)who.source & 0xffffULL) << 48u) |
               (((unsigned long long)who.line & 0xffffULL) << 32u) |
               (((unsigned long long)who.program & 0xffffULL) << 16u);
    words[1] = who.time;
}

Who who_of_words(const unsigned long long *words)
{
    // each field is sixteen bits of the first word, which an unsigned int represents
    const Who who = {(unsigned int)((words[0] >> 48u) & 0xffffULL), (unsigned int)((words[0] >> 32u) & 0xffffULL),
                     (unsigned int)((words[0] >> 16u) & 0xffffULL), words[1]};
    return who;
}

void who_code(Who who, char *code, size_t room)
{
    if (room <= WHO_CODE_CHARACTERS)
    {
        code[0] = (room != 0u) ? '\0' : code[0];
        return;
    }
    unsigned long long words[2];
    who_words(who, words);
    for (unsigned int at = 0u; at < 16u; at += 1u)
    {
        code[at] = s_who_digits[(words[0] >> (60u - (4u * at))) & 0xfULL];
        code[16u + at] = s_who_digits[(words[1] >> (60u - (4u * at))) & 0xfULL];
    }
    code[WHO_CODE_CHARACTERS] = '\0';
}

int who_read(const char *code, Who *who)
{
    unsigned long long words[2];
    if ((EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = code, .cap = PLATFORM_TEXT_TO_TERMINATOR) <
         WHO_CODE_CHARACTERS) ||
        !who_hex(code, 16u, &words[0]) || !who_hex(code + 16, 16u, &words[1]))
    {
        return 0;
    }
    *who = who_of_words(words);
    return 1;
}

// the path the table gives the number `number` into `source`, which has room for WHO_PATH_MAX_LEN: `-` where it gives
// none
static void who_source_path(unsigned int number, char *source)
{
    const unsigned char *table = NULL;
    unsigned long long size = 0ULL;
    unsigned long long at = 0ULL;
    unsigned int row_number = 0u;
    const int read = (number != 0u) && who_table(&table, &size);
    while (read && who_row(table, size, &at, &row_number, source))
    {
        if (row_number == number)
        {
            return;
        }
    }
    source[0] = '-';
    source[1] = '\0';
}

int who_named(Who who, char *text, size_t room)
{
    char source[WHO_PATH_MAX_LEN];
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
