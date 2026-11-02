// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sql.c: a text's tokens as each database writes them, its statements, and the statement a place stands in
#include "sql.h"
#include "../text/text.h"

unsigned int cli_sql_dialect(const CliSqlCfg *args)
{
    if ((cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "mysql", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "mariadb", .cap = CLI_CELLS_POOL})))
    {
        return CLI_SQL_MYSQL;
    }
    return (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "sqlite", .cap = CLI_CELLS_POOL}))
               ? CLI_SQL_SQLITE
               : CLI_SQL_POSTGRES;
}

typedef struct
{
    unsigned char byte;
    const unsigned char *bytes;
    unsigned long long length;
    unsigned long long *at;
    unsigned char close;
    int escapes;
    unsigned long long at_value;
    const char *text;
    const char *word;
    const char *const *words;
    unsigned int count;
} SqlCtx;
// whether a byte is one of the spaces ASCII names: space, tab, line feed, form feed and carriage return
EMBED_INLINE int cli_space(const SqlCtx *args)
{
    return (args->byte == ' ') || (args->byte == '\t') || (args->byte == '\n') || (args->byte == 0x0cu) ||
           (args->byte == '\r');
}

EMBED_INLINE int cli_alpha(const SqlCtx *args)
{
    return ((args->byte >= 'a') && (args->byte <= 'z')) || ((args->byte >= 'A') && (args->byte <= 'Z'));
}

EMBED_INLINE int cli_digit(const SqlCtx *args)
{
    return (args->byte >= '0') && (args->byte <= '9');
}

