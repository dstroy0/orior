// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_read.c: a text's leaves, the tokens of the grammar of the plugin its extension opens it as or of the language
// named, and a file's declarations read off them: every token whose name holds the segment `declaration` names a
// declaration, its type the rest of the name past that segment, as `identifier.declaration.function` names a function.
// No language is read here but by its plugin's grammar and what its plugin says
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../editor/highlight.h"
#include "../text/text.h"
#include "symbols_internal.h"

// the lines colored in one request
#define CLI_SYMBOLS_LINES 256u

// the longest state a request's last line ends in
#define CLI_SYMBOLS_STATE 1024u

// One file's leaves read: where its declarations go, and the line read
typedef struct
{
    CliDeclared each;
    void *context;
    unsigned int line;
} CliSymbolsLine;

const char *cli_symbols_declares(const CliSymbolsNameCtx *args)
{
    static const char s_segment[] = "declaration.";
    const unsigned long long size = sizeof(s_segment) - 1u;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES});
    unsigned long long from = 0ULL;
    while ((from + size) < length)
    {
        unsigned long long found = 0ULL;
        if (!EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = args->name + from,
                        .length_value = length - from, .required = s_segment, .size = size, .at_pointer = &found))
        {
            return NULL;
        }
        const unsigned long long at = from + found;
        if ((at == 0ULL) || (args->name[at - 1u] == '.'))
        {
            return args->name + at + size;
        }
        from = at + 1ULL;
    }
    return NULL;
}

// each letter decoded as the colorer decodes it, one past U+FFFF taking two columns as UTF-16 counts them
unsigned long long cli_symbols_byte_at(const CliSymbolsSizeCtx *args)
{
    unsigned long long *byte = args->byte;
    unsigned int *at = args->at;
    while ((*at < args->col) && (*byte < args->size))
    {
        const unsigned char lead = (unsigned char)args->line[*byte];
        unsigned int length;
        if (lead < 0x80u)
        {
            length = 1u;
        }
        else if (lead < 0xe0u)
        {
            length = 2u;
        }
        else if (lead < 0xf0u)
        {
            length = 3u;
        }
        else
        {
            length = 4u;
        }
        unsigned int letter;
        if (length == 1u)
        {
            letter = lead;
        }
        else if (length == 2u)
        {
            letter = lead & 0x1fu;
        }
        else if (length == 3u)
        {
            letter = lead & 0x0fu;
        }
        else
        {
            letter = lead & 0x07u;
        }
        for (unsigned int more = 1u; (more < length) && ((*byte + more) < args->size); more += 1u)
        {
            letter = (letter << 6u) | ((unsigned char)args->line[*byte + more] & 0x3fu);
        }
        *at += (letter >= 0x10000u) ? 2u : 1u;
        *byte += length;
    }
    return (*byte < args->size) ? *byte : args->size;
}

// One line's leaves read: each run whose token names a declaration handed on, the run's text its name and the rest
// of its token's name its type
static void cli_symbols_line(const CliLeavesEachCfg *args)
{
    CliSymbolsLine *const read_state = (CliSymbolsLine *)args->context;
    const char *const line = args->leaves->line;
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_POOL_BYTES});
    unsigned long long byte = 0ULL;
    unsigned int at = 0u;
    for (unsigned int run = 0u; run < args->leaves->runs; run += 1u)
    {
        const unsigned long long from =
            EMBED_CALL(cli_symbols_size_internal.byte_at, CliSymbolsSizeCtx, .line = line, .size = size,
                       .col = args->leaves->col[run], .byte = &byte, .at = &at);
        const unsigned long long to =
            ((run + 1u) < args->leaves->runs)
                ? EMBED_CALL(cli_symbols_size_internal.byte_at, CliSymbolsSizeCtx, .line = line, .size = size,
                             .col = args->leaves->col[run + 1u], .byte = &byte, .at = &at)
                : size;
        const char *const type =
            EMBED_CALL(cli_symbols_name_internal.declares, CliSymbolsNameCtx,
                       .name = args->leaves->text + args->leaves->name_at[args->leaves->name[run]]);
        if ((type != NULL) && (type[0] != '\0') && (to > from))
        {
            EMBED_CALL(read_state->each, CliDeclaredCfg, .context = read_state->context, .name = line + from,
                       .length = to - from, .type = type, .line = read_state->line);
        }
    }
    read_state->line += 1u;
}

