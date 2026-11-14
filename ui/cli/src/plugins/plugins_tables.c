// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugins_tables.c: a language plugin's grammar made whole before the colorer stores it: each word list a table of the
// tree fills read from that table, `{name}` in the tables' paths the name of the folder the plugin is for, and each
// rule whose pattern is a word list given the list's words as its pattern; and the folders a plugin is for
#include "../command_line/pool.h"
#include "../editor/defs.h"
#include "../text/regexp.h"
#include "../text/text.h"
#include "plugins.h"

// the patterns a list a table fills is read by: the rows saved by their cell in one column, and each cell taken or left
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
// words are taken from and of the column its rows are saved by, CLI_DEFS_CELLS for none; its patterns, each read where
// it is given; where its words are written, and whether one is
typedef struct
{
    const CliJson *json;
    unsigned int list;
    unsigned int column[CLI_DEFS_CELLS];
    unsigned int columns;
    unsigned int rows_by;
    CliRegexp pattern[CLI_TABLES_PATTERNS];
    int supplied[CLI_TABLES_PATTERNS];
    VerbaLinea *out;
    int written;
} CliTableList;
// the bytes a grammar and a pattern are written in, the max groups a list's pattern has and one, and the max characters
// of a cell matched
#define CLI_TABLES_CAPACITY (4ULL << 20u)
#define CLI_TABLES_PATTERN_CAPACITY (1ULL << 20u)
#define CLI_TABLES_GROUPS 8u
#define CLI_TABLES_CHARACTERS 1024u

// the tree the tables are read from, as the window opens it
static char s_tree[CLI_PATH_MAX_LEN];
static int s_has_tree;

void cli_plugins_set_tree(const CliPluginsRootCfg *args)
{
    memor.cpy(&(MemoriaCfg){.dst = s_tree,
                            .src = args->root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_POOL_BYTES}) + 1u});
    s_has_tree = 1;
}

int cli_plugins_tree(const CliPluginsTreeCfg *args)
{
    const int has = s_has_tree;
    memor.cpy(&(MemoriaCfg){.dst = args->tree_out, .src = s_tree, .bytes = sizeof(s_tree)});
    return has;
}

typedef struct
{
    VerbaLinea *line;
    const char *text;
    unsigned long long length;
    const char *name;
    const CliJson *json;
    unsigned int table;
    const char *tree;
    char *path;
    const CliRegexp *pattern;
    unsigned int *characters;
    unsigned long long *count;
    unsigned long long *from;
    unsigned long long *to;
    CliRegexp *pattern_out;
    CliRegexpWorkState *work_state;
    const char *source;
    VerbaLinea *err_msg;
    CliTableList *list;
    const char *cell;
    const CliDefsLine *line_defs_line_pointer;
    unsigned int list_u32;
    VerbaLinea *out;
    unsigned int grammar;
    const char *before;
    const char *after;
    VerbaLinea *source_verba_linea_pointer;
    unsigned int rule;
    unsigned int written;
    int *failed;
} PluginsTablesCtx;
// `length` bytes of `text` put into `line`, `name` for each `{name}` in them
EMBED_INLINE void cli_tables_by_name(const PluginsTablesCtx *args)
{
    for (unsigned long long at = 0ULL; at < args->length; at += 1ULL)
    {
        if (((at + 6ULL) <= args->length) &&
            (memor.cmp(&(MemoriaCfg){.src = args->text + at, .other = "{name}", .bytes = 6u}) == 0))
        {
            mmgr_line_put(args->line, args->name);
            at += 5ULL;
        }
        else
        {
            mmgr_line_ch(args->line, args->text[at]);
        }
    }
}

