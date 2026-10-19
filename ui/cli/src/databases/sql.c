// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sql.c: a text's tokens as each database writes them, its statements, and the statement a place stands in
#include "sql.h"

unsigned int cli_sql_dialect(const char *name)
{
    if ((cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "mysql", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "mariadb", .cap = CLI_CELLS_POOL})))
    {
        return CLI_SQL_MYSQL;
    }
    return (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "sqlite", .cap = CLI_CELLS_POOL})) ? CLI_SQL_SQLITE
                                                                                                  : CLI_SQL_POSTGRES;
}

// whether a byte is one of the spaces ASCII names: space, tab, line feed, form feed and carriage return
static int cli_space(unsigned char byte)
{
    return (byte == ' ') || (byte == '\t') || (byte == '\n') || (byte == 0x0cu) || (byte == '\r');
}

static int cli_alpha(unsigned char byte)
{
    return ((byte >= 'a') && (byte <= 'z')) || ((byte >= 'A') && (byte <= 'Z'));
}

static int cli_digit(unsigned char byte)
{
    return (byte >= '0') && (byte <= '9');
}

// `*at` moved past a quoted run that began just before it and ends at `close`, a doubled `close` standing for itself,
// and a backslash escaping the byte after it where `escapes` is 1
static void cli_quoted(const unsigned char *bytes, unsigned long long length, unsigned long long *at,
                       unsigned char close, int escapes)
{
    while (*at < length)
    {
        if (escapes && (bytes[*at] == '\\'))
        {
            *at += 2ull;
        }
        else if (bytes[*at] == close)
        {
            *at += 1ull;
            if ((*at < length) && (bytes[*at] == close))
            {
                *at += 1ull;
            }
            else
            {
                return;
            }
        }
        else
        {
            *at += 1ull;
        }
    }
    *at = length;
}

// The dollar quote's tag that starts at `at`, `$$` or `$name$`: its length, 0 where none starts there
static unsigned long long cli_dollar_tag(const unsigned char *bytes, unsigned long long length, unsigned long long at)
{
    unsigned long long end = at + 1ull;
    while ((end < length) && (cli_alpha(bytes[end]) || cli_digit(bytes[end]) || (bytes[end] == '_')))
    {
        end += 1ull;
    }
    const int digit_first = ((at + 1ull) < length) && cli_digit(bytes[at + 1u]);
    return ((end < length) && (bytes[end] == '$') && !digit_first) ? (end - at + 1ull) : 0ull;
}

