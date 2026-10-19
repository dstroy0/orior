// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sql_does.c: what a statement does by its first word, the query that counts the rows an unbounded write reaches, and
// the object a statement that shapes the schema names
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

// the words of a statement and its tokens past spaces and comments, held under the lock while one is read
static CliSqlWord s_words[CLI_SQL_WORDS];
static CliSqlToken s_tokens[CLI_SQL_WORDS];
static CliLock s_lock = CLI_LOCK_INIT;

// whether `length` bytes of `text` are `word` in either case
static int cli_is(const char *text, unsigned long long length, const char *word)
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

static int cli_in(const char *text, unsigned long long length, const char *const *words, unsigned int count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (cli_is(text, length, words[at]))
        {
            return 1;
        }
    }
    return 0;
}

// The words of a statement outside its strings, names and comments, each with the depth of brackets it stands at:
// their count
static unsigned int cli_words_of(const char *statement, unsigned long long length, unsigned int dialect)
{
    long long depth = 0ll;
    unsigned int count = 0u;
    unsigned long long at = 0ull;
    CliSqlToken token;
    while (cli_sql_token(statement, length, dialect, &at, &token) && (count < CLI_SQL_WORDS))
    {
        const unsigned long long size = token.end - token.start;
        if ((token.kind == CLI_SQL_MARK) && (size == 1u))
        {
            depth += (statement[token.start] == '(') ? 1ll : (statement[token.start] == ')') ? -1ll : 0ll;
        }
        else if (token.kind == CLI_SQL_WORD)
        {
            const CliSqlWord word = {token.start, token.end, depth};
            s_words[count] = word;
            count += 1u;
        }
    }
    return count;
}

static int cli_word_is(const char *statement, unsigned int index, const char *word)
{
    return cli_is(statement + s_words[index].start, s_words[index].end - s_words[index].start, word);
}

static const char *const s_writers[6] = {"insert", "update", "delete", "merge", "replace", "upsert"};

// whether any word of the statement writes rows
static int cli_writes(const char *statement, unsigned int count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (cli_in(statement + s_words[at].start, s_words[at].end - s_words[at].start, s_writers, 6u))
        {
            return 1;
        }
    }
    return 0;
}

static unsigned int cli_does_held(const char *statement, unsigned long long length, unsigned int dialect)
{
    static const char *const s_reads[6] = {"show", "describe", "desc", "values", "table", "help"};
    static const char *const s_changes[12] = {"insert", "update", "delete", "merge", "replace", "upsert",
                                              "copy",   "load",   "call",   "do",    "execute", "exec"};
    static const char *const s_shapes[15] = {"create",  "alter",  "drop",   "truncate", "rename",
                                             "comment", "grant",  "revoke", "reindex",  "cluster",
                                             "refresh", "attach", "detach", "vacuum",   "import"};
    static const char *const s_transactions[9] = {"begin",   "start", "commit", "rollback", "savepoint",
                                                  "release", "end",   "abort",  "xa"};
    const unsigned int count = cli_words_of(statement, length, dialect);
    const char *const first = (count != 0u) ? (statement + s_words[0].start) : "";
    const unsigned long long size = (count != 0u) ? (s_words[0].end - s_words[0].start) : 0ull;
    if (cli_is(first, size, "select"))
    {
        // SELECT ... INTO a new table makes it, in PostgreSQL; in MySQL it writes a file or a variable
        int into = 0;
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            into = into || (cli_word_is(statement, at, "into") && (s_words[at].depth == 0ll));
        }
        return (into && (dialect == CLI_SQL_POSTGRES)) ? CLI_DOES_SHAPES : CLI_DOES_READS;
    }
    if (cli_is(first, size, "with"))
    {
        return cli_writes(statement, count) ? CLI_DOES_WRITES : CLI_DOES_READS;
    }
    if (cli_in(first, size, s_reads, 6u))
    {
        return CLI_DOES_READS;
    }
    if (cli_is(first, size, "explain"))
    {
        int analyze = 0;
        for (unsigned int at = 0u; (at < count) && (at < 4u); at += 1u)
        {
            analyze = analyze || cli_word_is(statement, at, "analyze") || cli_word_is(statement, at, "analyse");
        }
        return (analyze && cli_writes(statement, count)) ? CLI_DOES_WRITES : CLI_DOES_READS;
    }
    if (cli_is(first, size, "pragma"))
    {
        for (unsigned long long at = 0ull; at < length; at += 1ull)
        {
            if (statement[at] == '=')
            {
                return CLI_DOES_OTHER;
            }
        }
        return CLI_DOES_READS;
    }
    if (cli_in(first, size, s_changes, 12u))
    {
        return CLI_DOES_WRITES;
    }
    if (cli_in(first, size, s_shapes, 15u))
    {
        return CLI_DOES_SHAPES;
    }
    return cli_in(first, size, s_transactions, 9u) ? CLI_DOES_TRANSACTION : CLI_DOES_OTHER;
}