// the table the string `table` of `json` names, `{name}` in it `name`, as a path in the tree `tree` into `path`, sized
// for CLI_PATH_MAX_LEN: 1, or 0 where it names none or the path does not fit
EMBED_INLINE int cli_tables_path(const PluginsTablesCtx *args)
{
    const CliJson *json = args->json;
    unsigned long long length = 0ULL;
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                        .json_read = json, .node = args->table, .length_pointer = &length);
    VerbaLinea line = {args->path, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->tree);
    mmgr_line_ch(&line, '/');
    EMBED_CALL(cli_tables_by_name, PluginsTablesCtx, .line = &line, .text = (text != NULL) ? text : "",
               .length = (text != NULL) ? length : 0ULL, .name = args->name);
    return (text != NULL) && (mmgr_line_finish(&line) != 0ULL);
}

// `length` bytes of `text`, its characters put into `characters`, sized for CLI_TABLES_CHARACTERS, their count into `count`,
// matched by `pattern` from its first character on: 1 with the first group's span, or the whole match's where it has no
// group or the group took no part, into `from` and `to`; 0 where it matches nowhere
EMBED_INLINE int cli_tables_find(const PluginsTablesCtx *args)
{
    const char *text = args->text;
    unsigned long long length = args->length;
    unsigned int *characters = args->characters;
    unsigned long long *count = args->count;
    unsigned long long *from = args->from;
    unsigned long long *to = args->to;
    *count = EMBED_CALL(cli_regexp_text.characters, CliRegexpTextCfg, .text = text, .size = length, .characters = characters,
                        .capacity_u64 = CLI_TABLES_CHARACTERS);
    CliRegexpSpan span[CLI_TABLES_GROUPS];
    const unsigned long long last = args->pattern->anchored ? 0ULL : *count;
    for (unsigned long long at = 0ULL; at <= last; at += 1ULL)
    {
        if (EMBED_CALL(cli_regexp_text_u_32s.match_into, CliRegexpTextU32sCfg, .regexp = args->pattern,
                       .text_u32s = characters, .length = *count, .at = at, .groups = span))
        {
            const unsigned int group = ((args->pattern->groups != 0u) && span[1].set) ? 1u : 0u;
            *from = span[group].from;
            *to = span[group].to;
            return 1;
        }
    }
    return 0;
}

// whether `pattern` matches anywhere in `length` bytes of `text`
EMBED_INLINE int cli_tables_matches(const PluginsTablesCtx *args)
{
    unsigned int characters[CLI_TABLES_CHARACTERS];
    unsigned long long count = 0ULL;
    unsigned long long from = 0ULL;
    unsigned long long to = 0ULL;
    return EMBED_CALL(cli_tables_find, PluginsTablesCtx, .pattern = args->pattern, .text = args->text,
                      .length = args->length, .characters = characters, .count = &count, .from = &from, .to = &to);
}

// the pattern `source` read into `pattern` in `capacity`: 1, or 0 with why given where it does not read or has more
// groups than CLI_TABLES_GROUPS holds
EMBED_INLINE int cli_tables_pattern_read(const PluginsTablesCtx *args)
{
    CliRegexpWorkState *work_state = args->work_state;
    const char *source = args->source;
    VerbaLinea *err_msg = args->err_msg;
    EMBED_CALL(cli_regexp_capacity.capacity, CliRegexpWorkStateCfg, .regexp_out = args->pattern_out,
               .work_state = work_state);
    if (!EMBED_CALL(cli_regexp_source.read, CliRegexpSourceCfg, .regexp_out = args->pattern_out, .source = source,
                    .flags = "", .err_msg = err_msg))
    {
        return 0;
    }
    if (args->pattern_out->groups >= CLI_TABLES_GROUPS)
    {
        mmgr_line_put(err_msg, "a table's pattern with more groups than impero supports");
        return 0;
    }
    return 1;
}