// a language plugin's grammar noted where it has one, and whether it names a declaration, in the place of one of its id
// noted before, and one of its id that has none taken out: the plugin read for each id that stands is the last noted
static void cli_symbols_grammar_note(const CliEachPluginCfg *args)
{
    CliSymbolsLanguages *const read = (CliSymbolsLanguages *)args->context;
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc_temporary, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                   .text = args->plugin->text, .size = args->plugin->size, .nodes_more = 0u, .text_more = 0ULL,
                   .err_msg = &err_msg, .owner = g_cli_code_owner);
    const char *const type =
        (root != CLI_JSON_NONE)
            ? EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                         .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                            .object = root, .name = "kind"),
                         .length_pointer = NULL)
            : NULL;
    const int language =
        (root != CLI_JSON_NONE) &&
        ((type == NULL) || cellul.eq(&(CatenaFinitaCfg){.src = type, .other = "language", .cap = CLI_POOL_BYTES}));
    const int grammared =
        language && EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = &json,
                               .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                  .json_read = &json, .object = root, .name = "grammar"),
                               .type = CFG_JSON_OBJECT);
    // the marks the plugin names a directive's line and its carrying on by, copied before the plugin's tree is given
    // back
    char mark[CLI_SYMBOLS_MARK_MAX] = "";
    char continued[CLI_SYMBOLS_MARK_MAX] = "";
    static const char *const s_mark_names[] = {"directive_mark", "directive_continued"};
    char *const marks[] = {mark, continued};
    for (unsigned int at = 0u; grammared && (at < (sizeof(s_mark_names) / sizeof(s_mark_names[0]))); at += 1u)
    {
        const char *const named = EMBED_CALL(
            cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
            .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                               .object = root, .name = s_mark_names[at]),
            .length_pointer = NULL);
        if (named != NULL)
        {
            memor.cpy(&(MemoriaCfg){
                .dst = marks[at],
                .src = named,
                .bytes = (size_t)cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_SYMBOLS_MARK_MAX - 1u})});
        }
    }
    g_cli_guard->persistent_buf_release(json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    unsigned long long found = 0ULL;
    const int declares = grammared && EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = args->plugin->text,
                                                 .length_value = args->plugin->size, .required = "declaration.",
                                                 .size = 12u, .at_pointer = &found);
    unsigned int at = 0u;
    while ((at < read->grammars) && !cellul.eq(&(CatenaFinitaCfg){
                                        .src = read->grammar[at].id, .other = args->plugin->id, .cap = CLI_POOL_BYTES}))
    {
        at += 1u;
    }
    if (!grammared && (at < read->grammars))
    {
        read->grammar[at] = read->grammar[read->grammars - 1u];
        read->grammars -= 1u;
        return;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->plugin->id, .cap = CLI_POOL_BYTES});
    if (!grammared || (at == CLI_SYMBOLS_GRAMMARS) || (length >= CLI_NAME_MAX_LEN))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = read->grammar[at].id, .src = args->plugin->id, .bytes = (size_t)length + 1u});
    read->grammar[at].key[0] = '\0';
    read->grammar[at].stored = 0;
    read->grammar[at].declares = declares;
    memor.cpy(&(MemoriaCfg){.dst = read->grammar[at].directive_mark, .src = mark, .bytes = sizeof(mark)});
    memor.cpy(&(MemoriaCfg){.dst = read->grammar[at].directive_continued, .src = continued, .bytes = sizeof(continued)});
    read->grammars += (at == read->grammars) ? 1u : 0u;
}

CliSymbolsLanguages *cli_symbols_languages_read(void)
{
    CliSymbolsLanguages *const read = g_cli_guard->persistent_buf_alloc(sizeof(CliSymbolsLanguages), g_cli_code_owner);
    if (read != NULL)
    {
        EMBED_CALL(cli_plugins_table.languages, CliPluginsCfg, .languages = &read->languages);
        read->grammars = 0u;
        EMBED_CALL(cli_plugins_each_table.each, CliPluginsEachCfg, .each = cli_symbols_grammar_note, .context = read);
    }
    return read;
}

CliSymbolsLanguages *cli_symbols_languages_for_call(void)
{
    CliSymbolsLanguages *const read = g_cli_guard->temporary_buf_alloc(sizeof(CliSymbolsLanguages), g_cli_code_owner);
    if (read != NULL)
    {
        EMBED_CALL(cli_plugins_table.languages_for_call, CliPluginsCfg, .languages = &read->languages);
        read->grammars = 0u;
        EMBED_CALL(cli_plugins_each_table.each, CliPluginsEachCfg, .each = cli_symbols_grammar_note, .context = read);
    }
    return read;
}

