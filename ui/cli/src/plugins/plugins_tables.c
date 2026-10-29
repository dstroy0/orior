// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugins_tables.c: a language plugin's grammar made whole before the colorer stores it: each word list a table of the
// tree fills read from that table, `{name}` in the tables' paths the name of the folder the plugin stands for, and each
// rule whose pattern is a word list given the list's words as its pattern; and the folders a plugin stands for
#include "../command_line/cells.h"
#include "../command_line/file_writing.h"
#include "../editor/defs.h"
#include "../text/regexp.h"
#include "plugins.h"

// the bytes a grammar and a pattern are begun to be written in, doubling as they are, the most groups a list's pattern
// has and one, and the most letters of a cell matched
#define CLI_TABLES_FIRST 4096u
#define CLI_TABLES_PATTERN_FIRST 256u
#define CLI_TABLES_GROUPS 8u
#define CLI_TABLES_LETTERS 1024u

// the patterns a list a table fills is read by: the rows kept by their cell in one column, and each cell taken or left
// out
enum
{
    CLI_TABLES_ROWS_MATCH,
    CLI_TABLES_ROWS_SKIP,
    CLI_TABLES_MATCH,
    CLI_TABLES_SKIP,
    CLI_TABLES_PATTERNS
};

// A list a table fills, as its table is read: the tree its node lies in and the node; the places of the columns its
// words are taken from and of the column its rows are kept by, CLI_DEFS_CELLS for none; its patterns, each read where
// it is given; where its words are written, and whether one is
typedef struct
{
    const CliJson *json;
    unsigned int list;
    unsigned int column[CLI_DEFS_CELLS];
    unsigned int columns;
    unsigned int rows_by;
    CliRegexp pattern[CLI_TABLES_PATTERNS];
    int given[CLI_TABLES_PATTERNS];
    VerbaLinea *out;
    int written;
} CliTableList;

// the tree the tables are read from, as the window opens it
static char s_tree[CLI_PATH_MAX_LEN];
static int s_has_tree;
static CliLock s_lock = CLI_LOCK_INIT;

