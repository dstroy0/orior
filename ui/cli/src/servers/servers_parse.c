// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_parse.c: the parse of a file the editor has open: orior's own where the file is Python or JavaScript, and
// its server's semantic tokens and folding ranges where its server gives them, none where neither reads it or the
// file is given its server as a slice, orior's highlighter coloring it and its server not asked. A parse is cached, by
// the file's key with its text's hash, until the file's text changes, and is read one at a time, so that a parse
// requested for again meanwhile waits for it and is answered from what was cached. Orior's own parse is counted, then
// read into storage of what it counted; a server's into storage of what its answers give
#include "../command_line/cells.h"
#include "../inspect/inspect.h"
#include "servers_internal.h"

// The head of a parse cached: its text's hash, whether it is orior's own, and its lines', runs', folds' and spans'
// counts
typedef struct
{
    unsigned long long hash;
    unsigned int whole;
    unsigned int lines;
    unsigned int runs;
    unsigned int folds;
    unsigned int ranges;
} CliServersParseHead;
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
static void parse_storage_free(void)
{
    g_cli_cells->persistent_buf_release(s_room.run, &g_cli_servers_cells);
    g_cli_cells->persistent_buf_release(s_room.line_first, &g_cli_servers_cells);
    g_cli_cells->persistent_buf_release(s_room.line_runs, &g_cli_servers_cells);
    g_cli_cells->persistent_buf_release(s_room.fold_first, &g_cli_servers_cells);
    g_cli_cells->persistent_buf_release(s_room.fold_last, &g_cli_servers_cells);
    g_cli_cells->persistent_buf_release(s_room.range, &g_cli_servers_cells);
    memor.set(&(MemoriaCfg){.dst = &s_room, .val = 0, .bytes = sizeof(s_room)});
}

// the parsing lock given back with the parse's storage and its text
static void parse_release(void)
{
    parse_storage_free();
    g_cli_cells->persistent_buf_release(s_text, &g_cli_servers_cells);
    s_text = NULL;
    EMBED_CALL(cli_servers_reading_answer_internal.reading_release, CliServersReadingAnswerCtx, .reading = &s_reading);
}

typedef struct
{
    unsigned int count;
    size_t size;
    const char *what;
    unsigned int runs;
    unsigned int lines;
    unsigned int folds;
    unsigned int ranges;
    const CliServersParseHead *head;
    void *into;
    const char *stored;
    unsigned long long *at;
    const char *key;
    unsigned long long hash;
    char *stored_out;
    const void *from;
    const char *path;
    const char *name;
    unsigned long long size_u64;
    CliLsp *server;
    const char *method;
    const char *uri;
    unsigned long long wait;
    CliLspAnswer *answer;
    VerbaLinea *err_msg;
    const unsigned char *legend;
    unsigned int legends;
    const CliInspectSpan *spans;
    VerbaLinea *out;
} ServersParseCtx;
// `count` items of `size` bytes each in a cell of their own, NULL where none are or the cells cannot meet them
EMBED_INLINE void *parse_part_take(const ServersParseCtx *args)
{
    size_t size = args->size;
    return (args->count > 0u)
               ? g_cli_cells->persistent_buf_alloc((unsigned long long)args->count * size, &g_cli_servers_cells)
               : NULL;
}