typedef struct
{
    const CliSymbolsLanguages *read;
    const char *id;
    const char *path;
    CliSymbolsLanguages *read_out;
    unsigned int place;
    const char *state;
    const char *text;
    unsigned long long size;
    CliLeavesEach each;
    void *context;
} SymbolsReadCtx;
// the place among `read`'s grammars of the language `id`, CLI_SYMBOLS_GRAMMARS where it is none whose grammar is read
EMBED_INLINE unsigned int cli_symbols_grammar_by_name(const SymbolsReadCtx *args)
{
    for (unsigned int at = 0u; at < args->read->grammars; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->read->grammar[at].id, .other = args->id, .cap = CLI_POOL_BYTES}))
        {
            return (args->read->grammar[at].stored >= 0) ? at : CLI_SYMBOLS_GRAMMARS;
        }
    }
    return CLI_SYMBOLS_GRAMMARS;
}

// the place among `read`'s grammars of the language the file at `path` opens as, CLI_SYMBOLS_GRAMMARS where it opens
// as none whose grammar is read
EMBED_INLINE unsigned int cli_symbols_grammar_get(const SymbolsReadCtx *args)
{
    const char *path = args->path;
    char id[CLI_NAME_MAX_LEN];
    return EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg,
                      .languages_read = &args->read->languages, .path = path, .id_out = id)
               ? EMBED_CALL(cli_symbols_grammar_by_name, SymbolsReadCtx, .read = args->read, .id = id)
               : CLI_SYMBOLS_GRAMMARS;
}

int cli_symbols_reads_in(const CliSymbolsReadLanguagesCtx *args)
{
    const unsigned int place = EMBED_CALL(cli_symbols_grammar_get, SymbolsReadCtx,
                                          .read = args->read_symbols_languages_pointer, .path = args->path);
    return (place != CLI_SYMBOLS_GRAMMARS) && args->read_symbols_languages_pointer->grammar[place].declares;
}

const char *cli_symbols_directive_mark(const CliSymbolsReadLanguagesCtx *args)
{
    const unsigned int place = EMBED_CALL(cli_symbols_grammar_get, SymbolsReadCtx,
                                          .read = args->read_symbols_languages_pointer, .path = args->path);
    return (place != CLI_SYMBOLS_GRAMMARS) ? args->read_symbols_languages_pointer->grammar[place].directive_mark : "";
}

const char *cli_symbols_directive_continued(const CliSymbolsReadLanguagesCtx *args)
{
    const unsigned int place = EMBED_CALL(cli_symbols_grammar_get, SymbolsReadCtx,
                                          .read = args->read_symbols_languages_pointer, .path = args->path);
    return (place != CLI_SYMBOLS_GRAMMARS) ? args->read_symbols_languages_pointer->grammar[place].directive_continued
                                           : "";
}

int cli_symbols_reads(const CliSymbolsPathNameCfg *args)
{
    CliSymbolsLanguages *const read = cli_symbols_languages_read();
    const int reads =
        (read != NULL) && EMBED_CALL(cli_symbols_read_languages_internal.reads_in, CliSymbolsReadLanguagesCtx,
                                     .read_symbols_languages_pointer = read, .path = args->path);
    if (read != NULL)
    {
        g_cli_guard->persistent_buf_release(read->languages.pair, g_cli_plugins_owner);
        *(&read->languages) = (CliLanguages){NULL, 0u, 0u};
    }
    g_cli_guard->persistent_buf_release(read, g_cli_code_owner);
    return reads;
}

void cli_symbols_declare(const CliSymbolsPathTextSizeCfg *args)
{
    CliSymbolsLanguages *const read = cli_symbols_languages_read();
    if (read != NULL)
    {
        EMBED_CALL(cli_symbols_path_read_languages_internal.declared_in, CliSymbolsPathReadLanguagesCtx,
                   .read_symbols_languages_pointer_other = read, .path = args->path, .text = args->text,
                   .size = args->size, .each = args->each_declared, .context = args->context);
    }
    if (read != NULL)
    {
        g_cli_guard->persistent_buf_release(read->languages.pair, g_cli_plugins_owner);
        *(&read->languages) = (CliLanguages){NULL, 0u, 0u};
    }
    g_cli_guard->persistent_buf_release(read, g_cli_code_owner);
}

void cli_symbols_declared_in(const CliSymbolsPathReadLanguagesCtx *args)
{
    CliSymbolsLine read_state = {args->each, args->context, 0u};
    (void)EMBED_CALL(cli_symbols_path_read_languages_text_internal.leaves_in, CliSymbolsPathReadLanguagesTextCtx,
                     .read_symbols_languages_pointer_other = args->read_symbols_languages_pointer_other,
                     .path = args->path, .state = "root", .text = args->text, .size = args->size,
                     .each_leaves_each = cli_symbols_line, .context = &read_state);
}

