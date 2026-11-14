// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sql_does.c: what a statement does by its first word, the query that counts the rows an unbounded write reaches, and
// the object a statement that shapes the schema names
#include "../text/text.h"
#include "sql.h"

// the most words of a statement read, and the most of its tokens past spaces and comments
#define CLI_SQL_WORDS 4096u

// A word of a statement outside its strings, names and comments: where it stands, and the depth of brackets at it
typedef struct
{
    unsigned long long start;
    unsigned long long end;
    long long depth;
} CliSqlWord;

// the words of a statement and its tokens past spaces and comments, guarded by the lock while one is read
static CliSqlWord s_words[CLI_SQL_WORDS];
static CliSqlToken s_tokens[CLI_SQL_WORDS];

typedef struct
{
    const char *text;
    unsigned long long length;
    const char *word_text;
    const char *const *words;
    unsigned int count;
    const char *statement;
    unsigned int dialect;
    unsigned int index;
    unsigned int level;
    unsigned long long from;
    const char *const *stops;
    VerbaLinea *out;
    unsigned long long to;
    unsigned int *at;
    char *out_capacity;
    unsigned long long capacity;
    CliShaped *shaped;
} SqlDoesCtx;
// whether `length` bytes of `text` are `word` in either case
EMBED_INLINE int cli_is(const SqlDoesCtx *args)
{
    return EMBED_CALL(cli_text_size_one.same_folded, CliTextSizeOneCfg, .size = args->length, .one = args->text,
                      .other_pointer = args->word_text);
}

EMBED_INLINE int cli_in(const SqlDoesCtx *args)
{
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if (EMBED_CALL(cli_is, SqlDoesCtx, .text = args->text, .length = args->length, .word_text = args->words[at]))
        {
            return 1;
        }
    }
    return 0;
}

// The words of a statement outside its strings, names and comments, each with the depth of brackets it stands at:
// their count
EMBED_INLINE unsigned int cli_words_get(const SqlDoesCtx *args)
{
    unsigned long long length = args->length;
    unsigned int dialect = args->dialect;
    long long depth = 0LL;
    unsigned int count = 0u;
    unsigned long long at = 0ULL;
    CliSqlToken token;
    while (EMBED_CALL(cli_sql_text_length_dialect.token, CliSqlTextLengthDialectCfg, .text = args->statement,
                      .length = length, .dialect = dialect, .at = &at, .token = &token) &&
           (count < CLI_SQL_WORDS))
    {
        const unsigned long long size = token.end - token.start;
        if ((token.type == CLI_SQL_MARK) && (size == 1u))
        {
            if (args->statement[token.start] == '(')
            {
                depth += 1LL;
            }
            else if (args->statement[token.start] == ')')
            {
                depth += -1LL;
            }
            else
            {
                depth += 0LL;
            }
        }
        else if (token.type == CLI_SQL_WORD)
        {
            const CliSqlWord word_text = {token.start, token.end, depth};
            s_words[count] = word_text;
            count += 1u;
        }
    }
    return count;
}

EMBED_INLINE int cli_word_is(const SqlDoesCtx *args)
{
    return EMBED_CALL(cli_is, SqlDoesCtx, .text = args->statement + s_words[args->index].start,
                      .length = s_words[args->index].end - s_words[args->index].start, .word_text = args->word_text);
}

static const char *const s_writers[6] = {"insert", "update", "delete", "merge", "replace", "upsert"};

// whether any word of the statement writes rows
EMBED_INLINE int cli_writes(const SqlDoesCtx *args)
{
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if (EMBED_CALL(cli_in, SqlDoesCtx, .text = args->statement + s_words[at].start,
                       .length = s_words[at].end - s_words[at].start, .words = s_writers, .count = 6u))
        {
            return 1;
        }
    }
    return 0;
}