// the parse's storage taken for `runs` runs, `lines` lines, `folds` folds and `ranges` spans, and the parse made empty
// over it: 1, or 0 where the cells cannot meet it
EMBED_INLINE int parse_storage_take(const ServersParseCtx *args)
{
    unsigned int runs = args->runs;
    unsigned int ranges = args->ranges;
    parse_storage_free();
    s_room.run = EMBED_CALL(parse_part_take, ServersParseCtx, .count = runs, .size = sizeof(CliInspectRun),
                            .what = "a parse's runs");
    s_room.line_first = EMBED_CALL(parse_part_take, ServersParseCtx, .count = args->lines, .size = sizeof(unsigned int),
                                   .what = "a parse's lines");
    s_room.line_runs = EMBED_CALL(parse_part_take, ServersParseCtx, .count = args->lines, .size = sizeof(unsigned int),
                                  .what = "a parse's lines");
    s_room.fold_first = EMBED_CALL(parse_part_take, ServersParseCtx, .count = args->folds, .size = sizeof(unsigned int),
                                   .what = "a parse's folds");
    s_room.fold_last = EMBED_CALL(parse_part_take, ServersParseCtx, .count = args->folds, .size = sizeof(unsigned int),
                                  .what = "a parse's folds");
    s_room.range = EMBED_CALL(parse_part_take, ServersParseCtx, .count = ranges, .size = sizeof(CliInspectSpan),
                              .what = "a parse's spans");
    EMBED_CALL(cli_inspect_parsed.parsed_begin, CliInspectParsedCfg, .parsed = &s_parsed, .runs = s_room.run,
               .runs_most = runs, .line_first = s_room.line_first, .line_runs = s_room.line_runs,
               .lines_most = args->lines, .fold_first = s_room.fold_first, .fold_last = s_room.fold_last,
               .folds_most = args->folds, .ranges = s_room.range, .ranges_most = ranges);
    return ((runs == 0u) || (s_room.run != NULL)) &&
           ((args->lines == 0u) || ((s_room.line_first != NULL) && (s_room.line_runs != NULL))) &&
           ((args->folds == 0u) || ((s_room.fold_first != NULL) && (s_room.fold_last != NULL))) &&
           ((ranges == 0u) || (s_room.range != NULL));
}

// the bytes a parse cached with `head` takes
EMBED_INLINE unsigned long long parse_stored_size(const ServersParseCtx *args)
{
    return sizeof(*args->head) + ((unsigned long long)args->head->lines * 2ull * sizeof(unsigned int)) +
           ((unsigned long long)args->head->runs * sizeof(CliInspectRun)) +
           ((unsigned long long)args->head->folds * 2ull * sizeof(unsigned int)) +
           ((unsigned long long)args->head->ranges * sizeof(CliInspectSpan));
}

unsigned long long cli_servers_hash_get(const CliServersReadingCtx *args)
{
    unsigned long long hash = 14695981039346656037ull;
    for (unsigned long long at = 0ull; at < args->size; at += 1ull)
    {
        hash = (hash ^ (unsigned char)args->text[at]) * 1099511628211ull;
    }
    return hash;
}

// `count` items of `size` bytes each copied from `stored` at `*at` into `into`, `*at` past them
EMBED_INLINE void parse_copied_out(const ServersParseCtx *args)
{
    unsigned long long *at = args->at;
    size_t size = args->size;
    if (args->count > 0u)
    {
        // the cached parse's size was checked against its head, and every part lies inside it
        memor.cpy(&(MemoriaCfg){
            .dst = args->into, .src = args->stored + *at, .bytes = (size_t)((unsigned long long)args->count * size)});
    }
    *at += (unsigned long long)args->count * size;
}

// the parse cached for `key` read again where its hash is `hash`: 1, or 0 where none is cached for this text
EMBED_INLINE int parse_stored_read(const ServersParseCtx *args)
{
    const char *key = args->key;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_servers_texts_lock);
    const char *beside = NULL;
    unsigned long long size = 0ull;
    const char *const stored = EMBED_CALL(cli_keyed_key_beside.get, CliKeyedKeyBesideCfg,
                                          .keyed = &g_cli_servers_parses, .key = key, .beside = &beside, .size = &size);
    CliServersParseHead head = {0ull, 0u, 0u, 0u, 0u, 0u};
    if ((stored != NULL) && (size >= sizeof(head)))
    {
        memor.cpy(&(MemoriaCfg){.dst = &head, .src = stored, .bytes = sizeof(head)});
    }
    const int found = (stored != NULL) && (size >= sizeof(head)) && (head.hash == args->hash) &&
                      (size == EMBED_CALL(parse_stored_size, ServersParseCtx, .head = &head)) &&
                      EMBED_CALL(parse_storage_take, ServersParseCtx, .runs = head.runs, .lines = head.lines,
                                 .folds = head.folds, .ranges = head.ranges);
    if (found)
    {
        unsigned long long at = sizeof(head);
        EMBED_CALL(parse_copied_out, ServersParseCtx, .into = s_room.line_first, .stored = stored, .at = &at,
                   .count = head.lines, .size = sizeof(unsigned int));
        EMBED_CALL(parse_copied_out, ServersParseCtx, .into = s_room.line_runs, .stored = stored, .at = &at,
                   .count = head.lines, .size = sizeof(unsigned int));
        EMBED_CALL(parse_copied_out, ServersParseCtx, .into = s_room.run, .stored = stored, .at = &at,
                   .count = head.runs, .size = sizeof(CliInspectRun));
        EMBED_CALL(parse_copied_out, ServersParseCtx, .into = s_room.fold_first, .stored = stored, .at = &at,
                   .count = head.folds, .size = sizeof(unsigned int));
        EMBED_CALL(parse_copied_out, ServersParseCtx, .into = s_room.fold_last, .stored = stored, .at = &at,
                   .count = head.folds, .size = sizeof(unsigned int));
        EMBED_CALL(parse_copied_out, ServersParseCtx, .into = s_room.range, .stored = stored, .at = &at,
                   .count = head.ranges, .size = sizeof(CliInspectSpan));
        s_parsed.lines = head.lines;
        s_parsed.runs = head.runs;
        s_parsed.folds = head.folds;
        s_parsed.ranges = head.ranges;
        s_whole = (int)head.whole;
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_servers_texts_lock);
    return found;
}