// the word one cell gives a list, where its patterns take it, written after those before
EMBED_INLINE void cli_tables_word(const PluginsTablesCtx *args)
{
    unsigned long long length = args->length;
    unsigned int characters[CLI_TABLES_CHARACTERS];
    unsigned long long count = 0ULL;
    unsigned long long from = 0ULL;
    unsigned long long to = 0ULL;
    if (args->list->supplied[CLI_TABLES_SKIP] &&
        EMBED_CALL(cli_tables_matches, PluginsTablesCtx, .pattern = &args->list->pattern[CLI_TABLES_SKIP],
                   .text = args->cell, .length = length))
    {
        return;
    }
    if (args->list->supplied[CLI_TABLES_MATCH])
    {
        if (!EMBED_CALL(cli_tables_find, PluginsTablesCtx, .pattern = &args->list->pattern[CLI_TABLES_MATCH],
                        .text = args->cell, .length = length, .characters = characters, .count = &count, .from = &from,
                        .to = &to))
        {
            return;
        }
    }
    else
    {
        count = EMBED_CALL(cli_regexp_text.characters, CliRegexpTextCfg, .text = args->cell, .size = length,
                           .characters = characters, .capacity_u64 = CLI_TABLES_CHARACTERS);
        to = count;
    }
    char word_text[CLI_NAME_MAX_LEN];
    VerbaLinea line = {word_text, sizeof(word_text), 0ULL};
    for (unsigned long long at = from; at < to; at += 1ULL)
    {
        EMBED_CALL(cli_line_character_internal.utf8_append, CliLineLetterCtx, .line = &line, .character = characters[at]);
    }
    const unsigned long long size = mmgr_line_finish(&line);
    if (size == 0ULL)
    {
        return;
    }
    mmgr_line_put(args->list->out, args->list->written ? "," : "");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->list->out,
               .text = word_text, .length = size);
    args->list->written = 1;
}

// the place of the column `name` names among the cells of `line`, CLI_DEFS_CELLS where none is it
EMBED_INLINE unsigned int cli_tables_column(const PluginsTablesCtx *args)
{
    for (unsigned int at = 0u; at < args->line_defs_line_pointer->count; at += 1u)
    {
        if ((args->line_defs_line_pointer->length[at] == args->length) &&
            (memor.cmp(&(MemoriaCfg){.src = args->line_defs_line_pointer->cell[at],
                                     .other = args->name,
                                     .bytes = (embed_word)args->length}) == 0))
        {
            return at;
        }
    }
    return CLI_DEFS_CELLS;
}

// one line of a list's table: the line naming the columns read for their places, and each row after it saved or not
// and its cells in the list's columns given as words
static void cli_tables_line(const CliDefsLineEachCfg *args)
{
    const CliDefsLine *line = args->line;
    CliTableList *const list = (CliTableList *)args->context;
    const CliJson *const json = list->json;
    if (args->named)
    {
        const unsigned int columns = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                .json_read = json, .object = list->list, .name = "columns");
        for (unsigned int item = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json,
                                            .node = columns, .type = CFG_JSON_ARRAY)
                                     ? json->node[columns].first
                                     : CLI_JSON_NONE;
             (item != CLI_JSON_NONE) && (list->columns < CLI_DEFS_CELLS); item = json->node[item].next)
        {
            unsigned long long length = 0ULL;
            const char *const name = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                                .json_read = json, .node = item, .length_pointer = &length);
            const unsigned int place = (name != NULL)
                                           ? EMBED_CALL(cli_tables_column, PluginsTablesCtx,
                                                        .line_defs_line_pointer = line, .name = name, .length = length)
                                           : CLI_DEFS_CELLS;
            if (place != CLI_DEFS_CELLS)
            {
                list->column[list->columns] = place;
                list->columns += 1u;
            }
        }
        unsigned long long length = 0ULL;
        const char *const by =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                               .json_read = json, .object = list->list, .name = "rows"),
                                          .name = "column"),
                       .length_pointer = &length);
        list->rows_by = (by != NULL) ? EMBED_CALL(cli_tables_column, PluginsTablesCtx, .line_defs_line_pointer = line,
                                                  .name = by, .length = length)
                                     : CLI_DEFS_CELLS;
        return;
    }
    const int saved_by_rows = list->supplied[CLI_TABLES_ROWS_MATCH] || list->supplied[CLI_TABLES_ROWS_SKIP];
    if (saved_by_rows)
    {
        const int has = list->rows_by < line->count;
        const char *const cell = has ? line->cell[list->rows_by] : "";
        const unsigned long long length = has ? line->length[list->rows_by] : 0ULL;
        if ((list->supplied[CLI_TABLES_ROWS_MATCH] &&
             !EMBED_CALL(cli_tables_matches, PluginsTablesCtx, .pattern = &list->pattern[CLI_TABLES_ROWS_MATCH],
                         .text = cell, .length = length)) ||
            (list->supplied[CLI_TABLES_ROWS_SKIP] &&
             EMBED_CALL(cli_tables_matches, PluginsTablesCtx, .pattern = &list->pattern[CLI_TABLES_ROWS_SKIP],
                        .text = cell, .length = length)))
        {
            return;
        }
    }
    for (unsigned int at = 0u; at < list->columns; at += 1u)
    {
        if (list->column[at] < line->count)
        {
            EMBED_CALL(cli_tables_word, PluginsTablesCtx, .list = list, .cell = line->cell[list->column[at]],
                       .length = line->length[list->column[at]]);
        }
    }
}