// `*at` moved past a quoted run that began just before it and ends at `close`, a doubled `close` standing for itself,
// and a backslash escaping the byte after it where `escapes` is 1
EMBED_INLINE void cli_quote(const SqlCtx *args)
{
    unsigned long long *at = args->at;
    while (*at < args->length)
    {
        if (args->escapes && (args->bytes[*at] == '\\'))
        {
            *at += 2ull;
        }
        else if (args->bytes[*at] == args->close)
        {
            *at += 1ull;
            if ((*at < args->length) && (args->bytes[*at] == args->close))
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
    *at = args->length;
}

// The dollar quote's tag that starts at `at`, `$$` or `$name$`: its length, 0 where none starts there
EMBED_INLINE unsigned long long cli_dollar_tag(const SqlCtx *args)
{
    unsigned long long at = args->at_value;
    unsigned long long end = at + 1ull;
    end = EMBED_CALL(cli_text_at_size_classes.past_class, CliTextAtSizeClassesCfg, .text = (const char *)args->bytes,
                     .at = end, .size = args->length, .classes = CLI_TEXT_LETTERS | CLI_TEXT_DIGITS, .set = "_");
    const int digit_first = ((at + 1ull) < args->length) && EMBED_CALL(cli_digit, SqlCtx, .byte = args->bytes[at + 1u]);
    return ((end < args->length) && (args->bytes[end] == '$') && !digit_first) ? (end - at + 1ull) : 0ull;
}

int cli_sql_token(const CliSqlTextLengthDialectCfg *args)
{
    unsigned long long *at = args->at;
    const unsigned char *const bytes = (const unsigned char *)args->text;
    if (*at >= args->length)
    {
        return 0;
    }
    const unsigned long long start = *at;
    const unsigned char byte = bytes[start];
    const int next_is = (start + 1ull) < args->length;
    const unsigned char next = next_is ? bytes[start + 1u] : 0u;
    unsigned int kind = CLI_SQL_MARK;
    if (EMBED_CALL(cli_space, SqlCtx, .byte = byte))
    {
        while ((*at < args->length) && EMBED_CALL(cli_space, SqlCtx, .byte = bytes[*at]))
        {
            *at += 1ull;
        }
        kind = CLI_SQL_SPACE;
    }
    else if (((byte == '-') && next_is && (next == '-')) || ((byte == '#') && (args->dialect == CLI_SQL_MYSQL)))
    {
        *at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = (const char *)bytes,
                         .at = *at, .size = args->length, .set = "\n");
        kind = CLI_SQL_COMMENT;
    }
    else if ((byte == '/') && next_is && (next == '*'))
    {
        long long depth = 0ll;
        while (*at < args->length)
        {
            if ((bytes[*at] == '/') && ((*at + 1ull) < args->length) && (bytes[*at + 1u] == '*'))
            {
                depth += ((args->dialect == CLI_SQL_POSTGRES) || (depth == 0ll)) ? 1ll : 0ll;
                *at += 2ull;
            }
            else if ((bytes[*at] == '*') && ((*at + 1ull) < args->length) && (bytes[*at + 1u] == '/'))
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
             (((byte == 'E') || (byte == 'e')) && next_is && (next == '\'') && (args->dialect == CLI_SQL_POSTGRES)))
    {
        const int escapes = (args->dialect == CLI_SQL_MYSQL) || (byte != '\'');
        *at += (byte == '\'') ? 1ull : 2ull;
        EMBED_CALL(cli_quote, SqlCtx, .bytes = bytes, .length = args->length, .at = at, .close = '\'',
                   .escapes = escapes);
        kind = CLI_SQL_TEXT;
    }
    else if (byte == '"')
    {
        *at += 1ull;
        EMBED_CALL(cli_quote, SqlCtx, .bytes = bytes, .length = args->length, .at = at, .close = '"',
                   .escapes = args->dialect == CLI_SQL_MYSQL);
        kind = (args->dialect == CLI_SQL_MYSQL) ? CLI_SQL_TEXT : CLI_SQL_NAME;
    }
    else if ((byte == '`') && (args->dialect != CLI_SQL_POSTGRES))
    {
        *at += 1ull;
        EMBED_CALL(cli_quote, SqlCtx, .bytes = bytes, .length = args->length, .at = at, .close = '`', .escapes = 0);
        kind = CLI_SQL_NAME;
    }
    else if ((byte == '[') && (args->dialect == CLI_SQL_SQLITE))
    {
        *at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = (const char *)bytes,
                         .at = *at, .size = args->length, .set = "]");
        *at = ((*at + 1ull) < args->length) ? (*at + 1ull) : args->length;
        kind = CLI_SQL_NAME;
    }
    else if ((byte == '$') && (args->dialect == CLI_SQL_POSTGRES) &&
             (EMBED_CALL(cli_dollar_tag, SqlCtx, .bytes = bytes, .length = args->length, .at_value = start) != 0ull))
    {
        const unsigned long long tag =
            EMBED_CALL(cli_dollar_tag, SqlCtx, .bytes = bytes, .length = args->length, .at_value = start);
        *at += tag;
        unsigned long long found = *at;
        while (((found + tag) <= args->length) &&
               (memor.cmp(&(MemoriaCfg){.src = bytes + found, .other = bytes + start, .bytes = tag}) != 0))
        {
            found += 1ull;
        }
        *at = ((found + tag) <= args->length) ? (found + tag) : args->length;
        kind = CLI_SQL_TEXT;
    }
    else if (EMBED_CALL(cli_alpha, SqlCtx, .byte = byte) || (byte == '_') || (byte >= 0x80u))
    {
        *at = EMBED_CALL(cli_text_at_size_classes.past_class, CliTextAtSizeClassesCfg, .text = (const char *)bytes,
                         .at = *at, .size = args->length,
                         .classes = CLI_TEXT_LETTERS | CLI_TEXT_DIGITS | CLI_TEXT_PAST_ASCII, .set = "_$");
        kind = CLI_SQL_WORD;
    }
    else if (EMBED_CALL(cli_digit, SqlCtx, .byte = byte))
    {
        *at = EMBED_CALL(cli_text_at_size_classes.past_class, CliTextAtSizeClassesCfg, .text = (const char *)bytes,
                         .at = *at, .size = args->length, .classes = CLI_TEXT_LETTERS | CLI_TEXT_DIGITS, .set = ".");
        kind = CLI_SQL_NUMBER;
    }
    else
    {
        *at += 1ull;
    }
    args->token->kind = kind;
    args->token->start = start;
    args->token->end = *at;
    return 1;
}

// whether `length` bytes of `text` are `word` in either case
EMBED_INLINE int cli_word_is(const SqlCtx *args)
{
    if (cellul.len(&(CatenaFinitaCfg){.src = args->word, .cap = CLI_CELLS_POOL}) != args->length)
    {
        return 0;
    }
    for (unsigned long long at = 0ull; at < args->length; at += 1ull)
    {
        const char letter =
            ((args->text[at] >= 'A') && (args->text[at] <= 'Z')) ? (char)(args->text[at] + 32) : args->text[at];
        if (letter != args->word[at])
        {
            return 0;
        }
    }
    return 1;
}

EMBED_INLINE int cli_word_in(const SqlCtx *args)
{
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if (EMBED_CALL(cli_word_is, SqlCtx, .text = args->text, .length = args->length, .word = args->words[at]))
        {
            return 1;
        }
    }
    return 0;
}