// `count` items of `size` bytes each from `from` put into `stored` at `*at`, `*at` past them
EMBED_INLINE void parse_put(const ServersParseCtx *args)
{
    unsigned long long *at = args->at;
    size_t size = args->size;
    if (args->count > 0u)
    {
        // the cell was taken to the size the head gives, and every part fits
        memor.cpy(&(MemoriaCfg){.dst = args->stored_out + *at,
                                .src = args->from,
                                .bytes = (size_t)((unsigned long long)args->count * size)});
    }
    *at += (unsigned long long)args->count * size;
}

// the parse cached for `key` with its text's hash, in a cell of its size
EMBED_INLINE void parse_store(const ServersParseCtx *args)
{
    const char *key = args->key;
    const CliServersParseHead head = {args->hash,    (unsigned int)s_whole, s_parsed.lines,
                                      s_parsed.runs, s_parsed.folds,        s_parsed.ranges};
    const unsigned long long size = EMBED_CALL(parse_stored_size, ServersParseCtx, .head = &head);
    char *const stored = g_cli_cells->temporary_buf_alloc(size, &g_cli_servers_cells);
    if (stored == NULL)
    {
        return;
    }
    unsigned long long at = 0ull;
    EMBED_CALL(parse_put, ServersParseCtx, .stored_out = stored, .at = &at, .from = &head, .count = 1u,
               .size = sizeof(head));
    EMBED_CALL(parse_put, ServersParseCtx, .stored_out = stored, .at = &at, .from = s_room.line_first,
               .count = s_parsed.lines, .size = sizeof(unsigned int));
    EMBED_CALL(parse_put, ServersParseCtx, .stored_out = stored, .at = &at, .from = s_room.line_runs,
               .count = s_parsed.lines, .size = sizeof(unsigned int));
    EMBED_CALL(parse_put, ServersParseCtx, .stored_out = stored, .at = &at, .from = s_room.run, .count = s_parsed.runs,
               .size = sizeof(CliInspectRun));
    EMBED_CALL(parse_put, ServersParseCtx, .stored_out = stored, .at = &at, .from = s_room.fold_first,
               .count = s_parsed.folds, .size = sizeof(unsigned int));
    EMBED_CALL(parse_put, ServersParseCtx, .stored_out = stored, .at = &at, .from = s_room.fold_last,
               .count = s_parsed.folds, .size = sizeof(unsigned int));
    EMBED_CALL(parse_put, ServersParseCtx, .stored_out = stored, .at = &at, .from = s_room.range,
               .count = s_parsed.ranges, .size = sizeof(CliInspectSpan));
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_servers_texts_lock);
    (void)EMBED_CALL(cli_keyed_key_beside.put, CliKeyedKeyBesideCfg, .keyed_out = &g_cli_servers_parses, .key = key,
                     .beside_value = args->path, .text = stored, .size_value = size);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_servers_texts_lock);
    g_cli_cells->persistent_buf_release(stored, &g_cli_servers_cells);
}