EMBED_INLINE unsigned int cli_does_lock(const SqlDoesCtx *args)
{
    static const char *const s_reads[6] = {"show", "describe", "desc", "values", "table", "help"};
    static const char *const s_changes[12] = {"insert", "update", "delete", "merge", "replace", "upsert",
                                              "copy",   "load",   "call",   "do",    "execute", "exec"};
    static const char *const s_shapes[15] = {"create",  "alter",  "drop",   "truncate", "rename",
                                             "comment", "grant",  "revoke", "reindex",  "cluster",
                                             "refresh", "attach", "detach", "vacuum",   "import"};
    static const char *const s_transactions[9] = {"begin",   "start", "commit", "rollback", "savepoint",
                                                  "release", "end",   "abort",  "xa"};
    const unsigned int count = cli_words_get(args);
    const char *const first = (count != 0u) ? (args->statement + s_words[0].start) : "";
    const unsigned long long size = (count != 0u) ? (s_words[0].end - s_words[0].start) : 0ULL;
    if (EMBED_CALL(cli_is, SqlDoesCtx, .text = first, .length = size, .word_text = "select"))
    {
        // SELECT ... INTO a new table makes it, in PostgreSQL; in MySQL it writes a file or a variable
        int into = 0;
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            into = into || (EMBED_CALL(cli_word_is, SqlDoesCtx, .statement = args->statement, .index = at,
                                       .word_text = "into") &&
                            (s_words[at].depth == 0LL));
        }
        return (into && (args->dialect == CLI_SQL_POSTGRES)) ? CLI_DOES_SHAPES : CLI_DOES_READS;
    }
    if (EMBED_CALL(cli_is, SqlDoesCtx, .text = first, .length = size, .word_text = "with"))
    {
        return EMBED_CALL(cli_writes, SqlDoesCtx, .statement = args->statement, .count = count) ? CLI_DOES_WRITES
                                                                                                : CLI_DOES_READS;
    }
    if (EMBED_CALL(cli_in, SqlDoesCtx, .text = first, .length = size, .words = s_reads, .count = 6u))
    {
        return CLI_DOES_READS;
    }
    if (EMBED_CALL(cli_is, SqlDoesCtx, .text = first, .length = size, .word_text = "explain"))
    {
        int analyze = 0;
        for (unsigned int at = 0u; (at < count) && (at < 4u); at += 1u)
        {
            analyze =
                analyze ||
                EMBED_CALL(cli_word_is, SqlDoesCtx, .statement = args->statement, .index = at,
                           .word_text = "analyze") ||
                EMBED_CALL(cli_word_is, SqlDoesCtx, .statement = args->statement, .index = at, .word_text = "analyse");
        }
        return (analyze && EMBED_CALL(cli_writes, SqlDoesCtx, .statement = args->statement, .count = count))
                   ? CLI_DOES_WRITES
                   : CLI_DOES_READS;
    }
    if (EMBED_CALL(cli_is, SqlDoesCtx, .text = first, .length = size, .word_text = "pragma"))
    {
        for (unsigned long long at = 0ULL; at < args->length; at += 1ULL)
        {
            if (args->statement[at] == '=')
            {
                return CLI_DOES_OTHER;
            }
        }
        return CLI_DOES_READS;
    }
    if (EMBED_CALL(cli_in, SqlDoesCtx, .text = first, .length = size, .words = s_changes, .count = 12u))
    {
        return CLI_DOES_WRITES;
    }
    if (EMBED_CALL(cli_in, SqlDoesCtx, .text = first, .length = size, .words = s_shapes, .count = 15u))
    {
        return CLI_DOES_SHAPES;
    }
    return EMBED_CALL(cli_in, SqlDoesCtx, .text = first, .length = size, .words = s_transactions, .count = 9u)
               ? CLI_DOES_TRANSACTION
               : CLI_DOES_OTHER;
}