unsigned int cli_sql_does(const char *statement, unsigned long long length, unsigned int dialect)
{
    cli_lock_take(&s_lock);
    const unsigned int does = cli_does_held(statement, length, dialect);
    cli_lock_give(&s_lock);
    return does;
}

int cli_sql_changes(const char *statement, unsigned long long length, unsigned int dialect)
{
    const unsigned int does = cli_sql_does(statement, length, dialect);
    return (does == CLI_DOES_WRITES) || (does == CLI_DOES_SHAPES);
}

// the words at the statement's own level, moved to the front: their count
static unsigned int cli_level(unsigned int count)
{
    unsigned int level = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (s_words[at].depth == 0ll)
        {
            s_words[level] = s_words[at];
            level += 1u;
        }
    }
    return level;
}

// the first word at the level named `word`, CLI_SQL_WORDS where none is
static unsigned int cli_find(const char *statement, unsigned int level, const char *word)
{
    for (unsigned int at = 0u; at < level; at += 1u)
    {
        if (cli_word_is(statement, at, word))
        {
            return at;
        }
    }
    return CLI_SQL_WORDS;
}

// where the first word at the level past `from` that is one of `stops` starts, or the statement's end
static unsigned long long cli_ends(const char *statement, unsigned long long length, unsigned int level,
                                   unsigned long long from, const char *const *stops, unsigned int count)
{
    unsigned long long end = length;
    for (unsigned int at = 0u; at < level; at += 1u)
    {
        if ((s_words[at].start > from) &&
            cli_in(statement + s_words[at].start, s_words[at].end - s_words[at].start, stops, count))
        {
            end = (s_words[at].start < end) ? s_words[at].start : end;
        }
    }
    return end;
}

// the bytes from `from` to `to` of the statement put, trimmed
static void cli_trimmed_put(CliLine *out, const char *statement, unsigned long long from, unsigned long long to)
{
    while ((from < to) && ((statement[from] == ' ') || ((statement[from] >= '\t') && (statement[from] <= '\r'))))
    {
        from += 1ull;
    }
    while ((to > from) &&
           ((statement[to - 1u] == ' ') || ((statement[to - 1u] >= '\t') && (statement[to - 1u] <= '\r'))))
    {
        to -= 1ull;
    }
    cli_line_counted(out, statement + from, (to > from) ? (to - from) : 0ull);
}

