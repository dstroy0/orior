// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_read.c: a text's leaves, the tokens of the grammar of the plugin its extension opens it as or of the language
// named, and a file's declarations read off them: every token whose name holds the segment `declaration` names a
// declaration, its kind the rest of the name past that segment, as `identifier.declaration.function` names a function.
// No language is read here but by its plugin's grammar and what its plugin says
#include "../command_line/cells.h"
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

const char *cli_symbols_declares(const char *name)
{
    static const char s_segment[] = "declaration.";
    const unsigned long long size = sizeof(s_segment) - 1u;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    unsigned long long from = 0ull;
    while ((from + size) < length)
    {
        unsigned long long found = 0ull;
        if (!cli_text_find(name + from, length - from, s_segment, size, &found))
        {
            return NULL;
        }
        const unsigned long long at = from + found;
        if ((at == 0ull) || (name[at - 1u] == '.'))
        {
            return name + at + size;
        }
        from = at + 1ull;
    }
    return NULL;
}

// each letter decoded as the colorer decodes it, one past U+FFFF taking two columns as UTF-16 counts them
unsigned long long cli_symbols_byte_at(const char *line, unsigned long long size, unsigned int col,
                                       unsigned long long *byte, unsigned int *at)
{
    while ((*at < col) && (*byte < size))
    {
        const unsigned char lead = (unsigned char)line[*byte];
        const unsigned int length = (lead < 0x80u) ? 1u : (lead < 0xe0u) ? 2u : (lead < 0xf0u) ? 3u : 4u;
        unsigned int letter = (length == 1u)   ? lead
                              : (length == 2u) ? (lead & 0x1fu)
                              : (length == 3u) ? (lead & 0x0fu)
                                               : (lead & 0x07u);
        for (unsigned int more = 1u; (more < length) && ((*byte + more) < size); more += 1u)
        {
            letter = (letter << 6u) | ((unsigned char)line[*byte + more] & 0x3fu);
        }
        *at += (letter >= 0x10000u) ? 2u : 1u;
        *byte += length;
    }
    return (*byte < size) ? *byte : size;
}

// One line's leaves read: each run whose token names a declaration handed on, the run's text its name and the rest
// of its token's name its kind
static void cli_symbols_line(void *context, const CliLeaves *leaves)
{
    CliSymbolsLine *const reading = (CliSymbolsLine *)context;
    const char *const line = leaves->line;
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL});
    unsigned long long byte = 0ull;
    unsigned int at = 0u;
    for (unsigned int run = 0u; run < leaves->runs; run += 1u)
    {
        const unsigned long long from = cli_symbols_byte_at(line, size, leaves->col[run], &byte, &at);
        const unsigned long long to =
            ((run + 1u) < leaves->runs) ? cli_symbols_byte_at(line, size, leaves->col[run + 1u], &byte, &at) : size;
        const char *const kind = cli_symbols_declares(leaves->text + leaves->name_at[leaves->name[run]]);
        if ((kind != NULL) && (kind[0] != '\0') && (to > from))
        {
            reading->each(reading->context, line + from, to - from, kind, reading->line);
        }
    }
    reading->line += 1u;
}

