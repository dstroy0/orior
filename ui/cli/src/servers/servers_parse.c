// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_parse.c: the parse of a file the editor has open: orior's own where the file is Python or JavaScript, and
// its server's semantic tokens and folding ranges where its server gives them, none where neither reads it. A parse
// is kept, by the file's key with its text's hash, until the file's text changes, and is read one at a time, so that a
// parse asked for again meanwhile waits for it and is answered from what was kept. Orior's own parse is counted, then
// read into storage of what it counted; a server's into storage of what its answers give
#include "../command_line/cells.h"
#include "../inspect/inspect.h"
#include "servers_internal.h"

// the most token types a server's legend names
#define CLI_SERVERS_LEGEND 256u

// The parse's storage, each part in a cell of its count, given back with the parsing lock: the runs, each line's first
// run and how many it has, the folds by their first and last lines, and the spans
typedef struct
{
    CliInspectRun *run;
    unsigned int *line_first;
    unsigned int *line_runs;
    unsigned int *fold_first;
    unsigned int *fold_last;
    CliInspectSpan *range;
} CliServersParseRoom;

// the parse's storage, the parse and whether it is orior's own, the text it is of, and what the servers answer, under
// the reading's lock
static CliServersParseRoom s_room;
static CliParsed s_parsed;
static int s_whole;
static char *s_text;
static CliServersReading s_reading = {CLI_LOCK_INIT};

// the parse's storage given back
static void parse_storage_given(void)
{
    cli_cell_given(s_room.run);
    cli_cell_given(s_room.line_first);
    cli_cell_given(s_room.line_runs);
    cli_cell_given(s_room.fold_first);
    cli_cell_given(s_room.fold_last);
    cli_cell_given(s_room.range);
    memor.set(&(MemoriaCfg){.dst = &s_room, .val = 0, .bytes = sizeof(s_room)});
}

// the parsing lock given back with the parse's storage and its text
static void parse_released(void)
{
    parse_storage_given();
    cli_cell_given(s_text);
    s_text = NULL;
    cli_servers_reading_released(&s_reading);
}

// `count` items of `size` bytes each in a cell of their own, NULL where none are or the cells cannot meet them
static void *parse_part_taken(unsigned int count, size_t size, const char *what)
{
    return (count > 0u) ? cli_cell_taken((unsigned long long)count * size, what) : NULL;
}

// the parse's storage taken for `runs` runs, `lines` lines, `folds` folds and `ranges` spans, and the parse made empty
// over it: 1, or 0 where the cells cannot meet it
static int parse_storage_taken(unsigned int runs, unsigned int lines, unsigned int folds, unsigned int ranges)
{
    parse_storage_given();
    s_room.run = parse_part_taken(runs, sizeof(CliInspectRun), "a parse's runs");
    s_room.line_first = parse_part_taken(lines, sizeof(unsigned int), "a parse's lines");
    s_room.line_runs = parse_part_taken(lines, sizeof(unsigned int), "a parse's lines");
    s_room.fold_first = parse_part_taken(folds, sizeof(unsigned int), "a parse's folds");
    s_room.fold_last = parse_part_taken(folds, sizeof(unsigned int), "a parse's folds");
    s_room.range = parse_part_taken(ranges, sizeof(CliInspectSpan), "a parse's spans");
    cli_parsed_begun(&s_parsed, s_room.run, runs, s_room.line_first, s_room.line_runs, lines, s_room.fold_first,
                     s_room.fold_last, folds, s_room.range, ranges);
    return ((runs == 0u) || (s_room.run != NULL)) &&
           ((lines == 0u) || ((s_room.line_first != NULL) && (s_room.line_runs != NULL))) &&
           ((folds == 0u) || ((s_room.fold_first != NULL) && (s_room.fold_last != NULL))) &&
           ((ranges == 0u) || (s_room.range != NULL));
}

// The head of a parse kept: its text's hash, whether it is orior's own, and its lines', runs', folds' and spans' counts
typedef struct
{
    unsigned long long hash;
    unsigned int whole;
    unsigned int lines;
    unsigned int runs;
    unsigned int folds;
    unsigned int ranges;
} CliServersParseHead;

// the bytes a parse kept with `head` takes
static unsigned long long parse_kept_size(const CliServersParseHead *head)
{
    return sizeof(*head) + ((unsigned long long)head->lines * 2ull * sizeof(unsigned int)) +
           ((unsigned long long)head->runs * sizeof(CliInspectRun)) +
           ((unsigned long long)head->folds * 2ull * sizeof(unsigned int)) +
           ((unsigned long long)head->ranges * sizeof(CliInspectSpan));
}