void cli_sql_statements(const CliSqlTextLengthCfg *args)
{
    const char *text = args->text;
    unsigned long long length = args->length;
    unsigned int dialect = args->dialect;
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
    while (EMBED_CALL(cli_sql_text_length_dialect.token, CliSqlTextLengthDialectCfg, .text = text, .length = length,
                      .dialect = dialect, .at = &at, .token = &token))
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
                    EMBED_CALL(args->each, CliEachSpanCfg, .context = args->context, .span = span);
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
                if (closed &&
                    !EMBED_CALL(cli_word_in, SqlCtx, .text = word, .length = size, .words = s_blocks, .count = 4u))
                {
                    depth -= 1ll;
                }
                if (!(closed &&
                      (EMBED_CALL(cli_word_in, SqlCtx, .text = word, .length = size, .words = s_blocks, .count = 4u) ||
                       EMBED_CALL(cli_word_is, SqlCtx, .text = word, .length = size, .word = "case"))))
                {
                    depth += (EMBED_CALL(cli_word_is, SqlCtx, .text = word, .length = size, .word = "begin") ||
                              EMBED_CALL(cli_word_is, SqlCtx, .text = word, .length = size, .word = "case"))
                                 ? 1ll
                                 : 0ll;
                    ending = EMBED_CALL(cli_word_is, SqlCtx, .text = word, .length = size, .word = "end");
                }
            }
            if (words < 64u)
            {
                created = (words == 0u)
                              ? EMBED_CALL(cli_word_is, SqlCtx, .text = word, .length = size, .word = "create")
                              : created;
                bodied = bodied || ((words < 8u) && EMBED_CALL(cli_word_in, SqlCtx, .text = word, .length = size,
                                                               .words = s_bodies, .count = 4u));
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
        EMBED_CALL(args->each, CliEachSpanCfg, .context = args->context, .span = span);
    }
}

// The statement a place stands in: the first that contains it, and else the last that ends before it
typedef struct
{
    unsigned long long at;
    CliSqlSpan enclosing;
    int has_enclosing;
    CliSqlSpan before;
    int has_before;
} CliSqlFinding;

static void cli_finding_each(const CliEachSpanCfg *args)
{
    CliSqlFinding *const finding = (CliSqlFinding *)args->context;
    if (!finding->has_enclosing && (finding->at >= args->span.start) && (finding->at <= args->span.end))
    {
        finding->enclosing = args->span;
        finding->has_enclosing = 1;
    }
    if (args->span.end <= finding->at)
    {
        finding->before = args->span;
        finding->has_before = 1;
    }
}

int cli_sql_statement_at(const CliSqlTextCfg *args)
{
    unsigned long long at = args->at_value;
    CliSqlSpan *span = args->span;
    CliSqlFinding finding;
    memor.set(&(MemoriaCfg){.dst = &finding, .val = 0, .bytes = sizeof(finding)});
    finding.at = at;
    EMBED_CALL(cli_sql_text_length.statements, CliSqlTextLengthCfg, .text = args->text, .length = args->length,
               .dialect = args->dialect, .each = cli_finding_each, .context = &finding);
    if (finding.has_enclosing)
    {
        *span = finding.enclosing;
        return 1;
    }
    if (!finding.has_before)
    {
        return 0;
    }
    // what stands between the statement and the place: its semicolon, then nothing but spaces and a comment on its line
    const char *from = args->text + finding.before.end;
    const char *const to = args->text + ((at < args->length) ? at : args->length);
    from += ((from < to) && (from[0] == ';')) ? 1 : 0;
    for (const char *letter = from; letter < to; letter += 1)
    {
        if (letter[0] == '\n')
        {
            return 0;
        }
    }
    while ((from < to) && EMBED_CALL(cli_space, SqlCtx, .byte = (unsigned char)from[0]))
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