// a language plugin's grammar noted where it has one, and whether it names a declaration, in the place of one of its id
// noted before, and one of its id that has none taken out: the plugin read for each id that stands is the last noted
static void cli_symbols_grammar_noted(void *context, const CliPlugin *plugin)
{
    CliSymbolsLanguages *const read = (CliSymbolsLanguages *)context;
    CliJson json;
    cli_json_begun(&json, NULL, 0u, NULL, 0ull);
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const unsigned int root =
        cli_json_taken_temporary(&json, plugin->text, plugin->size, 0u, 0ull, &err_msg, &g_cli_code_cells);
    const char *const kind =
        (root != CLI_JSON_NONE) ? cli_json_string(&json, cli_json_member(&json, root, "kind"), NULL) : NULL;
    const int language =
        (root != CLI_JSON_NONE) &&
        ((kind == NULL) || cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "language", .cap = CLI_CELLS_POOL}));
    const int grammared = language && cli_json_is(&json, cli_json_member(&json, root, "grammar"), CFG_JSON_OBJECT);
    cli_json_given(&json);
    unsigned long long found = 0ull;
    const int declares = grammared && cli_text_find(plugin->text, plugin->size, "declaration.", 12u, &found);
    unsigned int at = 0u;
    while ((at < read->grammars) &&
           !cellul.eq(&(CatenaFinitaCfg){.src = read->grammar[at].id, .other = plugin->id, .cap = CLI_CELLS_POOL}))
    {
        at += 1u;
    }
    if (!grammared && (at < read->grammars))
    {
        read->grammar[at] = read->grammar[read->grammars - 1u];
        read->grammars -= 1u;
        return;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = plugin->id, .cap = CLI_CELLS_POOL});
    if (!grammared || (at == CLI_SYMBOLS_GRAMMARS) || (length >= CLI_NAME_MAX_LEN))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = read->grammar[at].id, .src = plugin->id, .bytes = (size_t)length + 1u});
    read->grammar[at].key[0] = '\0';
    read->grammar[at].stored = 0;
    read->grammar[at].declares = declares;
    read->grammars += (at == read->grammars) ? 1u : 0u;
}

CliSymbolsLanguages *cli_symbols_languages_read(void)
{
    CliSymbolsLanguages *const read = g_cli_cells->persistent_buf_alloc(sizeof(CliSymbolsLanguages), &g_cli_code_cells);
    if (read != NULL)
    {
        cli_plugins_languages(&read->languages);
        read->grammars = 0u;
        cli_plugins_each(cli_symbols_grammar_noted, read);
    }
    return read;
}

void cli_symbols_languages_given(CliSymbolsLanguages *read)
{
    if (read != NULL)
    {
        cli_plugins_languages_given(&read->languages);
    }
    g_cli_cells->persistent_buf_release(read, &g_cli_code_cells);
}

// the place among `read`'s grammars of the language `id`, CLI_SYMBOLS_GRAMMARS where it is none whose grammar is read
static unsigned int cli_symbols_grammar_named(const CliSymbolsLanguages *read, const char *id)
{
    for (unsigned int at = 0u; at < read->grammars; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = read->grammar[at].id, .other = id, .cap = CLI_CELLS_POOL}))
        {
            return (read->grammar[at].stored >= 0) ? at : CLI_SYMBOLS_GRAMMARS;
        }
    }
    return CLI_SYMBOLS_GRAMMARS;
}

// the place among `read`'s grammars of the language the file at `path` opens as, CLI_SYMBOLS_GRAMMARS where it opens
// as none whose grammar is read
static unsigned int cli_symbols_grammar_of(const CliSymbolsLanguages *read, const char *path)
{
    char id[CLI_NAME_MAX_LEN];
    return cli_plugins_language_for(&read->languages, path, id) ? cli_symbols_grammar_named(read, id)
                                                                : CLI_SYMBOLS_GRAMMARS;
}

int cli_symbols_reads_in(const CliSymbolsLanguages *read, const char *path)
{
    const unsigned int place = cli_symbols_grammar_of(read, path);
    return (place != CLI_SYMBOLS_GRAMMARS) && read->grammar[place].declares;
}

int cli_symbols_reads(const char *path)
{
    CliSymbolsLanguages *const read = cli_symbols_languages_read();
    const int reads = (read != NULL) && cli_symbols_reads_in(read, path);
    cli_symbols_languages_given(read);
    return reads;
}

void cli_symbols_declared(const char *path, const char *text, unsigned long long size, CliDeclared each, void *context)
{
    CliSymbolsLanguages *const read = cli_symbols_languages_read();
    if (read != NULL)
    {
        cli_symbols_declared_in(read, path, text, size, each, context);
    }
    cli_symbols_languages_given(read);
}