int cli_sql_token(const char *text, unsigned long long length, unsigned int dialect, unsigned long long *at,
                  CliSqlToken *token)
{
    const unsigned char *const bytes = (const unsigned char *)text;
    if (*at >= length)
    {
        return 0;
    }
    const unsigned long long start = *at;
    const unsigned char byte = bytes[start];
    const int next_is = (start + 1ull) < length;
    const unsigned char next = next_is ? bytes[start + 1u] : 0u;
    unsigned int kind = CLI_SQL_MARK;
    if (cli_space(byte))
    {
        while ((*at < length) && cli_space(bytes[*at]))
        {
            *at += 1ull;
        }
        kind = CLI_SQL_SPACE;
    }
    else if (((byte == '-') && next_is && (next == '-')) || ((byte == '#') && (dialect == CLI_SQL_MYSQL)))
    {
        while ((*at < length) && (bytes[*at] != '\n'))
        {
            *at += 1ull;
        }
        kind = CLI_SQL_COMMENT;
    }
    else if ((byte == '/') && next_is && (next == '*'))
    {
        long long depth = 0ll;
        while (*at < length)
        {
            if ((bytes[*at] == '/') && ((*at + 1ull) < length) && (bytes[*at + 1u] == '*'))
            {
                depth += ((dialect == CLI_SQL_POSTGRES) || (depth == 0ll)) ? 1ll : 0ll;
                *at += 2ull;
            }
            else if ((bytes[*at] == '*') && ((*at + 1ull) < length) && (bytes[*at + 1u] == '/'))
            {
                depth -= 1ll;
                *at += 2ull;
                if (depth == 0ll)
                {
                    break;
                }
            }
            else
            {
                *at += 1ull;
            }
        }
        kind = CLI_SQL_COMMENT;
    }
    else if ((byte == '\'') ||
             (((byte == 'E') || (byte == 'e')) && next_is && (next == '\'') && (dialect == CLI_SQL_POSTGRES)))
    {
        const int escapes = (dialect == CLI_SQL_MYSQL) || (byte != '\'');
        *at += (byte == '\'') ? 1ull : 2ull;
        cli_quoted(bytes, length, at, '\'', escapes);
        kind = CLI_SQL_TEXT;
    }
    else if (byte == '"')
    {
        *at += 1ull;
        cli_quoted(bytes, length, at, '"', dialect == CLI_SQL_MYSQL);
        kind = (dialect == CLI_SQL_MYSQL) ? CLI_SQL_TEXT : CLI_SQL_NAME;
    }
    else if ((byte == '`') && (dialect != CLI_SQL_POSTGRES))
    {
        *at += 1ull;
        cli_quoted(bytes, length, at, '`', 0);
        kind = CLI_SQL_NAME;
    }
    else if ((byte == '[') && (dialect == CLI_SQL_SQLITE))
    {
        while ((*at < length) && (bytes[*at] != ']'))
        {
            *at += 1ull;
        }
        *at = ((*at + 1ull) < length) ? (*at + 1ull) : length;
        kind = CLI_SQL_NAME;
    }
    else if ((byte == '$') && (dialect == CLI_SQL_POSTGRES) && (cli_dollar_tag(bytes, length, start) != 0ull))
    {
        const unsigned long long tag = cli_dollar_tag(bytes, length, start);
        *at += tag;
        unsigned long long found = *at;
        while (((found + tag) <= length) &&
               (memor.cmp(&(MemoriaCfg){.src = bytes + found, .other = bytes + start, .bytes = tag}) != 0))
        {
            found += 1ull;
        }
        *at = ((found + tag) <= length) ? (found + tag) : length;
        kind = CLI_SQL_TEXT;
    }
    else if (cli_alpha(byte) || (byte == '_') || (byte >= 0x80u))
    {
        while ((*at < length) && (cli_alpha(bytes[*at]) || cli_digit(bytes[*at]) || (bytes[*at] == '_') ||
                                  (bytes[*at] == '$') || (bytes[*at] >= 0x80u)))
        {
            *at += 1ull;
        }
        kind = CLI_SQL_WORD;
    }
    else if (cli_digit(byte))
    {
        while ((*at < length) && (cli_alpha(bytes[*at]) || cli_digit(bytes[*at]) || (bytes[*at] == '.')))
        {
            *at += 1ull;
        }
        kind = CLI_SQL_NUMBER;
    }
    else
    {
        *at += 1ull;
    }
    token->kind = kind;
    token->start = start;
    token->end = *at;
    return 1;
}

// whether `length` bytes of `text` are `word` in either case
static int cli_word_is(const char *text, unsigned long long length, const char *word)
{
    if (cellul.len(&(CatenaFinitaCfg){.src = word, .cap = CLI_CELLS_POOL}) != length)
    {
        return 0;
    }
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        const char letter = ((text[at] >= 'A') && (text[at] <= 'Z')) ? (char)(text[at] + 32) : text[at];
        if (letter != word[at])
        {
            return 0;
        }
    }
    return 1;
}

static int cli_word_in(const char *text, unsigned long long length, const char *const *words, unsigned int count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (cli_word_is(text, length, words[at]))
        {
            return 1;
        }
    }
    return 0;
}

