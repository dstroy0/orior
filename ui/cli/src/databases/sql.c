// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sql.c: a text's tokens as each database writes them, its statements, and the statement a place stands in
#include "sql.h"
#include "../text/text.h"

unsigned int cli_sql_dialect(const CliSqlCfg *args)
{
    if ((cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "mysql", .cap = CLI_POOL_BYTES})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "mariadb", .cap = CLI_POOL_BYTES})))
    {
        return CLI_SQL_MYSQL;
    }
    return (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "sqlite", .cap = CLI_POOL_BYTES}))
               ? CLI_SQL_SQLITE
               : CLI_SQL_POSTGRES;
}

// no token read yet, a type no token takes
#define CLI_SQL_UNREAD 0xffffffffu

// A statement as its tokens are read: where its first and last tokens start and end, whether one is read; how many of
// its first words are read, whether the first is CREATE and whether one of the first eight names a body; and the depth
// of BEGIN and CASE in a body and an END not yet known to close one of them
typedef struct
{
    unsigned long long first;
    int started;
    unsigned long long last;
    unsigned int words;
    int created;
    int bodied;
    long long depth;
    int ending;
} CliSqlStatement;

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
    const char *word_text;
    const char *const *words;
    unsigned int count;
    unsigned int dialect;
    CliSqlStatement *statement;
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
            *at += 2ULL;
        }
        else if (args->bytes[*at] == args->close)
        {
            *at += 1ULL;
            if ((*at < args->length) && (args->bytes[*at] == args->close))
            {
                *at += 1ULL;
            }
            else
            {
                return;
            }
        }
        else
        {
            *at += 1ULL;
        }
    }
    *at = args->length;
}

// The dollar quote's tag that starts at `at`, `$$` or `$name$`: its length, 0 where none starts there
EMBED_INLINE unsigned long long cli_dollar_tag(const SqlCtx *args)
{
    unsigned long long at = args->at_value;
    unsigned long long end = at + 1ULL;
    end = EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = (const char *)args->bytes,
                     .at = end, .size = args->length, .classes = CLI_TEXT_LETTERS | CLI_TEXT_DIGITS, .set = "_");
    const int digit_first = ((at + 1ULL) < args->length) && EMBED_CALL(cli_digit, SqlCtx, .byte = args->bytes[at + 1u]);
    return ((end < args->length) && (args->bytes[end] == '$') && !digit_first) ? (end - at + 1ULL) : 0ULL;
}

// `*at` moved past a block comment that starts there, those written in it with it where the dialect counts them
EMBED_INLINE void cli_block_comment(const SqlCtx *args)
{
    const unsigned char *const bytes = args->bytes;
    unsigned long long *at = args->at;
    long long depth = 0LL;
    while (*at < args->length)
    {
        const int follows = (*at + 1ULL) < args->length;
        const int opens = (bytes[*at] == '/') && follows && (bytes[*at + 1u] == '*');
        const int closes = !opens && (bytes[*at] == '*') && follows && (bytes[*at + 1u] == '/');
        if (!opens && !closes)
        {
            *at += 1ULL;
            continue;
        }
        if (opens)
        {
            depth += ((args->dialect == CLI_SQL_POSTGRES) || (depth == 0LL)) ? 1LL : 0LL;
        }
        else
        {
            depth -= 1LL;
        }
        *at += 2ULL;
        if (closes && (depth == 0LL))
        {
            return;
        }
    }
}

// a run of spaces or a comment that starts at `*at`, `*at` moved past it: its type, CLI_SQL_UNREAD where none starts
// there
EMBED_INLINE unsigned int cli_sql_quiet(const SqlCtx *args)
{
    const unsigned char *const bytes = args->bytes;
    unsigned long long *at = args->at;
    const unsigned char byte = bytes[*at];
    const unsigned char next = ((*at + 1ULL) < args->length) ? bytes[*at + 1u] : 0u;
    if (EMBED_CALL(cli_space, SqlCtx, .byte = byte))
    {
        while ((*at < args->length) && EMBED_CALL(cli_space, SqlCtx, .byte = bytes[*at]))
        {
            *at += 1ULL;
        }
        return CLI_SQL_SPACE;
    }
    if (((byte == '-') && (next == '-')) || ((byte == '#') && (args->dialect == CLI_SQL_MYSQL)))
    {
        *at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = (const char *)bytes,
                         .at = *at, .size = args->length, .set = "\n");
        return CLI_SQL_COMMENT;
    }
    if ((byte == '/') && (next == '*'))
    {
        EMBED_CALL(cli_block_comment, SqlCtx, .bytes = bytes, .length = args->length, .at = at,
                   .dialect = args->dialect);
        return CLI_SQL_COMMENT;
    }
    return CLI_SQL_UNREAD;
}