unsigned int cli_sql_does(const CliSqlLengthCfg *args)
{
    const unsigned int does = EMBED_CALL(cli_does_lock, SqlDoesCtx, .statement = args->statement,
                                         .length = args->length, .dialect = args->dialect);
    return does;
}

int cli_sql_changes(const CliSqlLengthCfg *args)
{
    const unsigned int does = cli_sql_length.does(args);
    return (does == CLI_DOES_WRITES) || (does == CLI_DOES_SHAPES);
}

// the words at the statement's own level, moved to the front: their count
EMBED_INLINE unsigned int cli_level(const SqlDoesCtx *args)
{
    unsigned int level = 0u;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if (s_words[at].depth == 0LL)
        {
            s_words[level] = s_words[at];
            level += 1u;
        }
    }
    return level;
}

// the first word at the level named `word`, CLI_SQL_WORDS where none is
EMBED_INLINE unsigned int cli_find(const SqlDoesCtx *args)
{
    for (unsigned int at = 0u; at < args->level; at += 1u)
    {
        if (EMBED_CALL(cli_word_is, SqlDoesCtx, .statement = args->statement, .index = at,
                       .word_text = args->word_text))
        {
            return at;
        }
    }
    return CLI_SQL_WORDS;
}

// where the first word at the level past `from` that is one of `stops` starts, or the statement's end
EMBED_INLINE unsigned long long cli_ends(const SqlDoesCtx *args)
{
    unsigned long long end = args->length;
    for (unsigned int at = 0u; at < args->level; at += 1u)
    {
        if ((s_words[at].start > args->from) &&
            EMBED_CALL(cli_in, SqlDoesCtx, .text = args->statement + s_words[at].start,
                       .length = s_words[at].end - s_words[at].start, .words = args->stops, .count = args->count))
        {
            end = (s_words[at].start < end) ? s_words[at].start : end;
        }
    }
    return end;
}

// the bytes from `from` to `to` of the statement put, trimmed
EMBED_INLINE void cli_trimmed_append(const SqlDoesCtx *args)
{
    unsigned long long from = args->from;
    unsigned long long to = args->to;
    from = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = args->statement, .at = from,
                      .size = to, .set = CLI_TEXT_WHITE);
    to = (from + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = args->statement + from,
                            .at = to - from, .set = CLI_TEXT_WHITE));
    mmgr_line_put_n(args->out, args->statement + from, (to > from) ? (to - from) : 0ULL);
}

// the sum of the counts of the tables the `length` bytes of `statement` list between commas, each counted alone
EMBED_INLINE void cli_counts_added(const SqlDoesCtx *args)
{
    mmgr_line_put(args->out, "SELECT ");
    unsigned long long piece = 0ULL;
    for (unsigned long long at = 0ULL; at <= args->length; at += 1ULL)
    {
        if ((at != args->length) && (args->statement[at] != ','))
        {
            continue;
        }
        mmgr_line_put(args->out, (piece != 0ULL) ? " + (SELECT count(*) FROM " : "(SELECT count(*) FROM ");
        EMBED_CALL(cli_trimmed_append, SqlDoesCtx, .out = args->out, .statement = args->statement, .from = piece,
                   .to = at);
        mmgr_line_ch(args->out, ')');
        piece = at + 1ULL;
    }
}