void cli_symbols_declared_in(CliSymbolsLanguages *read, const char *path, const char *text, unsigned long long size,
                             CliDeclared each, void *context)
{
    CliSymbolsLine reading = {each, context, 0u};
    (void)cli_symbols_leaves_in(read, path, "root", text, size, cli_symbols_line, &reading);
}

// the leaves of `size` bytes of `text` read by the grammar at `place` among `read`'s, the first line starting in
// `state`, each line's handed to `each`: 1, or 0 where the place is none or the cells cannot meet the text's copy
static int cli_symbols_leaves_at(CliSymbolsLanguages *read, unsigned int place, const char *state, const char *text,
                                 unsigned long long size, CliLeavesEach each, void *context)
{
    if (place == CLI_SYMBOLS_GRAMMARS)
    {
        return 0;
    }
    CliSymbolsGrammar *const grammar = &read->grammar[place];
    if (grammar->stored == 0)
    {
        char why[CLI_PATH_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ull};
        grammar->stored = cli_plugins_grammar_stored(grammar->id, grammar->key, &err_msg) ? 1 : -1;
    }
    // the text's lines in a cell of their own, each ended by a NUL in place of its line's end, its carriage return
    // before it dropped
    char *const lines =
        (grammar->stored == 1) ? g_cli_cells->temporary_buf_alloc(size + 1ull, &g_cli_code_cells) : NULL;
    if (lines == NULL)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = lines, .src = text, .bytes = (size_t)size});
    lines[size] = '\0';
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        if (lines[at] != '\n')
        {
            continue;
        }
        lines[at] = '\0';
        if ((at != 0ull) && (lines[at - 1u] == '\r'))
        {
            lines[at - 1u] = '\0';
        }
    }
    // the state the first line starts in, root where it is longer than a state is kept
    const unsigned long long given = cellul.len(&(CatenaFinitaCfg){.src = state, .cap = CLI_CELLS_POOL});
    char starting[CLI_SYMBOLS_STATE] = "root";
    if ((given != 0ull) && (given < CLI_SYMBOLS_STATE))
    {
        memor.cpy(&(MemoriaCfg){.dst = starting, .src = state, .bytes = (size_t)given + 1u});
    }
    unsigned long long at = 0ull;
    int colored = 1;
    while (colored && (at < size))
    {
        const char *batch[CLI_SYMBOLS_LINES];
        unsigned int count = 0u;
        while ((count < CLI_SYMBOLS_LINES) && (at < size))
        {
            batch[count] = lines + at;
            count += 1u;
            at = cli_text_to_any(text, at, size, "\n");
            at += 1ull;
        }
        char ended_text[CLI_SYMBOLS_STATE];
        VerbaLinea ended = {ended_text, sizeof(ended_text), 0ull};
        char why[CLI_PATH_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ull};
        colored =
            cli_highlight_leaves(grammar->key, starting, batch, count, at >= size, each, context, &ended, &err_msg);
        const unsigned long long length = mmgr_line_finish(&ended);
        memor.cpy(&(MemoriaCfg){.dst = starting,
                                .src = (length != 0ull) ? ended_text : "root",
                                .bytes = (length != 0ull) ? (length + 1u) : 5u});
    }
    g_cli_cells->persistent_buf_release(lines, &g_cli_code_cells);
    return colored;
}

int cli_symbols_leaves_in(CliSymbolsLanguages *read, const char *path, const char *state, const char *text,
                          unsigned long long size, CliLeavesEach each, void *context)
{
    return cli_symbols_leaves_at(read, cli_symbols_grammar_of(read, path), state, text, size, each, context);
}

int cli_symbols_language_leaves_in(CliSymbolsLanguages *read, const char *id, const char *state, const char *text,
                                   unsigned long long size, CliLeavesEach each, void *context)
{
    return cli_symbols_leaves_at(read, cli_symbols_grammar_named(read, id), state, text, size, each, context);
}