// the hash of a text, FNV-1a over its bytes, to tell whether a parse of it is still the parse of it
static unsigned long long parse_hash_of(const char *text, unsigned long long size)
{
    unsigned long long hash = 14695981039346656037ull;
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        hash = (hash ^ (unsigned char)text[at]) * 1099511628211ull;
    }
    return hash;
}

// `count` items of `size` bytes each copied from `kept` at `*at` into `into`, `*at` past them
static void parse_copied_out(void *into, const char *kept, unsigned long long *at, unsigned int count, size_t size)
{
    if (count > 0u)
    {
        // the kept parse's size was checked against its head, so every part lies inside it
        memor.cpy(&(MemoriaCfg){.dst = into, .src = kept + *at, .bytes = (size_t)((unsigned long long)count * size)});
    }
    *at += (unsigned long long)count * size;
}

// the parse kept for `key` read again where its hash is `hash`: 1, or 0 where none is kept for this text
static int parse_kept_read(const char *key, unsigned long long hash)
{
    cli_lock_take(&g_cli_servers_texts_lock);
    const char *beside = NULL;
    unsigned long long size = 0ull;
    const char *const kept = cli_keyed_get(&g_cli_servers_parses, key, &beside, &size);
    CliServersParseHead head = {0ull, 0u, 0u, 0u, 0u, 0u};
    if ((kept != NULL) && (size >= sizeof(head)))
    {
        memor.cpy(&(MemoriaCfg){.dst = &head, .src = kept, .bytes = sizeof(head)});
    }
    const int found = (kept != NULL) && (size >= sizeof(head)) && (head.hash == hash) &&
                      (size == parse_kept_size(&head)) &&
                      parse_storage_taken(head.runs, head.lines, head.folds, head.ranges);
    if (found)
    {
        unsigned long long at = sizeof(head);
        parse_copied_out(s_room.line_first, kept, &at, head.lines, sizeof(unsigned int));
        parse_copied_out(s_room.line_runs, kept, &at, head.lines, sizeof(unsigned int));
        parse_copied_out(s_room.run, kept, &at, head.runs, sizeof(CliInspectRun));
        parse_copied_out(s_room.fold_first, kept, &at, head.folds, sizeof(unsigned int));
        parse_copied_out(s_room.fold_last, kept, &at, head.folds, sizeof(unsigned int));
        parse_copied_out(s_room.range, kept, &at, head.ranges, sizeof(CliInspectSpan));
        s_parsed.lines = head.lines;
        s_parsed.runs = head.runs;
        s_parsed.folds = head.folds;
        s_parsed.ranges = head.ranges;
        s_whole = (int)head.whole;
    }
    cli_lock_give(&g_cli_servers_texts_lock);
    return found;
}

// `count` items of `size` bytes each from `from` put into `kept` at `*at`, `*at` past them
static void parse_put(char *kept, unsigned long long *at, const void *from, unsigned int count, size_t size)
{
    if (count > 0u)
    {
        // the cell was taken to the size the head gives, so every part fits
        memor.cpy(&(MemoriaCfg){.dst = kept + *at, .src = from, .bytes = (size_t)((unsigned long long)count * size)});
    }
    *at += (unsigned long long)count * size;
}

// the parse kept for `key` with its text's hash, in a cell of its size
static void parse_keep(const char *key, const char *path, unsigned long long hash)
{
    const CliServersParseHead head = {hash,          (unsigned int)s_whole, s_parsed.lines,
                                      s_parsed.runs, s_parsed.folds,        s_parsed.ranges};
    const unsigned long long size = parse_kept_size(&head);
    char *const kept = cli_cell_taken(size, "a parse kept");
    if (kept == NULL)
    {
        return;
    }
    unsigned long long at = 0ull;
    parse_put(kept, &at, &head, 1u, sizeof(head));
    parse_put(kept, &at, s_room.line_first, s_parsed.lines, sizeof(unsigned int));
    parse_put(kept, &at, s_room.line_runs, s_parsed.lines, sizeof(unsigned int));
    parse_put(kept, &at, s_room.run, s_parsed.runs, sizeof(CliInspectRun));
    parse_put(kept, &at, s_room.fold_first, s_parsed.folds, sizeof(unsigned int));
    parse_put(kept, &at, s_room.fold_last, s_parsed.folds, sizeof(unsigned int));
    parse_put(kept, &at, s_room.range, s_parsed.ranges, sizeof(CliInspectSpan));
    cli_lock_take(&g_cli_servers_texts_lock);
    (void)cli_keyed_put(&g_cli_servers_parses, key, path, kept, size);
    cli_lock_give(&g_cli_servers_texts_lock);
    cli_cell_given(kept);
}