// The table an UPDATE or a DELETE with no WHERE at its own level reaches every row of, or each a TRUNCATE names, put
// after `SELECT count(*) FROM`; or the sum of each table's count where a TRUNCATE names more than one
EMBED_INLINE int cli_unbounded_lock(const SqlDoesCtx *args)
{
    static const char *const s_delete_stops[4] = {"using", "returning", "order", "limit"};
    static const char *const s_using_stops[1] = {"returning"};
    static const char *const s_truncate_stops[4] = {"restart", "continue", "cascade", "restrict"};
    static char s_source[1u << 20u];
    const unsigned int level = EMBED_CALL(cli_level, SqlDoesCtx, .count = cli_words_get(args));
    if (level == 0u)
    {
        return 0;
    }
    const int bounded = EMBED_CALL(cli_find, SqlDoesCtx, .statement = args->statement, .level = level,
                                   .word_text = "where") != CLI_SQL_WORDS;
    VerbaLinea source = {s_source, sizeof(s_source), 0ULL};
    if (EMBED_CALL(cli_word_is, SqlDoesCtx, .statement = args->statement, .index = 0u, .word_text = "update") &&
        !bounded)
    {
        const unsigned int set =
            EMBED_CALL(cli_find, SqlDoesCtx, .statement = args->statement, .level = level, .word_text = "set");
        if (set == CLI_SQL_WORDS)
        {
            return 0;
        }
        EMBED_CALL(cli_trimmed_append, SqlDoesCtx, .out = &source, .statement = args->statement, .from = s_words[0].end,
                   .to = s_words[set].start);
    }
    else if (EMBED_CALL(cli_word_is, SqlDoesCtx, .statement = args->statement, .index = 0u, .word_text = "delete") &&
             !bounded)
    {
        const unsigned int from =
            EMBED_CALL(cli_find, SqlDoesCtx, .statement = args->statement, .level = level, .word_text = "from");
        if (from == CLI_SQL_WORDS)
        {
            return 0;
        }
        const unsigned long long end =
            EMBED_CALL(cli_ends, SqlDoesCtx, .statement = args->statement, .length = args->length, .level = level,
                       .from = s_words[from].end, .stops = s_delete_stops, .count = 4u);
        EMBED_CALL(cli_trimmed_append, SqlDoesCtx, .out = &source, .statement = args->statement,
                   .from = s_words[from].end, .to = end);
        const unsigned int using =
            EMBED_CALL(cli_find, SqlDoesCtx, .statement = args->statement, .level = level, .word_text = "using");
        if (using != CLI_SQL_WORDS)
        {
            mmgr_line_put(&source, ", ");
            EMBED_CALL(cli_trimmed_append, SqlDoesCtx, .out = &source, .statement = args->statement,
                       .from = s_words[using].end,
                       .to =
                           EMBED_CALL(cli_ends, SqlDoesCtx, .statement = args->statement, .length = args->length,
                                      .level = level, .from = s_words[using].end, .stops = s_using_stops, .count = 1u));
        }
    }
    else if (EMBED_CALL(cli_word_is, SqlDoesCtx, .statement = args->statement, .index = 0u, .word_text = "truncate"))
    {
        const unsigned long long start =
            ((level > 1u) &&
             EMBED_CALL(cli_word_is, SqlDoesCtx, .statement = args->statement, .index = 1u, .word_text = "table"))
                ? s_words[1].end
                : s_words[0].end;
        const unsigned long long end =
            EMBED_CALL(cli_ends, SqlDoesCtx, .statement = args->statement, .length = args->length, .level = level,
                       .from = start, .stops = s_truncate_stops, .count = 4u);
        EMBED_CALL(cli_trimmed_append, SqlDoesCtx, .out = &source, .statement = args->statement, .from = start,
                   .to = end);
        const unsigned long long names = mmgr_line_ok(&source) ? source.at : 0ULL;
        int list_entries = 0;
        for (unsigned long long at = 0ULL; at < names; at += 1ULL)
        {
            list_entries = list_entries || (s_source[at] == ',');
        }
        if (list_entries)
        {
            // Each table a TRUNCATE names is counted alone and the counts added
            EMBED_CALL(cli_counts_added, SqlDoesCtx, .out = args->out, .statement = s_source, .length = names);
            return 1;
        }
    }
    else
    {
        return 0;
    }
    const unsigned long long size = mmgr_line_ok(&source) ? source.at : 0ULL;
    if (size == 0ULL)
    {
        return 0;
    }
    mmgr_line_put(args->out, "SELECT count(*) FROM ");
    mmgr_line_put_n(args->out, s_source, size);
    return 1;
}