// the class a server's semantic token of a type takes, by the type's name, CLI_INSPECT_CLASSES where it takes none
EMBED_INLINE unsigned int parse_class_of_token(const ServersParseCtx *args)
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
        if ((cellul.len(&(CatenaFinitaCfg){.src = s_names[at], .cap = CLI_CELLS_POOL}) == args->size_u64) &&
            (memor.cmp(&(MemoriaCfg){.src = s_names[at], .other = args->name, .bytes = (size_t)args->size_u64}) == 0))
        {
            return s_classes[at];
        }
    }
    return CLI_INSPECT_CLASSES;
}

// `method` sent to `server` about the file `uri`, its answer handed over in `answer`: 1, or 0 with why given
EMBED_INLINE int parse_request(const ServersParseCtx *args)
{
    CliLsp *server = args->server;
    const char *method = args->method;
    unsigned long long wait = args->wait;
    CliLspAnswer *answer = args->answer;
    VerbaLinea *err_msg = args->err_msg;
    char text[CLI_SERVERS_PARAMS];
    VerbaLinea params = {text, sizeof(text), 0ull};
    mmgr_line_put(&params, "{\"textDocument\":{\"uri\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &params, .text = args->uri,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->uri, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&params, "}}");
    return EMBED_CALL(cli_servers_err_msg_internal.request, CliServersErrMsgCtx, .server = server, .method = method,
                      .params = &params, .wait = wait, .answer = answer, .err_msg = err_msg);
}

// the server's semantic tokens of `path`, `lines` lines, read into the parse's runs, taken for the tokens its answer
// gives: 1, or 0 with why given
EMBED_INLINE int parse_tokens(const ServersParseCtx *args)
{
    unsigned int lines = args->lines;
    CliLspAnswer answer;
    if (!EMBED_CALL(parse_request, ServersParseCtx, .server = args->server,
                    .method = "textDocument/semanticTokens/full", .uri = args->uri, .wait = CLI_SERVERS_SEARCHING,
                    .answer = &answer, .err_msg = args->err_msg))
    {
        return 0;
    }
    const CliJson *const json = &s_reading.json;
    const unsigned int data =
        EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                   .object = EMBED_CALL(cli_servers_reading_answer_internal.answer_read, CliServersReadingAnswerCtx,
                                        .reading = &s_reading, .answer = &answer),
                   .name = "data");
    const unsigned int numbers = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                            .node = data, .kind = CFG_JSON_ARRAY)
                                     ? json->node[data].count
                                     : 0u;
    if (!EMBED_CALL(parse_storage_take, ServersParseCtx, .runs = numbers / 5u, .lines = lines, .folds = 0u,
                    .ranges = 0u))
    {
        mmgr_line_put(args->err_msg, "a file's parse could not be read: the cells cannot meet it");
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
        const unsigned long long value =
            EMBED_CALL(cli_servers_json_node_otherwise_internal.whole, CliServersJsonNodeOtherwiseCtx, .json = json,
                       .node = one, .otherwise = none);
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
        if ((token[3] < args->legends) && (args->legend[token[3]] != CLI_INSPECT_CLASSES) && (line < s_parsed.lines) &&
            (s_parsed.runs < s_parsed.runs_most))
        {
            if (s_room.line_runs[line] == 0u)
            {
                s_room.line_first[line] = s_parsed.runs;
            }
            s_room.run[s_parsed.runs] =
                (CliInspectRun){(unsigned int)col, (unsigned int)(col + token[2]), args->legend[token[3]]};
            s_parsed.runs += 1u;
            s_room.line_runs[line] += 1u;
        }
    }
    return 1;
}