// the class a server's semantic token of a type takes, by the type's name, CLI_INSPECT_CLASSES where it takes none
static unsigned int parse_class_of_token(const char *name, unsigned long long size)
{
    static const char *const s_names[] = {
        "namespace", "toolModule",  "crateRoot",        "type",           "class",          "enum",
        "interface", "struct",      "typeParameter",    "builtinType",    "typeAlias",      "union",
        "trait",     "concept",     "parameter",        "constParameter", "property",       "enumMember",
        "event",     "field",       "function",         "method",         "macro",          "procMacro",
        "lifetime",  "selfKeyword", "selfTypeKeyword",  "boolean",        "keyword",        "modifier",
        "comment",   "string",      "regexp",           "character",      "escapeSequence", "formatSpecifier",
        "number",    "operator",    "arithmetic",       "logical",        "bitwise",        "comparison",
        "decorator", "attribute",   "builtinAttribute", "derive",         "deriveHelper",   "punctuation",
        "brace",     "bracket",     "parenthesis",      "comma",          "colon",          "semicolon",
        "dot",       "angle",       "variable",         "label"};
    static const unsigned char s_classes[] = {
        CLI_INSPECT_MODULE,     CLI_INSPECT_MODULE,     CLI_INSPECT_MODULE,     CLI_INSPECT_TYPE,
        CLI_INSPECT_TYPE,       CLI_INSPECT_TYPE,       CLI_INSPECT_TYPE,       CLI_INSPECT_TYPE,
        CLI_INSPECT_TYPE,       CLI_INSPECT_TYPE,       CLI_INSPECT_TYPE,       CLI_INSPECT_TYPE,
        CLI_INSPECT_TYPE,       CLI_INSPECT_TYPE,       CLI_INSPECT_PARAMETER,  CLI_INSPECT_PARAMETER,
        CLI_INSPECT_PROPERTY,   CLI_INSPECT_PROPERTY,   CLI_INSPECT_PROPERTY,   CLI_INSPECT_PROPERTY,
        CLI_INSPECT_FUNCTION,   CLI_INSPECT_FUNCTION,   CLI_INSPECT_PREDEFINED, CLI_INSPECT_PREDEFINED,
        CLI_INSPECT_PREDEFINED, CLI_INSPECT_PREDEFINED, CLI_INSPECT_PREDEFINED, CLI_INSPECT_PREDEFINED,
        CLI_INSPECT_KEYWORD,    CLI_INSPECT_KEYWORD,    CLI_INSPECT_COMMENT,    CLI_INSPECT_STRING,
        CLI_INSPECT_STRING,     CLI_INSPECT_STRING,     CLI_INSPECT_STRING,     CLI_INSPECT_STRING,
        CLI_INSPECT_NUMBER,     CLI_INSPECT_OPERATOR,   CLI_INSPECT_OPERATOR,   CLI_INSPECT_OPERATOR,
        CLI_INSPECT_OPERATOR,   CLI_INSPECT_OPERATOR,   CLI_INSPECT_ATTRIBUTE,  CLI_INSPECT_ATTRIBUTE,
        CLI_INSPECT_ATTRIBUTE,  CLI_INSPECT_ATTRIBUTE,  CLI_INSPECT_ATTRIBUTE,  CLI_INSPECT_DELIMITER,
        CLI_INSPECT_DELIMITER,  CLI_INSPECT_DELIMITER,  CLI_INSPECT_DELIMITER,  CLI_INSPECT_DELIMITER,
        CLI_INSPECT_DELIMITER,  CLI_INSPECT_DELIMITER,  CLI_INSPECT_DELIMITER,  CLI_INSPECT_DELIMITER,
        CLI_INSPECT_NONE,       CLI_INSPECT_NONE};
    _Static_assert(sizeof(s_names) / sizeof(s_names[0]) == sizeof(s_classes), "each token type has its class");
    for (unsigned int at = 0u; at < sizeof(s_classes); at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = s_names[at], .cap = CLI_CELLS_POOL}) == size) &&
            (memor.cmp(&(MemoriaCfg){.src = s_names[at], .other = name, .bytes = (size_t)size}) == 0))
        {
            return s_classes[at];
        }
    }
    return CLI_INSPECT_CLASSES;
}