void cli_plugins_set_tree(const char *root)
{
    cli_lock_take(&s_lock);
    memor.cpy(&(MemoriaCfg){
        .dst = s_tree, .src = root, .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    s_has_tree = 1;
    cli_lock_give(&s_lock);
}

int cli_plugins_tree(char *tree)
{
    cli_lock_take(&s_lock);
    const int has = s_has_tree;
    memor.cpy(&(MemoriaCfg){.dst = tree, .src = s_tree, .bytes = sizeof(s_tree)});
    cli_lock_give(&s_lock);
    return has;
}

// `length` bytes of `text` put into `line`, `name` for each `{name}` in them
static void cli_tables_named(VerbaLinea *line, const char *text, unsigned long long length, const char *name)
{
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        if (((at + 6ull) <= length) &&
            (memor.cmp(&(MemoriaCfg){.src = text + at, .other = "{name}", .bytes = 6u}) == 0))
        {
            mmgr_line_put(line, name);
            at += 5ull;
        }
        else
        {
            mmgr_line_ch(line, text[at]);
        }
    }
}

// the table the string `table` of `json` names, `{name}` in it `name`, as a path in the tree `tree` into `path`, sized
// for CLI_PATH_MAX_LEN: 1, or 0 where it names none or the path does not fit
static int cli_tables_path(const CliJson *json, unsigned int table, const char *tree, const char *name, char *path)
{
    unsigned long long length = 0ull;
    const char *const text = cli_json_string(json, table, &length);
    VerbaLinea line = {path, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, tree);
    mmgr_line_ch(&line, '/');
    cli_tables_named(&line, (text != NULL) ? text : "", (text != NULL) ? length : 0ull, name);
    return (text != NULL) && (mmgr_line_finish(&line) != 0ull);
}

// `length` bytes of `text`, its letters put into `letters`, sized for CLI_TABLES_LETTERS, their count into `count`,
// matched by `pattern` from its first letter on: 1 with the first group's span, or the whole match's where it has no
// group or the group took no part, into `from` and `to`; 0 where it matches nowhere
static int cli_tables_found(const CliRegexp *pattern, const char *text, unsigned long long length,
                            unsigned int *letters, unsigned long long *count, unsigned long long *from,
                            unsigned long long *to)
{
    *count = cli_regexp_letters(text, length, letters, CLI_TABLES_LETTERS);
    CliRegexpSpan span[CLI_TABLES_GROUPS];
    const unsigned long long last = pattern->anchored ? 0ull : *count;
    for (unsigned long long at = 0ull; at <= last; at += 1ull)
    {
        if (cli_regexp_match_into(pattern, letters, *count, at, span))
        {
            const unsigned int group = ((pattern->groups != 0u) && span[1].set) ? 1u : 0u;
            *from = span[group].from;
            *to = span[group].to;
            return 1;
        }
    }
    return 0;
}

// whether `pattern` matches anywhere in `length` bytes of `text`
static int cli_tables_matches(const CliRegexp *pattern, const char *text, unsigned long long length)
{
    unsigned int letters[CLI_TABLES_LETTERS];
    unsigned long long count = 0ull;
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    return cli_tables_found(pattern, text, length, letters, &count, &from, &to);
}

// the pattern `source` read into `pattern` in `room`: 1, or 0 with why given where it does not read or has more groups
// than CLI_TABLES_GROUPS holds
static int cli_tables_pattern_read(CliRegexp *pattern, CliRegexpRoom *room, const char *source, VerbaLinea *err_msg)
{
    cli_regexp_roomed(pattern, room);
    if (!cli_regexp_read(pattern, source, "", err_msg))
    {
        return 0;
    }
    if (pattern->groups >= CLI_TABLES_GROUPS)
    {
        mmgr_line_put(err_msg, "a table's pattern with more groups than orior_cli supports");
        return 0;
    }
    return 1;
}

// the word one cell gives a list, where its patterns take it, written after those before
static void cli_tables_word(CliTableList *list, const char *cell, unsigned long long length)
{
    unsigned int letters[CLI_TABLES_LETTERS];
    unsigned long long count = 0ull;
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    if (list->given[CLI_TABLES_SKIP] && cli_tables_matches(&list->pattern[CLI_TABLES_SKIP], cell, length))
    {
        return;
    }
    if (list->given[CLI_TABLES_MATCH])
    {
        if (!cli_tables_found(&list->pattern[CLI_TABLES_MATCH], cell, length, letters, &count, &from, &to))
        {
            return;
        }
    }
    else
    {
        count = cli_regexp_letters(cell, length, letters, CLI_TABLES_LETTERS);
        to = count;
    }
    char word[CLI_NAME_MAX_LEN];
    VerbaLinea line = {word, sizeof(word), 0ull};
    for (unsigned long long at = from; at < to; at += 1ull)
    {
        cli_utf8_put(&line, letters[at]);
    }
    const unsigned long long size = mmgr_line_finish(&line);
    if (size == 0ull)
    {
        return;
    }
    mmgr_line_put(list->out, list->written ? "," : "");
    cli_json_quoted(list->out, word, size);
    list->written = 1;
}

// the place of the column `name` names among the cells of `line`, CLI_DEFS_CELLS where none is it
static unsigned int cli_tables_column(const CliDefsLine *line, const char *name, unsigned long long length)
{
    for (unsigned int at = 0u; at < line->count; at += 1u)
    {
        if ((line->length[at] == length) &&
            (memor.cmp(&(MemoriaCfg){.src = line->cell[at], .other = name, .bytes = (size_t)length}) == 0))
        {
            return at;
        }
    }
    return CLI_DEFS_CELLS;
}

// one line of a list's table: the line naming the columns read for their places, and each row after it kept or not
// and its cells in the list's columns given as words
static void cli_tables_line(void *context, const CliDefsLine *line, int named)
{
    CliTableList *const list = (CliTableList *)context;
    const CliJson *const json = list->json;
    if (named)
    {
        const unsigned int columns = cli_json_member(json, list->list, "columns");
        for (unsigned int item = cli_json_is(json, columns, CFG_JSON_ARRAY) ? json->node[columns].first : CLI_JSON_NONE;
             (item != CLI_JSON_NONE) && (list->columns < CLI_DEFS_CELLS); item = json->node[item].next)
        {
            unsigned long long length = 0ull;
            const char *const name = cli_json_string(json, item, &length);
            const unsigned int place = (name != NULL) ? cli_tables_column(line, name, length) : CLI_DEFS_CELLS;
            if (place != CLI_DEFS_CELLS)
            {
                list->column[list->columns] = place;
                list->columns += 1u;
            }
        }
        unsigned long long length = 0ull;
        const char *const by =
            cli_json_string(json, cli_json_member(json, cli_json_member(json, list->list, "rows"), "column"), &length);
        list->rows_by = (by != NULL) ? cli_tables_column(line, by, length) : CLI_DEFS_CELLS;
        return;
    }
    const int kept_by_rows = list->given[CLI_TABLES_ROWS_MATCH] || list->given[CLI_TABLES_ROWS_SKIP];
    if (kept_by_rows)
    {
        const int has = list->rows_by < line->count;
        const char *const cell = has ? line->cell[list->rows_by] : "";
        const unsigned long long length = has ? line->length[list->rows_by] : 0ull;
        if ((list->given[CLI_TABLES_ROWS_MATCH] &&
             !cli_tables_matches(&list->pattern[CLI_TABLES_ROWS_MATCH], cell, length)) ||
            (list->given[CLI_TABLES_ROWS_SKIP] &&
             cli_tables_matches(&list->pattern[CLI_TABLES_ROWS_SKIP], cell, length)))
        {
            return;
        }
    }
    for (unsigned int at = 0u; at < list->columns; at += 1u)
    {
        if (list->column[at] < line->count)
        {
            cli_tables_word(list, line->cell[list->column[at]], line->length[list->column[at]]);
        }
    }
}

// the list at `list` in `json` a table fills, its words written into `out` as a JSON array, none where its table does
// not read: 1, or 0 with why given where one of its patterns does not read
static int cli_tables_list_written(const CliJson *json, unsigned int list, const char *tree, const char *name,
                                   VerbaLinea *out, VerbaLinea *err_msg)
{
    char path[CLI_PATH_MAX_LEN];
    CliRegexpRoom *const room =
        g_cli_cells->persistent_buf_alloc(CLI_TABLES_PATTERNS * sizeof(CliRegexpRoom), &g_cli_plugins_cells);
    if (room == NULL)
    {
        mmgr_line_put(err_msg, "a table's patterns could not be given a cell");
        return 0;
    }
    CliTableList read = {.json = json, .list = list, .rows_by = CLI_DEFS_CELLS, .out = out};
    const unsigned int rows = cli_json_member(json, list, "rows");
    const char *const sources[CLI_TABLES_PATTERNS] = {cli_json_string(json, cli_json_member(json, rows, "match"), NULL),
                                                      cli_json_string(json, cli_json_member(json, rows, "skip"), NULL),
                                                      cli_json_string(json, cli_json_member(json, list, "match"), NULL),
                                                      cli_json_string(json, cli_json_member(json, list, "skip"), NULL)};
    int read_whole = 1;
    for (unsigned int place = 0u; read_whole && (place < CLI_TABLES_PATTERNS); place += 1u)
    {
        read.given[place] = sources[place] != NULL;
        read_whole =
            !read.given[place] || cli_tables_pattern_read(&read.pattern[place], &room[place], sources[place], err_msg);
    }
    mmgr_line_ch(out, '[');
    if (read_whole && cli_tables_path(json, cli_json_member(json, list, "table"), tree, name, path))
    {
        (void)cli_defs_lines(path, cli_tables_line, &read);
    }
    mmgr_line_ch(out, ']');
    g_cli_cells->persistent_buf_release(room, &g_cli_plugins_cells);
    return read_whole;
}

// the grammar at `grammar` in `json` written into `out` with each list a table fills read from it: 1, or 0 with why
// given
static int cli_tables_lists_written(const CliJson *json, unsigned int grammar, const char *tree, const char *name,
                                    VerbaLinea *out, VerbaLinea *err_msg)
{
    int whole = 1;
    mmgr_line_ch(out, '{');
    for (unsigned int member = json->node[grammar].first; whole && (member != CLI_JSON_NONE);
         member = json->node[member].next)
    {
        const char *const key = json->text + json->node[member].key;
        mmgr_line_put(out, (member != json->node[grammar].first) ? "," : "");
        cli_json_quoted(out, key, cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}));
        mmgr_line_ch(out, ':');
        if (cli_json_is(json, member, CFG_JSON_OBJECT) && (cli_json_member(json, member, "table") != CLI_JSON_NONE))
        {
            whole = cli_tables_list_written(json, member, tree, name, out, err_msg);
        }
        else
        {
            cli_json_write(json, member, out, 0u, 0);
        }
    }
    mmgr_line_ch(out, '}');
    return whole;
}