// the list at `list` in `json` a table fills, its words written into `out` as a JSON array, none where its table does
// not read: 1, or 0 with why given where one of its patterns does not read
EMBED_INLINE int cli_tables_list_write(const PluginsTablesCtx *args)
{
    const CliJson *json = args->json;
    unsigned int list = args->list_u32;
    const char *name = args->name;
    VerbaLinea *out = args->out;
    char path[CLI_PATH_MAX_LEN];
    CliRegexpWorkState *const work_state =
        g_cli_guard->temporary_buf_alloc(CLI_TABLES_PATTERNS * sizeof(CliRegexpWorkState), g_cli_plugins_owner);
    if (work_state == NULL)
    {
        mmgr_line_put(args->err_msg, "a table's patterns could not be given a cell");
        return 0;
    }
    CliTableList read = {.json = json, .list = list, .rows_by = CLI_DEFS_CELLS, .out = out};
    const unsigned int rows = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                         .object = list, .name = "rows");
    const char *const sources[CLI_TABLES_PATTERNS] = {
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = rows, .name = "match"),
                   .length_pointer = NULL),
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = rows, .name = "skip"),
                   .length_pointer = NULL),
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = list, .name = "match"),
                   .length_pointer = NULL),
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = list, .name = "skip"),
                   .length_pointer = NULL)};
    int patterns_read = 1;
    unsigned int place = 0u;
    while (patterns_read && (place < CLI_TABLES_PATTERNS))
    {
        read.supplied[place] = sources[place] != NULL;
        patterns_read =
            !read.supplied[place] ||
            EMBED_CALL(cli_tables_pattern_read, PluginsTablesCtx, .pattern_out = &read.pattern[place],
                       .work_state = &work_state[place], .source = sources[place], .err_msg = args->err_msg);
        place += 1u;
    }
    mmgr_line_ch(out, '[');
    if (patterns_read && EMBED_CALL(cli_tables_path, PluginsTablesCtx, .json = json,
                                    .table = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                        .json_read = json, .object = list, .name = "table"),
                                    .tree = args->tree, .name = name, .path = path))
    {
        (void)EMBED_CALL(cli_defs_path.lines, CliDefsPathCfg, .path = path, .each = cli_tables_line, .context = &read);
    }
    mmgr_line_ch(out, ']');
    g_cli_guard->persistent_buf_release(work_state, g_cli_plugins_owner);
    return patterns_read;
}