int cli_sql_unbounded(const CliSqlLengthDialectCfg *args)
{
    const int found = EMBED_CALL(cli_unbounded_lock, SqlDoesCtx, .statement = args->statement, .length = args->length,
                                 .dialect = args->dialect, .out = args->out);
    return found;
}

// whether the `index`th token past spaces and comments is `word` in either case, none past the last
EMBED_INLINE int cli_lower_is(const SqlDoesCtx *args)
{
    return (args->index < args->count)
               ? EMBED_CALL(cli_is, SqlDoesCtx, .text = args->statement + s_tokens[args->index].start,
                            .length = s_tokens[args->index].end - s_tokens[args->index].start,
                            .word_text = args->word_text)
               : (args->word_text[0] == '\0');
}

// a name of names and dots from token `*at` on, each name after a dot, put into `out`, sized for `capacity`; `*at`
// moved past it
EMBED_INLINE void cli_dot(const SqlDoesCtx *args)
{
    unsigned int *at = args->at;
    VerbaLinea line = {args->out_capacity, args->capacity, 0ULL};
    while (*at < args->count)
    {
        const CliSqlToken *const token = &s_tokens[*at];
        const char *const word_text = args->statement + token->start;
        const unsigned long long size = token->end - token->start;
        const int dot = (size == 1u) && (word_text[0] == '.');
        const int joined = (token->type == CLI_SQL_WORD) || (token->type == CLI_SQL_NAME) || dot;
        const int after_dot = (line.at == 0ULL) || (args->out_capacity[line.at - 1u] == '.');
        if (!joined || (!after_dot && !dot))
        {
            break;
        }
        mmgr_line_put_n(&line, word_text, size);
        *at += 1u;
    }
    mmgr_line_finish(&line);
}

// the `index`th token past spaces and comments in lower case into `out`, sized for CLI_NAME_MAX_LEN, empty past the
// last
EMBED_INLINE void cli_lower_into(const SqlDoesCtx *args)
{
    VerbaLinea line = {args->out_capacity, CLI_NAME_MAX_LEN, 0ULL};
    for (unsigned long long at = (args->index < args->count) ? s_tokens[args->index].start : 0ULL;
         (args->index < args->count) && (at < s_tokens[args->index].end); at += 1ULL)
    {
        const char letter = args->statement[at];
        mmgr_line_ch(&line, ((letter >= 'A') && (letter <= 'Z')) ? (char)(letter + 32) : letter);
    }
    mmgr_line_finish(&line);
}