// the server's folding ranges of `path` read into the parse's folds, taken for the ranges its answer gives, none where
// it gives none
EMBED_INLINE void parse_folds(const ServersParseCtx *args)
{
    CliLspAnswer answer;
    char quiet[256];
    VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
    if (!EMBED_CALL(parse_request, ServersParseCtx, .server = args->server, .method = "textDocument/foldingRange",
                    .uri = args->uri, .wait = CLI_SERVERS_REQUEST_TIMEOUT_MS, .answer = &answer, .err_msg = &ignored))
    {
        return;
    }
    const CliJson *const json = &s_reading.json;
    const unsigned int all = EMBED_CALL(cli_servers_reading_answer_internal.answer_read, CliServersReadingAnswerCtx,
                                        .reading = &s_reading, .answer = &answer);
    const unsigned int given = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                          .node = all, .kind = CFG_JSON_ARRAY)
                                   ? json->node[all].count
                                   : 0u;
    s_room.fold_first = EMBED_CALL(parse_part_take, ServersParseCtx, .count = given, .size = sizeof(unsigned int),
                                   .what = "a parse's folds");
    s_room.fold_last = EMBED_CALL(parse_part_take, ServersParseCtx, .count = given, .size = sizeof(unsigned int),
                                  .what = "a parse's folds");
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
        const unsigned long long first =
            EMBED_CALL(cli_servers_json_node_otherwise_internal.whole, CliServersJsonNodeOtherwiseCtx, .json = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = one, .name = "startLine"),
                       .otherwise = none);
        const unsigned long long last =
            EMBED_CALL(cli_servers_json_node_otherwise_internal.whole, CliServersJsonNodeOtherwiseCtx, .json = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = one, .name = "endLine"),
                       .otherwise = none);
        if ((first != none) && (last != none))
        {
            // each line is cut to its low word, as the protocol's unsigned integer is read
            s_room.fold_first[s_parsed.folds] = (unsigned int)first;
            s_room.fold_last[s_parsed.folds] = (unsigned int)last;
            s_parsed.folds += 1u;
        }
    }
    s_parsed.folds = EMBED_CALL(cli_inspect_count.folds_get, CliInspectCountCfg, .first = s_room.fold_first,
                                .last = s_room.fold_last, .count = s_parsed.folds);
}

// the parse of `path` from its server, the text of `size` bytes it is of: 1, 0 where its server gives none or is
// given the file as a slice, which orior's highlighter colors and its server is not asked to, or -1 with why given
EMBED_INLINE int parse_from_server(const ServersParseCtx *args)
{
    const char *path = args->path;
    unsigned long long size = args->size_u64;
    if (EMBED_CALL(cli_servers_path_lsp_internal.slice, CliServersPathLspCtx, .path = path))
    {
        return 0;
    }
    CliLsp *const server = EMBED_CALL(cli_servers_path_lsp_internal.owner_get, CliServersPathLspCtx, .path = path);
    if (server == NULL)
    {
        return 0;
    }
    unsigned long long length = 0ull;
    const char *const capabilities =
        EMBED_CALL(cli_lsp_size_kind.capabilities, CliLspSizeKindCfg, .lsp_read = server, .size = &length);
    const CliJson *const json = &s_reading.json;
    const unsigned int can = EMBED_CALL(cli_servers_reading_internal.read, CliServersReadingCtx, .reading = &s_reading,
                                        .text = capabilities, .size = length);
    const unsigned int provider = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                             .object = can, .name = "semanticTokensProvider");
    const unsigned int folding = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                            .object = can, .name = "foldingRangeProvider");
    const int folds = (folding != CLI_JSON_NONE) &&
                      !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                  .node = folding, .kind = CFG_JSON_NULL) &&
                      !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                  .node = folding, .kind = CFG_JSON_FALSE);
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = provider,
                    .kind = CFG_JSON_OBJECT))
    {
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
        return 0;
    }
    static unsigned char s_legend[CLI_SERVERS_LEGEND];
    unsigned int legends = 0u;
    const unsigned int types = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                               .json_read = json, .object = provider, .name = "legend"),
                                          .name = "tokenTypes");
    for (unsigned int one = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                       .node = types, .kind = CFG_JSON_ARRAY)
                                ? json->node[types].first
                                : CLI_JSON_NONE;
         (one != CLI_JSON_NONE) && (legends < CLI_SERVERS_LEGEND); one = json->node[one].next)
    {
        unsigned long long name_size = 0ull;
        const char *const name = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = json, .node = one, .length_pointer = &name_size);
        s_legend[legends] = (name != NULL) ? (unsigned char)EMBED_CALL(parse_class_of_token, ServersParseCtx,
                                                                       .name = name, .size_u64 = name_size)
                                           : CLI_INSPECT_CLASSES;
        legends += 1u;
    }
    unsigned int lines = 1u;
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        lines += (s_text[at] == '\n') ? 1u : 0u;
    }
    char uri[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_servers_path_uri_internal.uri_get, CliServersPathUriCtx, .path = path, .uri_out = uri);
    if (!EMBED_CALL(parse_tokens, ServersParseCtx, .server = server, .uri = uri, .legend = s_legend, .legends = legends,
                    .lines = lines, .err_msg = args->err_msg))
    {
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
        return -1;
    }
    if (folds)
    {
        EMBED_CALL(parse_folds, ServersParseCtx, .server = server, .uri = uri);
    }
    EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
    s_whole = 0;
    return 1;
}

