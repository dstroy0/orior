// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sql.c: a text's tokens off the leaves the SQL grammar plugin's walk saves, begun in the state of the dialect the
// database writes; its statements; and the statement a place is in
#include "sql.h"
#include "../code/symbols_internal.h"
#include "../command_line/pool.h"
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

// the state of the SQL grammar each dialect's walk begins in, by CLI_SQL_ dialect
static const char *const s_states[3] = {"postgres", "mysql", "sqlite"};

// a type no token takes: a leaf that is no token
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
    const CliLeaf *leaf;
    unsigned char byte;
    const char *text;
    unsigned long long length;
    const char *word_text;
    const char *const *words;
    unsigned int count;
    CliSqlStatement *statement;
} SqlCtx;
// whether a byte is one of the spaces ASCII names: space, tab, line feed, form feed and carriage return
EMBED_INLINE int cli_space(const SqlCtx *args)
{
    return (args->byte == ' ') || (args->byte == '\t') || (args->byte == '\n') || (args->byte == 0x0cu) ||
           (args->byte == '\r');
}

// what a leaf is to the reader, by its bits: a quoted name, a string, a number, a bare word, or any other mark;
// CLI_SQL_UNREAD for a mark of no bytes, a blank or a comment
EMBED_INLINE unsigned int cli_leaf_type(const SqlCtx *args)
{
    const unsigned long long is = args->leaf->is;
    if ((args->leaf->to == args->leaf->from) || ((is & (CLI_LEAF_BLANK | CLI_LEAF_COMMENT)) != 0u))
    {
        return CLI_SQL_UNREAD;
    }
    if ((is & CLI_LEAF_QUOTED) != 0u)
    {
        return CLI_SQL_NAME;
    }
    if ((is & CLI_LEAF_STRING) != 0u)
    {
        return CLI_SQL_TEXT;
    }
    if ((is & CLI_LEAF_NUMBER) != 0u)
    {
        return CLI_SQL_NUMBER;
    }
    return ((is & (CLI_LEAF_NAME | CLI_LEAF_KEYWORD | CLI_LEAF_TYPE)) != 0u) ? CLI_SQL_WORD : CLI_SQL_MARK;
}

int cli_sql_tokens(const CliSqlTextLengthDialectCfg *args)
{
    CliSqlTokens *const tokens = args->tokens;
    *tokens = (CliSqlTokens){NULL, 0u};
    CliLeafRead read;
    const int leaved =
        EMBED_CALL(cli_symbols_text_internal.language_leaves_save, CliSymbolsTextCtx, .id = "sql",
                   .state = s_states[args->dialect], .text = args->text, .size = args->length, .read = &read);
    int taken = leaved;
    if (leaved && (read.count != 0u))
    {
        tokens->at =
            g_cli_guard->temporary_buf_alloc((unsigned long long)read.count * sizeof(CliSqlToken), g_cli_db_owner);
        taken = tokens->at != NULL;
        for (unsigned int at = 0u; taken && (at < read.count); at += 1u)
        {
            const CliLeaf *const leaf = &read.leaf[at];
            const unsigned int type = EMBED_CALL(cli_leaf_type, SqlCtx, .leaf = leaf);
            if (type != CLI_SQL_UNREAD)
            {
                tokens->at[tokens->count] = (CliSqlToken){type, leaf->from, leaf->to};
                tokens->count += 1u;
            }
        }
    }
    g_cli_guard->persistent_buf_release(read.leaf, g_cli_leaves_owner);
    return taken;
}

void cli_sql_tokens_free(const CliSqlTextLengthDialectCfg *args)
{
    g_cli_guard->persistent_buf_release(args->tokens->at, g_cli_db_owner);
    *args->tokens = (CliSqlTokens){NULL, 0u};
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
    CliSqlStatement statement = {0ULL, 0, 0ULL, 0u, 0, 0, 0LL, 0};
    CliSqlTokens tokens;
    if (!EMBED_CALL(cli_sql_text_length_dialect.tokens, CliSqlTextLengthDialectCfg, .text = text,
                    .length = args->length, .dialect = args->dialect, .tokens = &tokens))
    {
        return;
    }
    for (unsigned int at = 0u; at < tokens.count; at += 1u)
    {
        const CliSqlToken token = tokens.at[at];
        const char *const word_text = text + token.start;
        const unsigned long long size = token.end - token.start;
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
    EMBED_CALL(cli_sql_text_length_dialect.tokens_free, CliSqlTextLengthDialectCfg, .tokens = &tokens);
    if (statement.started)
    {
        const CliSqlSpan span = {statement.first, statement.last};
        EMBED_CALL(args->each, CliEachSpanCfg, .context = args->context, .span = span);
    }
}

// The statement a place is in: the first that contains it, and else the last that ends before it
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
    // what is between the statement and the place: its semicolon, then nothing but spaces and a comment on its line
    const char *from = args->text + finding.before.end;
    const char *const to = args->text + ((at < args->length) ? at : args->length);
    from += ((from < to) && (from[0] == ';')) ? 1 : 0;
    for (const char *character = from; character < to; character += 1)
    {
        if (character[0] == '\n')
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
