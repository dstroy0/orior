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

// each character decoded as the colorer decodes it, one past U+FFFF taking two columns as UTF-16 counts them
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
        unsigned int character;
        if (length == 1u)
        {
            character = lead;
        }
        else if (length == 2u)
        {
            character = lead & 0x1fu;
        }
        else if (length == 3u)
        {
            character = lead & 0x0fu;
        }
        else
        {
            character = lead & 0x07u;
        }
        for (unsigned int more = 1u; (more < length) && ((*byte + more) < args->size); more += 1u)
        {
            character = (character << 6u) | ((unsigned char)args->line[*byte + more] & 0x3fu);
        }
        *at += (character >= 0x10000u) ? 2u : 1u;
        *byte += length;
    }
    return (*byte < args->size) ? *byte : args->size;
}

// One line's leaves read: each run whose token names a declaration passed on, the run's text its name and the rest
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

int cli_symbols_reads_in(const CliSymbolsReadLanguagesCtx *args)
{
    CliLanguageGrammar grammar;
    return EMBED_CALL(cli_plugins_languages_path.grammar_of, CliPluginsGrammarOfCfg, .path = args->path, .id = NULL,
                      .store = 0, .grammar_out = &grammar) &&
           grammar.declares;
}

int cli_symbols_grammar_read(const CliSymbolsGrammarReadCtx *args)
{
    return EMBED_CALL(cli_plugins_languages_path.grammar_of, CliPluginsGrammarOfCfg, .path = args->path, .id = NULL,
                      .store = 0, .grammar_out = args->grammar_out);
}

int cli_symbols_reads(const CliSymbolsPathNameCfg *args)
{
    return EMBED_CALL(cli_symbols_read_languages_internal.reads_in, CliSymbolsReadLanguagesCtx, .path = args->path);
}

void cli_symbols_declare(const CliSymbolsPathTextSizeCfg *args)
{
    EMBED_CALL(cli_symbols_path_read_languages_internal.declared_in, CliSymbolsPathReadLanguagesCtx, .path = args->path,
               .text = args->text, .size = args->size, .each = args->each_declared, .context = args->context);
}

void cli_symbols_declared_in(const CliSymbolsPathReadLanguagesCtx *args)
{
    CliSymbolsLine read_state = {args->each, args->context, 0u};
    (void)EMBED_CALL(cli_symbols_path_read_languages_text_internal.leaves_in, CliSymbolsPathReadLanguagesTextCtx,
                     .path = args->path, .state = "root", .text = args->text, .size = args->size,
                     .each_leaves_each = cli_symbols_line, .context = &read_state);
}

typedef struct
{
    const char *path;
    const char *id;
    const char *state;
    const char *text;
    unsigned long long size;
    CliLeavesEach each;
    void *context;
} SymbolsReadCtx;
// the leaves of `size` bytes of `text` read by the grammar of the file at `path`, or of the language `id`, the first
// line starting in `state`, each line's passed to `each`: 1, or 0 where the language has no grammar that stores or the
// cells cannot meet the text's copy
static int cli_symbols_leaves_at(const SymbolsReadCtx *args)
{
    const char *state = args->state;
    const char *text = args->text;
    unsigned long long size = args->size;
    CliLeavesEach each = args->each;
    void *context = args->context;
    CliLanguageGrammar read;
    CliLanguageGrammar *const grammar = &read;
    if (!EMBED_CALL(cli_plugins_languages_path.grammar_of, CliPluginsGrammarOfCfg, .path = args->path, .id = args->id,
                    .store = 1, .grammar_out = grammar))
    {
        return 0;
    }
    // the text's lines in a cell of their own, each ended by a NUL in place of its line's end, its carriage return
    // before it dropped
    char *const lines = g_platform_guard->temporary_buf_alloc(size + 1ULL, g_cli_code_owner);
    if (lines == NULL)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = lines, .src = text, .bytes = (embed_word)size});
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
        memor.cpy(&(MemoriaCfg){.dst = starting, .src = state, .bytes = (embed_word)supplied + 1u});
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
    g_platform_guard->persistent_buf_release(lines, g_cli_code_owner);
    return colored;
}

int cli_symbols_leaves_in(const CliSymbolsPathReadLanguagesTextCtx *args)
{
    return EMBED_CALL(cli_symbols_leaves_at, SymbolsReadCtx, .path = args->path, .id = NULL, .state = args->state,
                      .text = args->text, .size = args->size, .each = args->each_leaves_each, .context = args->context);
}

int cli_symbols_language_leaves_in(const CliSymbolsReadLanguagesTextCtx *args)
{
    return EMBED_CALL(cli_symbols_leaves_at, SymbolsReadCtx, .path = NULL, .id = args->id, .state = args->state,
                      .text = args->text, .size = args->size, .each = args->each_leaves_each, .context = args->context);
}