// the words of the list `name` of the grammar at `grammar` in `json`, each escaped, the longest first, between `before`
// and `after`, written into `source` as one pattern: how many words it takes
static unsigned int cli_tables_words_pattern(const CliJson *json, unsigned int grammar, const char *name,
                                             const char *before, const char *after, VerbaLinea *source)
{
    const unsigned int list = cli_json_member(json, grammar, name);
    if (!cli_json_is(json, list, CFG_JSON_ARRAY))
    {
        return 0u;
    }
    unsigned long long longest = 0ull;
    for (unsigned int item = json->node[list].first; item != CLI_JSON_NONE; item = json->node[item].next)
    {
        unsigned long long length = 0ull;
        longest = ((cli_json_string(json, item, &length) != NULL) && (length > longest)) ? length : longest;
    }
    mmgr_line_put(source, before);
    mmgr_line_put(source, "(?:");
    unsigned int taken = 0u;
    for (unsigned long long size = longest; size != 0ull; size -= 1ull)
    {
        for (unsigned int item = json->node[list].first; item != CLI_JSON_NONE; item = json->node[item].next)
        {
            unsigned long long length = 0ull;
            const char *const word = cli_json_string(json, item, &length);
            if ((word == NULL) || (length != size))
            {
                continue;
            }
            mmgr_line_put(source, (taken != 0u) ? "|" : "");
            for (unsigned long long at = 0ull; at < length; at += 1ull)
            {
                const char *const special = "\\^$.|?*+()[]{}";
                for (const char *one = special; *one != '\0'; one += 1)
                {
                    if (*one == word[at])
                    {
                        mmgr_line_ch(source, '\\');
                        break;
                    }
                }
                mmgr_line_ch(source, word[at]);
            }
            taken += 1u;
        }
    }
    mmgr_line_ch(source, ')');
    mmgr_line_put(source, after);
    return taken;
}