// `*at` moved past a dollar-quoted string that starts there, `$$` or `$name$` around it, to the end of the text where
// it does not close: 1, or 0 where none starts there
EMBED_INLINE int cli_dollar_quoted(const SqlCtx *args)
{
    const unsigned char *const bytes = args->bytes;
    unsigned long long *at = args->at;
    const unsigned long long start = *at;
    const unsigned long long tag =
        EMBED_CALL(cli_dollar_tag, SqlCtx, .bytes = bytes, .length = args->length, .at_value = start);
    if (tag == 0ULL)
    {
        return 0;
    }
    unsigned long long found = start + tag;
    while (((found + tag) <= args->length) &&
           (memor.cmp(&(MemoriaCfg){.src = bytes + found, .other = bytes + start, .bytes = tag}) != 0))
    {
        found += 1ULL;
    }
    *at = ((found + tag) <= args->length) ? (found + tag) : args->length;
    return 1;
}

// a quoted run that starts at `*at` as the dialect quotes, a string or a name, `*at` moved past it: its type,
// CLI_SQL_UNREAD where none starts there
EMBED_INLINE unsigned int cli_sql_quoted(const SqlCtx *args)
{
    const unsigned char *const bytes = args->bytes;
    unsigned long long *at = args->at;
    const unsigned int dialect = args->dialect;
    const unsigned char byte = bytes[*at];
    const unsigned char next = ((*at + 1ULL) < args->length) ? bytes[*at + 1u] : 0u;
    if ((byte == '\'') || (((byte == 'E') || (byte == 'e')) && (next == '\'') && (dialect == CLI_SQL_POSTGRES)))
    {
        *at += (byte == '\'') ? 1ULL : 2ULL;
        EMBED_CALL(cli_quote, SqlCtx, .bytes = bytes, .length = args->length, .at = at, .close = '\'',
                   .escapes = (dialect == CLI_SQL_MYSQL) || (byte != '\''));
        return CLI_SQL_TEXT;
    }
    if (byte == '"')
    {
        *at += 1ULL;
        EMBED_CALL(cli_quote, SqlCtx, .bytes = bytes, .length = args->length, .at = at, .close = '"',
                   .escapes = dialect == CLI_SQL_MYSQL);
        return (dialect == CLI_SQL_MYSQL) ? CLI_SQL_TEXT : CLI_SQL_NAME;
    }
    if ((byte == '`') && (dialect != CLI_SQL_POSTGRES))
    {
        *at += 1ULL;
        EMBED_CALL(cli_quote, SqlCtx, .bytes = bytes, .length = args->length, .at = at, .close = '`', .escapes = 0);
        return CLI_SQL_NAME;
    }
    if ((byte == '[') && (dialect == CLI_SQL_SQLITE))
    {
        *at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = (const char *)bytes,
                         .at = *at, .size = args->length, .set = "]");
        *at = ((*at + 1ULL) < args->length) ? (*at + 1ULL) : args->length;
        return CLI_SQL_NAME;
    }
    if ((byte == '$') && (dialect == CLI_SQL_POSTGRES) &&
        EMBED_CALL(cli_dollar_quoted, SqlCtx, .bytes = bytes, .length = args->length, .at = at))
    {
        return CLI_SQL_TEXT;
    }
    return CLI_SQL_UNREAD;
}

// a word, a number or one mark that starts at `*at` with `byte`, `*at` moved past it: its type
EMBED_INLINE unsigned int cli_sql_bare(const SqlCtx *args)
{
    unsigned long long *at = args->at;
    const char *const text = (const char *)args->bytes;
    if (EMBED_CALL(cli_alpha, SqlCtx, .byte = args->byte) || (args->byte == '_') || (args->byte >= 0x80u))
    {
        *at = EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = text, .at = *at,
                         .size = args->length, .classes = CLI_TEXT_LETTERS | CLI_TEXT_DIGITS | CLI_TEXT_END_ASCII,
                         .set = "_$");
        return CLI_SQL_WORD;
    }
    if (EMBED_CALL(cli_digit, SqlCtx, .byte = args->byte))
    {
        *at = EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = text, .at = *at,
                         .size = args->length, .classes = CLI_TEXT_LETTERS | CLI_TEXT_DIGITS, .set = ".");
        return CLI_SQL_NUMBER;
    }
    *at += 1ULL;
    return CLI_SQL_MARK;
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
    unsigned int type =
        EMBED_CALL(cli_sql_quiet, SqlCtx, .bytes = bytes, .length = args->length, .at = at, .dialect = args->dialect);
    if (type == CLI_SQL_UNREAD)
    {
        type = EMBED_CALL(cli_sql_quoted, SqlCtx, .bytes = bytes, .length = args->length, .at = at,
                          .dialect = args->dialect);
    }
    if (type == CLI_SQL_UNREAD)
    {
        type = EMBED_CALL(cli_sql_bare, SqlCtx, .bytes = bytes, .length = args->length, .at = at, .byte = byte);
    }
    args->token->type = type;
    args->token->start = start;
    args->token->end = *at;
    return 1;
}