// the parse of the file the editor has open at `path` read into the parse, under the parsing lock: 1, 0 where nothing
// reads it, or -1 with why given
static int parse_get(const ServersParseCtx *args)
{
    const char *path = args->path;
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_servers_path_key_internal.key_get, CliServersPathKeyCtx, .path = path, .key_out = key);
    unsigned long long size = 0ull;
    s_text = EMBED_CALL(cli_servers_path_size_kind.text_take, CliServersPathSizeKindCfg, .path = path,
                        .size_pointer = &size);
    if (s_text == NULL)
    {
        return 0;
    }
    const unsigned long long hash =
        EMBED_CALL(cli_servers_reading_internal.hash_get, CliServersReadingCtx, .text = s_text, .size = size);
    if (EMBED_CALL(parse_stored_read, ServersParseCtx, .key = key, .hash = hash))
    {
        return 1;
    }
    const char *const language = EMBED_CALL(cli_inspect_table.language_get, CliInspectCfg, .path = path);
    int found = 0;
    if (language != NULL)
    {
        // the parse counted with no storage handed it, then read into storage of what it counted
        EMBED_CALL(cli_inspect_parsed.parsed_begin, CliInspectParsedCfg, .parsed = &s_parsed, .runs = NULL,
                   .runs_most = ~0u, .line_first = NULL, .line_runs = NULL, .lines_most = ~0u, .fold_first = NULL,
                   .fold_last = NULL, .folds_most = ~0u, .ranges = NULL, .ranges_most = ~0u);
        found = EMBED_CALL(cli_inspect_language_text.parse, CliInspectLanguageTextCfg, .language = language,
                           .text = s_text, .size = size, .parsed = &s_parsed) &&
                EMBED_CALL(parse_storage_take, ServersParseCtx, .runs = s_parsed.runs, .lines = s_parsed.lines,
                           .folds = s_parsed.folds, .ranges = s_parsed.ranges) &&
                EMBED_CALL(cli_inspect_language_text.parse, CliInspectLanguageTextCfg, .language = language,
                           .text = s_text, .size = size, .parsed = &s_parsed);
    }
    s_whole = found;
    if (!found)
    {
        found =
            EMBED_CALL(parse_from_server, ServersParseCtx, .path = path, .size_u64 = size, .err_msg = args->err_msg);
    }
    if (found == 1)
    {
        EMBED_CALL(parse_store, ServersParseCtx, .key = key, .path = path, .hash = hash);
    }
    return found;
}

void cli_servers_forget_tokens(void)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_servers_texts_lock);
    for (unsigned int at = 0u; at < g_cli_servers_parses.entries; at += 1u)
    {
        CliKeyedEntry *const one =
            EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = &g_cli_servers_parses, .at = at);
        CliServersParseHead head = {0ull, 1u, 0u, 0u, 0u, 0u};
        const unsigned long long text_at = (unsigned long long)one->key_size + 1u + one->beside_size + 1u;
        if (one->used && (one->size - text_at - 1u >= sizeof(head)))
        {
            memor.cpy(&(MemoriaCfg){.dst = &head, .src = one->cell + text_at, .bytes = sizeof(head)});
        }
        one->used = one->used && head.whole;
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_servers_texts_lock);
}