// one rule of a state written into `out` after `written` rules before it: one whose pattern is a word list given the
// list's words as its pattern, and left out where the list has none. Whether it is written; 0 with why given in
// `failed` where its pattern could not be given a cell
static int cli_tables_rule_written(const CliJson *json, unsigned int grammar, unsigned int rule, unsigned int written,
                                   VerbaLinea *out, int *failed)
{
    const unsigned int pattern = cli_json_item(json, rule, 0u);
    const char *const words = cli_json_string(json, cli_json_member(json, pattern, "words"), NULL);
    if (words == NULL)
    {
        mmgr_line_put(out, (written != 0u) ? "," : "");
        cli_json_write(json, rule, out, 0u, 0);
        return 1;
    }
    const char *const before = cli_json_string(json, cli_json_member(json, pattern, "before"), NULL);
    const char *const after = cli_json_string(json, cli_json_member(json, pattern, "after"), NULL);
    VerbaLinea source;
    if (!cli_growing_line_made(&source, CLI_TABLES_PATTERN_FIRST))
    {
        *failed = 1;
        return 0;
    }
    const unsigned int taken = cli_tables_words_pattern(json, grammar, words, (before != NULL) ? before : "",
                                                        (after != NULL) ? after : "", &source);
    if (taken != 0u)
    {
        mmgr_line_put(out, (written != 0u) ? ",[" : "[");
        cli_json_quoted(out, source.out, source.at);
        for (unsigned int item = json->node[pattern].next; item != CLI_JSON_NONE; item = json->node[item].next)
        {
            mmgr_line_ch(out, ',');
            cli_json_write(json, item, out, 0u, 0);
        }
        mmgr_line_ch(out, ']');
    }
    g_cli_cells->persistent_buf_release(source.out, NULL);
    return taken != 0u;
}

// the grammar at `grammar` in `json`, its lists filled, written into `out` with each rule whose pattern is a word list
// given that list's words: 1, or 0 with why given
static int cli_tables_rules_written(const CliJson *json, unsigned int grammar, VerbaLinea *out, VerbaLinea *err_msg)
{
    int failed = 0;
    mmgr_line_ch(out, '{');
    for (unsigned int member = json->node[grammar].first; member != CLI_JSON_NONE; member = json->node[member].next)
    {
        const char *const key = json->text + json->node[member].key;
        mmgr_line_put(out, (member != json->node[grammar].first) ? "," : "");
        cli_json_quoted(out, key, cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}));
        mmgr_line_ch(out, ':');
        if (!cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "tokenizer", .cap = CLI_CELLS_POOL}) ||
            !cli_json_is(json, member, CFG_JSON_OBJECT))
        {
            cli_json_write(json, member, out, 0u, 0);
            continue;
        }
        mmgr_line_ch(out, '{');
        for (unsigned int state = json->node[member].first; state != CLI_JSON_NONE; state = json->node[state].next)
        {
            const char *const name = json->text + json->node[state].key;
            mmgr_line_put(out, (state != json->node[member].first) ? "," : "");
            cli_json_quoted(out, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
            mmgr_line_put(out, ":[");
            unsigned int written = 0u;
            for (unsigned int rule = cli_json_is(json, state, CFG_JSON_ARRAY) ? json->node[state].first : CLI_JSON_NONE;
                 rule != CLI_JSON_NONE; rule = json->node[rule].next)
            {
                if (cli_tables_rule_written(json, grammar, rule, written, out, &failed))
                {
                    written += 1u;
                }
            }
            mmgr_line_ch(out, ']');
        }
        mmgr_line_ch(out, '}');
    }
    mmgr_line_ch(out, '}');
    if (failed)
    {
        mmgr_line_put(err_msg, "a pattern of a word list could not be given a cell");
    }
    return !failed;
}