EMBED_INLINE int cli_shaped_lock(const SqlDoesCtx *args)
{
    unsigned long long length = args->length;
    unsigned int dialect = args->dialect;
    static const char *const s_verbs[6] = {"create", "alter", "drop", "truncate", "rename", "comment"};
    static const char *const s_skips[14] = {"temp",    "temporary", "unique",  "materialized", "unlogged",
                                            "global",  "local",     "foreign", "virtual",      "recursive",
                                            "definer", "algorithm", "sql",     "security"};
    static const char *const s_guards[4] = {"if", "not", "exists", "concurrently"};
    unsigned int count = 0u;
    unsigned long long place = 0ULL;
    CliSqlToken token;
    while (EMBED_CALL(cli_sql_text_length_dialect.token, CliSqlTextLengthDialectCfg, .text = args->statement,
                      .length = length, .dialect = dialect, .at = &place, .token = &token) &&
           (count < CLI_SQL_WORDS))
    {
        if ((token.type != CLI_SQL_SPACE) && (token.type != CLI_SQL_COMMENT))
        {
            s_tokens[count] = token;
            count += 1u;
        }
    }
    EMBED_CALL(cli_lower_into, SqlDoesCtx, .statement = args->statement, .count = count, .index = 0u,
               .out_capacity = args->shaped->verb);
    if (!EMBED_CALL(cli_in, SqlDoesCtx, .text = args->shaped->verb,
                    .length = cellul.len(&(CatenaFinitaCfg){.src = args->shaped->verb, .cap = CLI_POOL_BYTES}),
                    .words = s_verbs, .count = 6u))
    {
        return 0;
    }
    unsigned int at = 1u;
    if ((cellul.eq(&(CatenaFinitaCfg){.src = args->shaped->verb, .other = "create", .cap = CLI_POOL_BYTES})) &&
        EMBED_CALL(cli_lower_is, SqlDoesCtx, .statement = args->statement, .count = count, .index = 1u,
                   .word_text = "or") &&
        EMBED_CALL(cli_lower_is, SqlDoesCtx, .statement = args->statement, .count = count, .index = 2u,
                   .word_text = "replace"))
    {
        at = 3u;
    }
    while ((at < count) && EMBED_CALL(cli_in, SqlDoesCtx, .text = args->statement + s_tokens[at].start,
                                      .length = s_tokens[at].end - s_tokens[at].start, .words = s_skips, .count = 14u))
    {
        at += 1u;
    }
    EMBED_CALL(cli_lower_into, SqlDoesCtx, .statement = args->statement, .count = count, .index = at,
               .out_capacity = args->shaped->type);
    if ((cellul.eq(&(CatenaFinitaCfg){.src = args->shaped->verb, .other = "truncate", .cap = CLI_POOL_BYTES})) &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = args->shaped->type, .other = "table", .cap = CLI_POOL_BYTES})))
    {
        memor.cpy(&(MemoriaCfg){.dst = args->shaped->type,
                                .src = "table",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "table", .cap = CLI_POOL_BYTES}) + 1u});
    }
    else
    {
        at += 1u;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->shaped->verb, .other = "comment", .cap = CLI_POOL_BYTES}))
    {
        return 0;
    }
    while ((at < count) && EMBED_CALL(cli_in, SqlDoesCtx, .text = args->statement + s_tokens[at].start,
                                      .length = s_tokens[at].end - s_tokens[at].start, .words = s_guards, .count = 4u))
    {
        at += 1u;
    }
    EMBED_CALL(cli_dot, SqlDoesCtx, .statement = args->statement, .count = count, .at = &at,
               .out_capacity = args->shaped->name, .capacity = sizeof(args->shaped->name));
    if (args->shaped->name[0] == '\0')
    {
        return 0;
    }
    args->shaped->has_on = 0;
    args->shaped->on[0] = '\0';
    if ((cellul.eq(&(CatenaFinitaCfg){.src = args->shaped->type, .other = "index", .cap = CLI_POOL_BYTES})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = args->shaped->type, .other = "trigger", .cap = CLI_POOL_BYTES})))
    {
        unsigned int found = 0u;
        while ((found < count) && !EMBED_CALL(cli_lower_is, SqlDoesCtx, .statement = args->statement, .count = count,
                                              .index = found, .word_text = "on"))
        {
            found += 1u;
        }
        if (found < count)
        {
            unsigned int table = found + 1u;
            table += EMBED_CALL(cli_lower_is, SqlDoesCtx, .statement = args->statement, .count = count, .index = table,
                                .word_text = "only")
                         ? 1u
                         : 0u;
            EMBED_CALL(cli_dot, SqlDoesCtx, .statement = args->statement, .count = count, .at = &table,
                       .out_capacity = args->shaped->on, .capacity = sizeof(args->shaped->on));
            args->shaped->has_on = args->shaped->on[0] != '\0';
        }
    }
    return 1;
}

int cli_sql_shape(const CliSqlLengthCfg *args)
{
    const int found = EMBED_CALL(cli_shaped_lock, SqlDoesCtx, .statement = args->statement, .length = args->length,
                                 .dialect = args->dialect, .shaped = args->shaped);
    return found;
}