// the grammar at `grammar` in `json` written into `out` with each list a table fills read from it: 1, or 0 with why
// given
EMBED_INLINE int cli_tables_lists_write(const PluginsTablesCtx *args)
{
    const CliJson *json = args->json;
    const char *name = args->name;
    VerbaLinea *out = args->out;
    int whole = 1;
    mmgr_line_ch(out, '{');
    for (unsigned int member = json->node[args->grammar].first; whole && (member != CLI_JSON_NONE);
         member = json->node[member].next)
    {
        const char *const key = json->text + json->node[member].key;
        mmgr_line_put(out, (member != json->node[args->grammar].first) ? "," : "");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = key,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_POOL_BYTES}));
        mmgr_line_ch(out, ':');
        if (EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = member,
                       .type = CFG_JSON_OBJECT) &&
            (EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = member,
                        .name = "table") != CLI_JSON_NONE))
        {
            whole = EMBED_CALL(cli_tables_list_write, PluginsTablesCtx, .json = json, .list_u32 = member,
                               .tree = args->tree, .name = name, .out = out, .err_msg = args->err_msg);
        }
        else
        {
            EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = member,
                       .out = out, .indent = 0u, .sorted = 0);
        }
    }
    mmgr_line_ch(out, '}');
    return whole;
}

// the words of the list `name` of the grammar at `grammar` in `json`, each escaped, the longest first, between `before`
// and `after`, written into `source` as one pattern: how many words it takes
EMBED_INLINE unsigned int cli_tables_words_pattern(const PluginsTablesCtx *args)
{
    const CliJson *json = args->json;
    const char *name = args->name;
    const unsigned int list = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                         .object = args->grammar, .name = name);
    if (!EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = list,
                    .type = CFG_JSON_ARRAY))
    {
        return 0u;
    }
    unsigned long long longest = 0ULL;
    for (unsigned int item = json->node[list].first; item != CLI_JSON_NONE; item = json->node[item].next)
    {
        unsigned long long length = 0ULL;
        longest = ((EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                               .node = item, .length_pointer = &length) != NULL) &&
                   (length > longest))
                      ? length
                      : longest;
    }
    mmgr_line_put(args->source_verba_linea_pointer, args->before);
    mmgr_line_put(args->source_verba_linea_pointer, "(?:");
    unsigned int reserved = 0u;
    for (unsigned long long size = longest; size != 0ULL; size -= 1ULL)
    {
        for (unsigned int item = json->node[list].first; item != CLI_JSON_NONE; item = json->node[item].next)
        {
            unsigned long long length = 0ULL;
            const char *const word_text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                                     .json_read = json, .node = item, .length_pointer = &length);
            if ((word_text == NULL) || (length != size))
            {
                continue;
            }
            mmgr_line_put(args->source_verba_linea_pointer, (reserved != 0u) ? "|" : "");
            EMBED_CALL(cli_text_size_set.append_marked_n, CliTextSizeSetCfg, .out = args->source_verba_linea_pointer,
                       .text = word_text, .size = length, .set = "\\^$.|?*+()[]{}", .mark = '\\');
            reserved += 1u;
        }
    }
    mmgr_line_ch(args->source_verba_linea_pointer, ')');
    mmgr_line_put(args->source_verba_linea_pointer, args->after);
    return reserved;
}