// `method` asked of `server` about the file `uri`, its answer read into the reading: 1, or 0 with why said
static int parse_asked(CliLsp *server, const char *method, const char *uri, unsigned long long wait, CliLine *result,
                       CliLine *said)
{
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    CliLine params;
    cli_pieces_begun(&pieces, &params, piece, sizeof(piece));
    cli_line_text(&params, "{\"textDocument\":{\"uri\":");
    cli_json_quoted(&params, uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}));
    cli_line_text(&params, "}}");
    cli_servers_answer_begun(&s_reading, result);
    return cli_servers_asked(server, method, &pieces, &params, wait, result, said);
}

// the server's semantic tokens of `path`, `lines` lines, read into the parse's runs, taken for the tokens its answer
// gives: 1, or 0 with why said
static int parse_tokens(CliLsp *server, const char *uri, const unsigned char *legend, unsigned int legends,
                        unsigned int lines, CliLine *said)
{
    CliLine result;
    if (!parse_asked(server, "textDocument/semanticTokens/full", uri, CLI_SERVERS_SEARCHING, &result, said))
    {
        return 0;
    }
    const CliJson *const json = &s_reading.json;
    const unsigned int data = cli_json_member(json, cli_servers_answer_read(&s_reading, &result), "data");
    const unsigned int numbers = cli_json_is(json, data, CFG_JSON_ARRAY) ? json->node[data].count : 0u;
    if (!parse_storage_taken(numbers / 5u, lines, 0u, 0u))
    {
        cli_line_text(said, "a file's parse could not be read: the cells cannot meet it");
        return 0;
    }
    s_parsed.lines = lines;
    memor.set(&(MemoriaCfg){.dst = s_room.line_runs, .val = 0, .bytes = (size_t)lines * sizeof(unsigned int)});
    unsigned long long token[5] = {0ull, 0ull, 0ull, 0ull, 0ull};
    unsigned int taken = 0u;
    unsigned long long line = 0ull;
    unsigned long long col = 0ull;
    for (unsigned int one = (numbers > 0u) ? json->node[data].first : CLI_JSON_NONE; one != CLI_JSON_NONE;
         one = json->node[one].next)
    {
        const unsigned long long none = ~0ull;
        const unsigned long long value = cli_servers_whole(json, one, none);
        if (value == none)
        {
            continue;
        }
        // each number is cut to its low word, as the protocol's unsigned integer is read
        token[taken] = value & 0xffffffffull;
        taken += 1u;
        if (taken < 5u)
        {
            continue;
        }
        taken = 0u;
        if (token[0] > 0ull)
        {
            line += token[0];
            col = 0ull;
        }
        col += token[1];
        if ((token[3] < legends) && (legend[token[3]] != CLI_INSPECT_CLASSES) && (line < s_parsed.lines) &&
            (s_parsed.runs < s_parsed.runs_most))
        {
            if (s_room.line_runs[line] == 0u)
            {
                s_room.line_first[line] = s_parsed.runs;
            }
            s_room.run[s_parsed.runs] =
                (CliInspectRun){(unsigned int)col, (unsigned int)(col + token[2]), legend[token[3]]};
            s_parsed.runs += 1u;
            s_room.line_runs[line] += 1u;
        }
    }
    return 1;
}

// the server's folding ranges of `path` read into the parse's folds, taken for the ranges its answer gives, none where
// it gives none
static void parse_folds(CliLsp *server, const char *uri)
{
    CliLine result;
    char quiet[256];
    CliLine ignored = {quiet, sizeof(quiet), 0ull};
    if (!parse_asked(server, "textDocument/foldingRange", uri, CLI_SERVERS_ASKING, &result, &ignored))
    {
        return;
    }
    const CliJson *const json = &s_reading.json;
    const unsigned int all = cli_servers_answer_read(&s_reading, &result);
    const unsigned int given = cli_json_is(json, all, CFG_JSON_ARRAY) ? json->node[all].count : 0u;
    s_room.fold_first = parse_part_taken(given, sizeof(unsigned int), "a parse's folds");
    s_room.fold_last = parse_part_taken(given, sizeof(unsigned int), "a parse's folds");
    if ((s_room.fold_first == NULL) || (s_room.fold_last == NULL))
    {
        return;
    }
    s_parsed.fold_first = s_room.fold_first;
    s_parsed.fold_last = s_room.fold_last;
    s_parsed.folds_most = given;
    for (unsigned int one = json->node[all].first; (one != CLI_JSON_NONE) && (s_parsed.folds < given);
         one = json->node[one].next)
    {
        const unsigned long long none = ~0ull;
        const unsigned long long first = cli_servers_whole(json, cli_json_member(json, one, "startLine"), none);
        const unsigned long long last = cli_servers_whole(json, cli_json_member(json, one, "endLine"), none);
        if ((first != none) && (last != none))
        {
            // each line is cut to its low word, as the protocol's unsigned integer is read
            s_room.fold_first[s_parsed.folds] = (unsigned int)first;
            s_room.fold_last[s_parsed.folds] = (unsigned int)last;
            s_parsed.folds += 1u;
        }
    }
    s_parsed.folds = cli_inspect_folds_of(s_room.fold_first, s_room.fold_last, s_parsed.folds);
}