// whether `length` bytes of `text` are `word` in either case
EMBED_INLINE int cli_word_is(const SqlCtx *args)
{
    return EMBED_CALL(cli_text_size_one.same_folded, CliTextSizeOneCfg, .size = args->length, .one = args->text,
                      .other_pointer = args->word_text);
}

EMBED_INLINE int cli_word_in(const SqlCtx *args)
{
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if (EMBED_CALL(cli_word_is, SqlCtx, .text = args->text, .length = args->length, .word_text = args->words[at]))
        {
            return 1;
        }
    }
    return 0;
}

// a word, `length` bytes of `text`, read in the statement: in a body, an END closing BEGIN or CASE where no block word
// follows it, and BEGIN and CASE opening one; and among the statement's first words, whether the first is CREATE and
// whether one of the first eight names a body. END IF, END LOOP, END WHILE and END REPEAT close blocks that are not
// counted
EMBED_INLINE void cli_sql_word_read(const SqlCtx *args)
{
    static const char *const s_blocks[4] = {"if", "loop", "while", "repeat"};
    static const char *const s_bodies[4] = {"trigger", "procedure", "function", "event"};
    CliSqlStatement *const statement = args->statement;
    if (statement->created && statement->bodied)
    {
        const int closed = statement->ending;
        const int block =
            EMBED_CALL(cli_word_in, SqlCtx, .text = args->text, .length = args->length, .words = s_blocks, .count = 4u);
        const int case_word =
            EMBED_CALL(cli_word_is, SqlCtx, .text = args->text, .length = args->length, .word_text = "case");
        statement->ending = 0;
        statement->depth -= (closed && !block) ? 1LL : 0LL;
        if (!(closed && (block || case_word)))
        {
            statement->depth +=
                (EMBED_CALL(cli_word_is, SqlCtx, .text = args->text, .length = args->length, .word_text = "begin") ||
                 case_word)
                    ? 1LL
                    : 0LL;
            statement->ending =
                EMBED_CALL(cli_word_is, SqlCtx, .text = args->text, .length = args->length, .word_text = "end");
        }
    }
    if (statement->words < 64u)
    {
        statement->created = (statement->words == 0u) ? EMBED_CALL(cli_word_is, SqlCtx, .text = args->text,
                                                                   .length = args->length, .word_text = "create")
                                                      : statement->created;
        statement->bodied = statement->bodied || ((statement->words < 8u) &&
                                                  EMBED_CALL(cli_word_in, SqlCtx, .text = args->text,
                                                             .length = args->length, .words = s_bodies, .count = 4u));
        statement->words += 1u;
    }
}

void cli_sql_statements(const CliSqlTextLengthCfg *args)
{
    const char *text = args->text;
    unsigned long long length = args->length;
    unsigned int dialect = args->dialect;
    CliSqlStatement statement = {0ULL, 0, 0ULL, 0u, 0, 0, 0LL, 0};
    unsigned long long at = 0ULL;
    CliSqlToken token;
    while (EMBED_CALL(cli_sql_text_length_dialect.token, CliSqlTextLengthDialectCfg, .text = text, .length = length,
                      .dialect = dialect, .at = &at, .token = &token))
    {
        const char *const word_text = text + token.start;
        const unsigned long long size = token.end - token.start;
        if ((token.type == CLI_SQL_SPACE) || (token.type == CLI_SQL_COMMENT))
        {
            continue;
        }
        const int semicolon = (token.type == CLI_SQL_MARK) && (size == 1u) && (word_text[0] == ';');
        if (semicolon)
        {
            statement.depth -= statement.ending ? 1LL : 0LL;
            statement.ending = 0;
        }
        else if (token.type == CLI_SQL_WORD)
        {
            EMBED_CALL(cli_sql_word_read, SqlCtx, .statement = &statement, .text = word_text, .length = size);
        }
        else if (statement.ending)
        {
            statement.ending = 0;
            statement.depth -= 1LL;
        }
        if (semicolon && (statement.depth <= 0LL))
        {
            if (statement.started)
            {
                const CliSqlSpan span = {statement.first, statement.last};
                EMBED_CALL(args->each, CliEachSpanCfg, .context = args->context, .span = span);
            }
            statement = (CliSqlStatement){0ULL, 0, 0ULL, 0u, 0, 0, 0LL, 0};
            continue;
        }
        statement.first = statement.started ? statement.first : token.start;
        statement.started = 1;
        statement.last = token.end;
    }
    if (statement.started)
    {
        const CliSqlSpan span = {statement.first, statement.last};
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