// one rule of a state written into `out` after `written` rules before it: one whose pattern is a word list given the
// list's words as its pattern, and left out where the list has none. Whether it is written; 0 with why given in
// `failed` where its pattern could not be given a cell
EMBED_INLINE int cli_tables_rule_write(const PluginsTablesCtx *args)
{
    const CliJson *json = args->json;
    VerbaLinea *out = args->out;
    int *failed = args->failed;
    const unsigned int pattern = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = json, .array = args->rule, .index = 0u);
    const char *const words =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = pattern, .name = "words"),
                   .length_pointer = NULL);
    if (words == NULL)
    {
        mmgr_line_put(out, (args->written != 0u) ? "," : "");
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = args->rule,
                   .out = out, .indent = 0u, .sorted = 0);
        return 1;
    }
    const char *const before =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = pattern, .name = "before"),
                   .length_pointer = NULL);
    const char *const after =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = pattern, .name = "after"),
                   .length_pointer = NULL);
    CliGrownLine source;
    cli_grown_begin(&source, CLI_TABLES_PATTERN_CAPACITY, g_cli_plugins_owner);
    if (source.cell == NULL)
    {
        *failed = 1;
        return 0;
    }
    const unsigned int reserved =
        EMBED_CALL(cli_tables_words_pattern, PluginsTablesCtx, .json = json, .grammar = args->grammar, .name = words,
                   .before = (before != NULL) ? before : "", .after = (after != NULL) ? after : "",
                   .source_verba_linea_pointer = &source.line);
    if (reserved != 0u)
    {
        mmgr_line_put(out, (args->written != 0u) ? ",[" : "[");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = source.cell,
                   .length = cli_grown_size(&source));
        for (unsigned int item = json->node[pattern].next; item != CLI_JSON_NONE; item = json->node[item].next)
        {
            mmgr_line_ch(out, ',');
            EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = item,
                       .out = out, .indent = 0u, .sorted = 0);
        }
        mmgr_line_ch(out, ']');
    }
    cli_grown_end(&source);
    return reserved != 0u;
}

// the grammar at `grammar` in `json`, its lists filled, written into `out` with each rule whose pattern is a word list
// given that list's words: 1, or 0 with why given
EMBED_INLINE int cli_tables_rules_write(const PluginsTablesCtx *args)
{
    const CliJson *json = args->json;
    VerbaLinea *out = args->out;
    int failed = 0;
    mmgr_line_ch(out, '{');
    for (unsigned int member = json->node[args->grammar].first; member != CLI_JSON_NONE;
         member = json->node[member].next)
    {
        const char *const key = json->text + json->node[member].key;
        mmgr_line_put(out, (member != json->node[args->grammar].first) ? "," : "");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = key,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_POOL_BYTES}));
        mmgr_line_ch(out, ':');
        if (!cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "tokenizer", .cap = CLI_POOL_BYTES}) ||
            !EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = member,
                        .type = CFG_JSON_OBJECT))
        {
            EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = member,
                       .out = out, .indent = 0u, .sorted = 0);
            continue;
        }
        mmgr_line_ch(out, '{');
        for (unsigned int state = json->node[member].first; state != CLI_JSON_NONE; state = json->node[state].next)
        {
            const char *const name = json->text + json->node[state].key;
            mmgr_line_put(out, (state != json->node[member].first) ? "," : "");
            EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = name,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}));
            mmgr_line_put(out, ":[");
            unsigned int written = 0u;
            for (unsigned int rule = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg,
                                                .json_read = json, .node = state, .type = CFG_JSON_ARRAY)
                                         ? json->node[state].first
                                         : CLI_JSON_NONE;
                 rule != CLI_JSON_NONE; rule = json->node[rule].next)
            {
                written += EMBED_CALL(cli_tables_rule_write, PluginsTablesCtx, .json = json, .grammar = args->grammar,
                                      .rule = rule, .written = written, .out = out, .failed = &failed)
                               ? 1u
                               : 0u;
            }
            mmgr_line_ch(out, ']');
        }
        mmgr_line_ch(out, '}');
    }
    mmgr_line_ch(out, '}');
    if (failed)
    {
        mmgr_line_put(args->err_msg, "a pattern of a word list could not be given a cell");
    }
    return !failed;
}