// the parse of `path` from its server, the text of `size` bytes it is of: 1, 0 where its server gives none, or -1
// with why said
static int parse_from_server(const char *path, unsigned long long size, CliLine *said)
{
    CliLsp *const server = cli_servers_holding(path);
    if (server == NULL)
    {
        return 0;
    }
    unsigned long long length = 0ull;
    const char *const capabilities = cli_lsp_capabilities(server, &length);
    const CliJson *const json = &s_reading.json;
    const unsigned int can = cli_servers_read(&s_reading, capabilities, length);
    const unsigned int provider = cli_json_member(json, can, "semanticTokensProvider");
    const unsigned int folding = cli_json_member(json, can, "foldingRangeProvider");
    const int folds = (folding != CLI_JSON_NONE) && !cli_json_is(json, folding, CFG_JSON_NULL) &&
                      !cli_json_is(json, folding, CFG_JSON_FALSE);
    if (!cli_json_is(json, provider, CFG_JSON_OBJECT))
    {
        cli_lsp_given(server);
        return 0;
    }
    static unsigned char s_legend[CLI_SERVERS_LEGEND];
    unsigned int legends = 0u;
    const unsigned int types = cli_json_member(json, cli_json_member(json, provider, "legend"), "tokenTypes");
    for (unsigned int one = cli_json_is(json, types, CFG_JSON_ARRAY) ? json->node[types].first : CLI_JSON_NONE;
         (one != CLI_JSON_NONE) && (legends < CLI_SERVERS_LEGEND); one = json->node[one].next)
    {
        unsigned long long name_size = 0ull;
        const char *const name = cli_json_string(json, one, &name_size);
        s_legend[legends] = (name != NULL) ? (unsigned char)parse_class_of_token(name, name_size) : CLI_INSPECT_CLASSES;
        legends += 1u;
    }
    unsigned int lines = 1u;
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        lines += (s_text[at] == '\n') ? 1u : 0u;
    }
    char uri[CLI_PATH_LONGEST];
    cli_servers_uri_of(path, uri);
    if (!parse_tokens(server, uri, s_legend, legends, lines, said))
    {
        cli_lsp_given(server);
        return -1;
    }
    if (folds)
    {
        parse_folds(server, uri);
    }
    cli_lsp_given(server);
    s_whole = 0;
    return 1;
}

// the parse of the file the editor has open at `path` read into the parse, under the parsing lock: 1, 0 where nothing
// reads it, or -1 with why said
static int parse_of(const char *path, CliLine *said)
{
    char key[CLI_PATH_LONGEST];
    cli_servers_key_of(path, key);
    unsigned long long size = 0ull;
    s_text = cli_servers_text_taken(path, &size);
    if (s_text == NULL)
    {
        return 0;
    }
    const unsigned long long hash = parse_hash_of(s_text, size);
    if (parse_kept_read(key, hash))
    {
        return 1;
    }
    const char *const language = cli_inspect_language_of(path);
    int found = 0;
    if (language != NULL)
    {
        // the parse counted with no storage handed it, then read into storage of what it counted
        cli_parsed_begun(&s_parsed, NULL, ~0u, NULL, NULL, ~0u, NULL, NULL, ~0u, NULL, ~0u);
        found = cli_inspect_parse(language, s_text, size, &s_parsed) &&
                parse_storage_taken(s_parsed.runs, s_parsed.lines, s_parsed.folds, s_parsed.ranges) &&
                cli_inspect_parse(language, s_text, size, &s_parsed);
    }
    s_whole = found;
    if (!found)
    {
        found = parse_from_server(path, size, said);
    }
    if (found == 1)
    {
        parse_keep(key, path, hash);
    }
    return found;
}