void cli_sql_statements(const char *text, unsigned long long length, unsigned int dialect,
                        void (*each)(void *context, CliSqlSpan span), void *context)
{
    static const char *const s_blocks[4] = {"if", "loop", "while", "repeat"};
    static const char *const s_bodies[4] = {"trigger", "procedure", "function", "event"};
    unsigned long long first = 0ull;
    int started = 0;
    unsigned long long last = 0ull;
    // the first words of the statement, as far as a body is known by them: whether the first is CREATE, and whether
    // one of the first eight names a body
    unsigned int words = 0u;
    int created = 0;
    int bodied = 0;
    // The depth of BEGIN and CASE in a body, and an END not yet known to close one of them: END IF, END LOOP, END WHILE
    // and END REPEAT close blocks that are not counted
    long long depth = 0ll;
    int ending = 0;
    unsigned long long at = 0ull;
    CliSqlToken token;
    while (cli_sql_token(text, length, dialect, &at, &token))
    {
        const char *const word = text + token.start;
        const unsigned long long size = token.end - token.start;
        if ((token.kind == CLI_SQL_SPACE) || (token.kind == CLI_SQL_COMMENT))
        {
            continue;
        }
        if ((token.kind == CLI_SQL_MARK) && (size == 1u) && (word[0] == ';'))
        {
            depth -= ending ? 1ll : 0ll;
            ending = 0;
            if (depth <= 0ll)
            {
                if (started)
                {
                    const CliSqlSpan span = {first, last};
                    each(context, span);
                    started = 0;
                }
                words = 0u;
                created = 0;
                bodied = 0;
                depth = 0ll;
                continue;
            }
        }
        else if (token.kind == CLI_SQL_WORD)
        {
            if (created && bodied)
            {
                const int closed = ending;
                ending = 0;
                if (closed && !cli_word_in(word, size, s_blocks, 4u))
                {
                    depth -= 1ll;
                }
                if (!(closed && (cli_word_in(word, size, s_blocks, 4u) || cli_word_is(word, size, "case"))))
                {
                    depth += (cli_word_is(word, size, "begin") || cli_word_is(word, size, "case")) ? 1ll : 0ll;
                    ending = cli_word_is(word, size, "end");
                }
            }
            if (words < 64u)
            {
                created = (words == 0u) ? cli_word_is(word, size, "create") : created;
                bodied = bodied || ((words < 8u) && cli_word_in(word, size, s_bodies, 4u));
                words += 1u;
            }
        }
        else if (ending)
        {
            ending = 0;
            depth -= 1ll;
        }
        first = started ? first : token.start;
        started = 1;
        last = token.end;
    }
    if (started)
    {
        const CliSqlSpan span = {first, last};
        each(context, span);
    }
}

// The statement a place stands in: the first that holds it, and else the last that ends before it
typedef struct
{
    unsigned long long at;
    CliSqlSpan holding;
    int held;
    CliSqlSpan before;
    int found;
} CliSqlFinding;

static void cli_finding_each(void *context, CliSqlSpan span)
{
    CliSqlFinding *const finding = (CliSqlFinding *)context;
    if (!finding->held && (finding->at >= span.start) && (finding->at <= span.end))
    {
        finding->holding = span;
        finding->held = 1;
    }
    if (span.end <= finding->at)
    {
        finding->before = span;
        finding->found = 1;
    }
}

int cli_sql_statement_at(const char *text, unsigned long long length, unsigned long long at, unsigned int dialect,
                         CliSqlSpan *span)
{
    CliSqlFinding finding;
    memor.set(&(MemoriaCfg){.dst = &finding, .val = 0, .bytes = sizeof(finding)});
    finding.at = at;
    cli_sql_statements(text, length, dialect, cli_finding_each, &finding);
    if (finding.held)
    {
        *span = finding.holding;
        return 1;
    }
    if (!finding.found)
    {
        return 0;
    }
    // what stands between the statement and the place: its semicolon, then nothing but spaces and a comment on its line
    const char *from = text + finding.before.end;
    const char *const to = text + ((at < length) ? at : length);
    from += ((from < to) && (from[0] == ';')) ? 1 : 0;
    for (const char *letter = from; letter < to; letter += 1)
    {
        if (letter[0] == '\n')
        {
            return 0;
        }
    }
    while ((from < to) && cli_space((unsigned char)from[0]))
    {
        from += 1;
    }
    if ((from == to) || ((to - from) >= 2 && (from[0] == '-') && (from[1] == '-')) || (from[0] == '#'))
    {
        *span = finding.before;
        return 1;
    }
    return 0;
}