int cli_plugins_grammar_whole(const CliJson *json, unsigned int grammar, const char *tree, const char *name,
                              VerbaLinea *out, VerbaLinea *err_msg)
{
    VerbaLinea lists;
    if (!cli_growing_line_made(&lists, CLI_TABLES_FIRST))
    {
        mmgr_line_put(err_msg, "a grammar could not be given a cell to be made whole in");
        return 0;
    }
    int whole = cli_tables_lists_written(json, grammar, tree, name, &lists, err_msg) && mmgr_line_ok(&lists);
    CliJson filled;
    cli_json_begun(&filled, NULL, 0u, NULL, 0ull);
    const unsigned int filled_grammar =
        whole ? cli_json_taken(&filled, lists.out, lists.at, 0u, 0ull, err_msg) : CLI_JSON_NONE;
    whole = (filled_grammar != CLI_JSON_NONE) && cli_tables_rules_written(&filled, filled_grammar, out, err_msg);
    cli_json_given(&filled);
    g_cli_cells->persistent_buf_release(lists.out, NULL);
    return whole;
}

// One row of a folder's table, matched against a plugin's `unless`: the place of its column, the pattern, and whether a
// row matched
typedef struct
{
    const CliJson *json;
    unsigned int unless;
    unsigned int column;
    const CliRegexp *pattern;
    int matched;
} CliTablesUnless;

static void cli_tables_unless_line(void *context, const CliDefsLine *line, int named)
{
    CliTablesUnless *const unless = (CliTablesUnless *)context;
    if (named)
    {
        unsigned long long length = 0ull;
        const char *const by =
            cli_json_string(unless->json, cli_json_member(unless->json, unless->unless, "column"), &length);
        unless->column = (by != NULL) ? cli_tables_column(line, by, length) : CLI_DEFS_CELLS;
        return;
    }
    unless->matched = unless->matched ||
                      ((unless->pattern != NULL) && (unless->column < line->count) &&
                       cli_tables_matches(unless->pattern, line->cell[unless->column], line->length[unless->column]));
}

int cli_plugins_folder_stands(const CliJson *json, unsigned int each, const char *tree, const char *name)
{
    const unsigned int except = cli_json_member(json, each, "except");
    for (unsigned int item = cli_json_is(json, except, CFG_JSON_ARRAY) ? json->node[except].first : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        const char *const left_out = cli_json_string(json, item, NULL);
        if ((left_out != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = left_out, .other = name, .cap = CLI_CELLS_POOL}))
        {
            return 0;
        }
    }
    char folder[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    if (!cli_tables_path(json, cli_json_member(json, each, "folder"), tree, name, folder) ||
        !cli_tables_path(json, cli_json_member(json, each, "table"), folder, name, path))
    {
        return 0;
    }
    const unsigned int unless = cli_json_member(json, each, "unless");
    const char *const source = cli_json_string(json, cli_json_member(json, unless, "match"), NULL);
    CliRegexpRoom *const room =
        (source != NULL) ? g_cli_cells->persistent_buf_alloc(sizeof(CliRegexpRoom), &g_cli_plugins_cells) : NULL;
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    CliRegexp pattern;
    const int read = (room != NULL) && cli_tables_pattern_read(&pattern, room, source, &err_msg);
    CliTablesUnless matched = {json, unless, CLI_DEFS_CELLS, read ? &pattern : NULL, 0};
    const int stands =
        ((source == NULL) || read) && cli_defs_lines(path, cli_tables_unless_line, &matched) && !matched.matched;
    g_cli_cells->persistent_buf_release(room, &g_cli_plugins_cells);
    return stands;
}