int cli_servers_colors(const CliServersPathOutErrMsgFromToCfg *args)
{
    EMBED_CALL(cli_servers_reading_answer_internal.reading_lock, CliServersReadingAnswerCtx, .reading = &s_reading);
    const int found = EMBED_CALL(parse_get, ServersParseCtx, .path = args->path, .err_msg = args->err_msg);
    if (found == 1)
    {
        const unsigned int end =
            ((unsigned long long)args->to_u32 + 1ull < s_parsed.lines) ? args->to_u32 + 1u : s_parsed.lines;
        const unsigned int start = (args->from_u32 < end) ? args->from_u32 : end;
        mmgr_line_put(args->out, s_whole ? "{\"whole\":true,\"from\":" : "{\"whole\":false,\"from\":");
        mmgr_line_decimal(args->out, start, 1u);
        mmgr_line_put(args->out, ",\"lines\":[");
        for (unsigned int line = start; line < end; line += 1u)
        {
            mmgr_line_put(args->out, (line > start) ? ",[" : "[");
            for (unsigned int run = 0u; run < s_room.line_runs[line]; run += 1u)
            {
                const CliInspectRun *const one = &s_room.run[s_room.line_first[line] + run];
                mmgr_line_put(args->out, (run > 0u) ? ",[" : "[");
                mmgr_line_decimal(args->out, one->from, 1u);
                mmgr_line_ch(args->out, ',');
                mmgr_line_decimal(args->out, one->to, 1u);
                mmgr_line_ch(args->out, ',');
                mmgr_line_decimal(args->out, one->class_of, 1u);
                mmgr_line_ch(args->out, ']');
            }
            mmgr_line_ch(args->out, ']');
        }
        mmgr_line_put(args->out, "],\"folds\":[");
        for (unsigned int fold = 0u; fold < s_parsed.folds; fold += 1u)
        {
            mmgr_line_put(args->out, (fold > 0u) ? ",[" : "[");
            mmgr_line_decimal(args->out, s_room.fold_first[fold], 1u);
            mmgr_line_ch(args->out, ',');
            mmgr_line_decimal(args->out, s_room.fold_last[fold], 1u);
            mmgr_line_ch(args->out, ']');
        }
        mmgr_line_put(args->out, "]}");
    }
    else if (found == 0)
    {
        mmgr_line_put(args->out, "null");
    }
    parse_release();
    return found >= 0;
}

// spans written as `[[from, to]]`, each place `{"line", "col"}`
EMBED_INLINE void parse_spans_write(const ServersParseCtx *args)
{
    VerbaLinea *out = args->out;
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? ",[" : "[");
        EMBED_CALL(cli_edits_place.text_place_write, CliEditsPlaceCfg,
                   .place = (CliTextPlace){args->spans[at].from.line, args->spans[at].from.col}, .out = out);
        mmgr_line_ch(out, ',');
        EMBED_CALL(cli_edits_place.text_place_write, CliEditsPlaceCfg,
                   .place = (CliTextPlace){args->spans[at].to.line, args->spans[at].to.col}, .out = out);
        mmgr_line_ch(out, ']');
    }
    mmgr_line_ch(out, ']');
}

int cli_servers_spans(const CliServersPathOutErrMsgFromCfg *args)
{
    CliTextPlace from = args->from;
    CliTextPlace to = args->to;
    VerbaLinea *out = args->out;
    EMBED_CALL(cli_servers_reading_answer_internal.reading_lock, CliServersReadingAnswerCtx, .reading = &s_reading);
    const CliInspectPlace start = {from.line, from.col};
    const CliInspectPlace end = {to.line, to.col};
    const int found = EMBED_CALL(parse_get, ServersParseCtx, .path = args->path, .err_msg = args->err_msg);
    if ((found == 1) && s_whole)
    {
        // the spans the span requested lies in, in a cell of one place for each span of the parse
        CliInspectSpan *const spans = EMBED_CALL(parse_part_take, ServersParseCtx, .count = s_parsed.ranges,
                                                 .size = sizeof(CliInspectSpan), .what = "a selection's spans");
        const unsigned int count =
            (spans != NULL) ? EMBED_CALL(cli_inspect_ranges.ranges_at, CliInspectRangesCfg, .ranges_read = s_room.range,
                                         .count = s_parsed.ranges, .from = start, .to = end,
                                         .out_inspect_span_pointer = spans, .most = s_parsed.ranges)
                            : 0u;
        EMBED_CALL(parse_spans_write, ServersParseCtx, .spans = spans, .count = count, .out = out);
        g_cli_cells->persistent_buf_release(spans, &g_cli_servers_cells);
        parse_release();
        return 1;
    }
    parse_release();
    return (found >= 0) &&
           EMBED_CALL(cli_servers_path_err_msg_out_internal.selection, CliServersPathErrMsgOutCtx, .path = args->path,
                      .key = args->key, .from = from, .to = to, .out = out, .err_msg = args->err_msg);
}