// The table an UPDATE or a DELETE with no WHERE at its own level reaches every row of, or each a TRUNCATE names, put
// after `SELECT count(*) FROM`; or the sum of each table's count where a TRUNCATE names more than one
static int cli_unbounded_held(const char *statement, unsigned long long length, unsigned int dialect, CliLine *out)
{
    static const char *const s_delete_stops[4] = {"using", "returning", "order", "limit"};
    static const char *const s_using_stops[1] = {"returning"};
    static const char *const s_truncate_stops[4] = {"restart", "continue", "cascade", "restrict"};
    static char s_source[1u << 20u];
    const unsigned int level = cli_level(cli_words_of(statement, length, dialect));
    if (level == 0u)
    {
        return 0;
    }
    const int bounded = cli_find(statement, level, "where") != CLI_SQL_WORDS;
    CliLine source = {s_source, sizeof(s_source), 0ull};
    if (cli_word_is(statement, 0u, "update") && !bounded)
    {
        const unsigned int set = cli_find(statement, level, "set");
        if (set == CLI_SQL_WORDS)
        {
            return 0;
        }
        cli_trimmed_put(&source, statement, s_words[0].end, s_words[set].start);
    }
    else if (cli_word_is(statement, 0u, "delete") && !bounded)
    {
        const unsigned int from = cli_find(statement, level, "from");
        if (from == CLI_SQL_WORDS)
        {
            return 0;
        }
        const unsigned long long end = cli_ends(statement, length, level, s_words[from].end, s_delete_stops, 4u);
        cli_trimmed_put(&source, statement, s_words[from].end, end);
        const unsigned int using = cli_find(statement, level, "using");
        if (using != CLI_SQL_WORDS)
        {
            cli_line_text(&source, ", ");
            cli_trimmed_put(&source, statement, s_words[using].end,
                            cli_ends(statement, length, level, s_words[using].end, s_using_stops, 1u));
        }
    }
    else if (cli_word_is(statement, 0u, "truncate"))
    {
        const unsigned long long start =
            ((level > 1u) && cli_word_is(statement, 1u, "table")) ? s_words[1].end : s_words[0].end;
        const unsigned long long end = cli_ends(statement, length, level, start, s_truncate_stops, 4u);
        cli_trimmed_put(&source, statement, start, end);
        const unsigned long long names = cli_line_fits(&source) ? source.at : 0ull;
        int listed = 0;
        for (unsigned long long at = 0ull; at < names; at += 1ull)
        {
            listed = listed || (s_source[at] == ',');
        }
        if (listed)
        {
            // Each table a TRUNCATE names is counted alone and the counts added
            cli_line_text(out, "SELECT ");
            unsigned long long piece = 0ull;
            for (unsigned long long at = 0ull; at <= names; at += 1ull)
            {
                if ((at == names) || (s_source[at] == ','))
                {
                    cli_line_text(out, (piece != 0ull) ? " + (SELECT count(*) FROM " : "(SELECT count(*) FROM ");
                    cli_trimmed_put(out, s_source, piece, at);
                    cli_line_character(out, ')');
                    piece = at + 1ull;
                }
            }
            return 1;
        }
    }
    else
    {
        return 0;
    }
    const unsigned long long size = cli_line_fits(&source) ? source.at : 0ull;
    if (size == 0ull)
    {
        return 0;
    }
    cli_line_text(out, "SELECT count(*) FROM ");
    cli_line_counted(out, s_source, size);
    return 1;
}

int cli_sql_unbounded(const char *statement, unsigned long long length, unsigned int dialect, CliLine *out)
{
    cli_lock_take(&s_lock);
    const int found = cli_unbounded_held(statement, length, dialect, out);
    cli_lock_give(&s_lock);
    return found;
}

// whether the `index`th token past spaces and comments is `word` in either case, none past the last
static int cli_lower_is(const char *statement, unsigned int count, unsigned int index, const char *word)
{
    return (index < count)
               ? cli_is(statement + s_tokens[index].start, s_tokens[index].end - s_tokens[index].start, word)
               : (word[0] == '\0');
}

// a name of names and dots from token `*at` on, each name after a dot, put into `out`, which holds `room`; `*at` moved
// past it
static void cli_dotted(const char *statement, unsigned int count, unsigned int *at, char *out, unsigned long long room)
{
    CliLine line = {out, room, 0ull};
    while (*at < count)
    {
        const CliSqlToken *const token = &s_tokens[*at];
        const char *const word = statement + token->start;
        const unsigned long long size = token->end - token->start;
        const int dot = (size == 1u) && (word[0] == '.');
        const int joined = (token->kind == CLI_SQL_WORD) || (token->kind == CLI_SQL_NAME) || dot;
        const int after_dot = (line.at == 0ull) || (out[line.at - 1u] == '.');
        if (!joined || (!after_dot && !dot))
        {
            break;
        }
        cli_line_counted(&line, word, size);
        *at += 1u;
    }
    cli_line_closed(&line);
}