int cli_plugins_grammar_resolved(const CliPluginsErrMsgJsonCfg *args)
{
    CliGrownLine lists;
    cli_grown_begin(&lists, CLI_TABLES_CAPACITY, g_cli_plugins_owner);
    if (lists.cell == NULL)
    {
        mmgr_line_put(args->err_msg, "a grammar could not be given a cell to be made whole in");
        return 0;
    }
    int whole = EMBED_CALL(cli_tables_lists_write, PluginsTablesCtx, .json = args->json, .grammar = args->grammar,
                           .tree = args->tree, .name = args->name, .out = &lists.line, .err_msg = args->err_msg) &&
                mmgr_line_ok(&lists.line);
    CliJson filled;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &filled,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    const unsigned int filled_grammar =
        whole ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &filled,
                           .text = lists.cell, .size = cli_grown_size(&lists), .nodes_more = 0u, .text_more = 0ULL,
                           .err_msg = args->err_msg, .owner = g_cli_plugins_owner)
              : CLI_JSON_NONE;
    whole = (filled_grammar != CLI_JSON_NONE) &&
            EMBED_CALL(cli_tables_rules_write, PluginsTablesCtx, .json = &filled, .grammar = filled_grammar,
                       .out = args->out, .err_msg = args->err_msg);
    g_cli_guard->persistent_buf_release(filled.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), filled.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&filled),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    cli_grown_end(&lists);
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

static void cli_tables_unless_line(const CliDefsLineEachCfg *args)
{
    const CliDefsLine *line = args->line;
    CliTablesUnless *const unless = (CliTablesUnless *)args->context;
    if (args->named)
    {
        unsigned long long length = 0ULL;
        const char *const by =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = unless->json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = unless->json, .object = unless->unless, .name = "column"),
                       .length_pointer = &length);
        unless->column = (by != NULL) ? EMBED_CALL(cli_tables_column, PluginsTablesCtx, .line_defs_line_pointer = line,
                                                   .name = by, .length = length)
                                      : CLI_DEFS_CELLS;
        return;
    }
    unless->matched =
        unless->matched || ((unless->pattern != NULL) && (unless->column < line->count) &&
                            EMBED_CALL(cli_tables_matches, PluginsTablesCtx, .pattern = unless->pattern,
                                       .text = line->cell[unless->column], .length = line->length[unless->column]));
}

int cli_plugins_folder_exists(const CliPluginsJsonCfg *args)
{
    const unsigned int except = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                           .json_read = args->json, .object = args->each_u32, .name = "except");
    for (unsigned int item = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg,
                                        .json_read = args->json, .node = except, .type = CFG_JSON_ARRAY)
                                 ? args->json->node[except].first
                                 : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = args->json->node[item].next)
    {
        const char *const left_out = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                                .json_read = args->json, .node = item, .length_pointer = NULL);
        if ((left_out != NULL) &&
            cellul.eq(&(CatenaFinitaCfg){.src = left_out, .other = args->name, .cap = CLI_POOL_BYTES}))
        {
            return 0;
        }
    }
    char folder[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_tables_path, PluginsTablesCtx, .json = args->json,
                    .table = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                        .object = args->each_u32, .name = "folder"),
                    .tree = args->tree, .name = args->name, .path = folder) ||
        !EMBED_CALL(cli_tables_path, PluginsTablesCtx, .json = args->json,
                    .table = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                        .object = args->each_u32, .name = "table"),
                    .tree = folder, .name = args->name, .path = path))
    {
        return 0;
    }
    const unsigned int unless = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                           .json_read = args->json, .object = args->each_u32, .name = "unless");
    const char *const source =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = unless, .name = "match"),
                   .length_pointer = NULL);
    CliRegexpWorkState *const work_state =
        (source != NULL) ? g_cli_guard->temporary_buf_alloc(sizeof(CliRegexpWorkState), g_cli_plugins_owner) : NULL;
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    CliRegexp pattern;
    const int read =
        (work_state != NULL) && EMBED_CALL(cli_tables_pattern_read, PluginsTablesCtx, .pattern_out = &pattern,
                                           .work_state = work_state, .source = source, .err_msg = &err_msg);
    CliTablesUnless matched = {args->json, unless, CLI_DEFS_CELLS, read ? &pattern : NULL, 0};
    const int exists = ((source == NULL) || read) &&
                       EMBED_CALL(cli_defs_path.lines, CliDefsPathCfg, .path = path, .each = cli_tables_unless_line,
                                  .context = &matched) &&
                       !matched.matched;
    g_cli_guard->persistent_buf_release(work_state, g_cli_plugins_owner);
    return exists;
}