// the leaves of `size` bytes of `text` read by the grammar at `place` among `read`'s, the first line starting in
// `state`, each line's handed to `each`: 1, or 0 where the place is none or the cells cannot meet the text's copy
static int cli_symbols_leaves_at(const SymbolsReadCtx *args)
{
    const char *state = args->state;
    const char *text = args->text;
    unsigned long long size = args->size;
    CliLeavesEach each = args->each;
    void *context = args->context;
    if (args->place == CLI_SYMBOLS_GRAMMARS)
    {
        return 0;
    }
    CliSymbolsGrammar *const grammar = &args->read_out->grammar[args->place];
    if (grammar->stored == 0)
    {
        char why[CLI_PATH_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        grammar->stored = EMBED_CALL(cli_plugins_id_key.grammar_store, CliPluginsIdKeyCfg, .id = grammar->id,
                                     .key = grammar->key, .err_msg = &err_msg)
                              ? 1
                              : -1;
    }
    // the text's lines in a cell of their own, each ended by a NUL in place of its line's end, its carriage return
    // before it dropped
    char *const lines = (grammar->stored == 1) ? g_cli_guard->temporary_buf_alloc(size + 1ULL, g_cli_code_owner) : NULL;
    if (lines == NULL)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = lines, .src = text, .bytes = (size_t)size});
    lines[size] = '\0';
    for (unsigned long long at = 0ULL; at < size; at += 1ULL)
    {
        if (lines[at] != '\n')
        {
            continue;
        }
        lines[at] = '\0';
        if ((at != 0ULL) && (lines[at - 1u] == '\r'))
        {
            lines[at - 1u] = '\0';
        }
    }
    // the state the first line starts in, root where it is longer than a state is saved
    const unsigned long long supplied = cellul.len(&(CatenaFinitaCfg){.src = state, .cap = CLI_POOL_BYTES});
    char starting[CLI_SYMBOLS_STATE] = "root";
    if ((supplied != 0ULL) && (supplied < CLI_SYMBOLS_STATE))
    {
        memor.cpy(&(MemoriaCfg){.dst = starting, .src = state, .bytes = (size_t)supplied + 1u});
    }
    unsigned long long at = 0ULL;
    int colored = 1;
    while (colored && (at < size))
    {
        const char *batch[CLI_SYMBOLS_LINES];
        unsigned int count = 0u;
        while ((count < CLI_SYMBOLS_LINES) && (at < size))
        {
            batch[count] = lines + at;
            count += 1u;
            at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text, .at = at,
                            .size = size, .set = "\n");
            at += 1ULL;
        }
        char ended_text[CLI_SYMBOLS_STATE];
        VerbaLinea ended = {ended_text, sizeof(ended_text), 0ULL};
        char why[CLI_PATH_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        colored = EMBED_CALL(cli_highlight_key.leaves, CliHighlightKeyCfg, .key = grammar->key, .state = starting,
                             .lines = batch, .count = count, .ends = at >= size, .each = each, .context = context,
                             .ended = &ended, .err_msg = &err_msg);
        const unsigned long long length = mmgr_line_finish(&ended);
        memor.cpy(&(MemoriaCfg){.dst = starting,
                                .src = (length != 0ULL) ? ended_text : "root",
                                .bytes = (length != 0ULL) ? (length + 1u) : 5u});
    }
    g_cli_guard->persistent_buf_release(lines, g_cli_code_owner);
    return colored;
}

int cli_symbols_leaves_in(const CliSymbolsPathReadLanguagesTextCtx *args)
{
    return EMBED_CALL(cli_symbols_leaves_at, SymbolsReadCtx, .read_out = args->read_symbols_languages_pointer_other,
                      .place = EMBED_CALL(cli_symbols_grammar_get, SymbolsReadCtx,
                                          .read = args->read_symbols_languages_pointer_other, .path = args->path),
                      .state = args->state, .text = args->text, .size = args->size, .each = args->each_leaves_each,
                      .context = args->context);
}

int cli_symbols_language_leaves_in(const CliSymbolsReadLanguagesTextCtx *args)
{
    return EMBED_CALL(cli_symbols_leaves_at, SymbolsReadCtx, .read_out = args->read_symbols_languages_pointer_other,
                      .place = EMBED_CALL(cli_symbols_grammar_by_name, SymbolsReadCtx,
                                          .read = args->read_symbols_languages_pointer_other, .id = args->id),
                      .state = args->state, .text = args->text, .size = args->size, .each = args->each_leaves_each,
                      .context = args->context);
}