// the `index`th token past spaces and comments in lower case into `out`, which holds CLI_NAME_LONGEST, empty past the
// last
static void cli_lower_into(const char *statement, unsigned int count, unsigned int index, char *out)
{
    CliLine line = {out, CLI_NAME_LONGEST, 0ull};
    for (unsigned long long at = (index < count) ? s_tokens[index].start : 0ull;
         (index < count) && (at < s_tokens[index].end); at += 1ull)
    {
        const char letter = statement[at];
        cli_line_character(&line, ((letter >= 'A') && (letter <= 'Z')) ? (char)(letter + 32) : letter);
    }
    cli_line_closed(&line);
}

static int cli_shaped_held(const char *statement, unsigned long long length, unsigned int dialect, CliShaped *shaped)
{
    static const char *const s_verbs[6] = {"create", "alter", "drop", "truncate", "rename", "comment"};
    static const char *const s_skips[14] = {"temp",    "temporary", "unique",  "materialized", "unlogged",
                                            "global",  "local",     "foreign", "virtual",      "recursive",
                                            "definer", "algorithm", "sql",     "security"};
    static const char *const s_guards[4] = {"if", "not", "exists", "concurrently"};
    unsigned int count = 0u;
    unsigned long long place = 0ull;
    CliSqlToken token;
    while (cli_sql_token(statement, length, dialect, &place, &token) && (count < CLI_SQL_WORDS))
    {
        if ((token.kind != CLI_SQL_SPACE) && (token.kind != CLI_SQL_COMMENT))
        {
            s_tokens[count] = token;
            count += 1u;
        }
    }
    cli_lower_into(statement, count, 0u, shaped->verb);
    if (!cli_in(shaped->verb, cellul.len(&(CatenaFinitaCfg){.src = shaped->verb, .cap = CLI_CELLS_POOL}), s_verbs, 6u))
    {
        return 0;
    }
    unsigned int at = 1u;
    if ((cellul.eq(&(CatenaFinitaCfg){.src = shaped->verb, .other = "create", .cap = CLI_CELLS_POOL})) &&
        cli_lower_is(statement, count, 1u, "or") && cli_lower_is(statement, count, 2u, "replace"))
    {
        at = 3u;
    }
    while ((at < count) && cli_in(statement + s_tokens[at].start, s_tokens[at].end - s_tokens[at].start, s_skips, 14u))
    {
        at += 1u;
    }
    cli_lower_into(statement, count, at, shaped->kind);
    if ((cellul.eq(&(CatenaFinitaCfg){.src = shaped->verb, .other = "truncate", .cap = CLI_CELLS_POOL})) &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = shaped->kind, .other = "table", .cap = CLI_CELLS_POOL})))
    {
        memor.cpy(&(MemoriaCfg){.dst = shaped->kind,
                                .src = "table",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "table", .cap = CLI_CELLS_POOL}) + 1u});
    }
    else
    {
        at += 1u;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = shaped->verb, .other = "comment", .cap = CLI_CELLS_POOL}))
    {
        return 0;
    }
    while ((at < count) && cli_in(statement + s_tokens[at].start, s_tokens[at].end - s_tokens[at].start, s_guards, 4u))
    {
        at += 1u;
    }
    cli_dotted(statement, count, &at, shaped->name, sizeof(shaped->name));
    if (shaped->name[0] == '\0')
    {
        return 0;
    }
    shaped->has_on = 0;
    shaped->on[0] = '\0';
    if ((cellul.eq(&(CatenaFinitaCfg){.src = shaped->kind, .other = "index", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = shaped->kind, .other = "trigger", .cap = CLI_CELLS_POOL})))
    {
        unsigned int found = 0u;
        while ((found < count) && !cli_lower_is(statement, count, found, "on"))
        {
            found += 1u;
        }
        if (found < count)
        {
            unsigned int table = found + 1u;
            table += cli_lower_is(statement, count, table, "only") ? 1u : 0u;
            cli_dotted(statement, count, &table, shaped->on, sizeof(shaped->on));
            shaped->has_on = shaped->on[0] != '\0';
        }
    }
    return 1;
}

int cli_sql_shaped(const char *statement, unsigned long long length, unsigned int dialect, CliShaped *shaped)
{
    cli_lock_take(&s_lock);
    const int found = cli_shaped_held(statement, length, dialect, shaped);
    cli_lock_give(&s_lock);
    return found;
}