void cli_servers_forget_tokens(void)
{
    cli_lock_take(&g_cli_servers_texts_lock);
    for (unsigned int at = 0u; at < g_cli_servers_parses.entries; at += 1u)
    {
        CliKeyedEntry *const one = cli_keyed_entry(&g_cli_servers_parses, at);
        CliServersParseHead head = {0ull, 1u, 0u, 0u, 0u, 0u};
        const unsigned long long text_at = (unsigned long long)one->key_size + 1u + one->beside_size + 1u;
        if (one->used && (one->size - text_at - 1u >= sizeof(head)))
        {
            memor.cpy(&(MemoriaCfg){.dst = &head, .src = one->cell + text_at, .bytes = sizeof(head)});
        }
        one->used = one->used && head.whole;
    }
    cli_lock_give(&g_cli_servers_texts_lock);
}

int cli_servers_colors(const char *path, unsigned int from, unsigned int to, CliLine *out, CliLine *said)
{
    cli_servers_reading_held(&s_reading);
    const int found = parse_of(path, said);
    if (found == 1)
    {
        const unsigned int end = ((unsigned long long)to + 1ull < s_parsed.lines) ? to + 1u : s_parsed.lines;
        const unsigned int start = (from < end) ? from : end;
        cli_line_text(out, s_whole ? "{\"whole\":true,\"from\":" : "{\"whole\":false,\"from\":");
        cli_line_decimal(out, start, 1u);
        cli_line_text(out, ",\"lines\":[");
        for (unsigned int line = start; line < end; line += 1u)
        {
            cli_line_text(out, (line > start) ? ",[" : "[");
            for (unsigned int run = 0u; run < s_room.line_runs[line]; run += 1u)
            {
                const CliInspectRun *const one = &s_room.run[s_room.line_first[line] + run];
                cli_line_text(out, (run > 0u) ? ",[" : "[");
                cli_line_decimal(out, one->from, 1u);
                cli_line_character(out, ',');
                cli_line_decimal(out, one->to, 1u);
                cli_line_character(out, ',');
                cli_line_decimal(out, one->class_of, 1u);
                cli_line_character(out, ']');
            }
            cli_line_character(out, ']');
        }
        cli_line_text(out, "],\"folds\":[");
        for (unsigned int fold = 0u; fold < s_parsed.folds; fold += 1u)
        {
            cli_line_text(out, (fold > 0u) ? ",[" : "[");
            cli_line_decimal(out, s_room.fold_first[fold], 1u);
            cli_line_character(out, ',');
            cli_line_decimal(out, s_room.fold_last[fold], 1u);
            cli_line_character(out, ']');
        }
        cli_line_text(out, "]}");
    }
    else if (found == 0)
    {
        cli_line_text(out, "null");
    }
    parse_released();
    return found >= 0;
}

// spans written as `[[from, to]]`, each place `{"line", "col"}`
static void parse_spans_written(const CliInspectSpan *spans, unsigned int count, CliLine *out)
{
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(out, (at > 0u) ? ",[" : "[");
        cli_text_place_written((CliTextPlace){spans[at].from.line, spans[at].from.col}, out);
        cli_line_character(out, ',');
        cli_text_place_written((CliTextPlace){spans[at].to.line, spans[at].to.col}, out);
        cli_line_character(out, ']');
    }
    cli_line_character(out, ']');
}

int cli_servers_spans(const char *path, CliTextPlace from, CliTextPlace to, CliLine *out, CliLine *said)
{
    cli_servers_reading_held(&s_reading);
    const CliInspectPlace start = {from.line, from.col};
    const CliInspectPlace end = {to.line, to.col};
    const int found = parse_of(path, said);
    if ((found == 1) && s_whole)
    {
        // the spans the span asked for lies in, in a cell of one place for each span of the parse
        CliInspectSpan *const held = parse_part_taken(s_parsed.ranges, sizeof(CliInspectSpan), "a selection's spans");
        const unsigned int count =
            (held != NULL) ? cli_inspect_ranges_at(s_room.range, s_parsed.ranges, start, end, held, s_parsed.ranges)
                           : 0u;
        parse_spans_written(held, count, out);
        cli_cell_given(held);
        parse_released();
        return 1;
    }
    parse_released();
    return (found >= 0) && cli_servers_selection(path, from, to, out, said);
}
