// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_changed.c: a batch of changes made on the disk while the tree's check is on: each server hears of them, and
// a server that checks a file at a time is given each file the editor holds again, which it then reads against a
// header or a module that changed under it; a file gone loses its diagnostics; each file changed is read again by the
// inspections and the checkers; and the files changed are checked again, with the files of the tree whose includes or
// imports name one of them as a word, by a file of the same toolchain
#include "../command_line/cells.h"
#include "../plugins/plugins.h"
#include "servers_internal.h"

// the toolchain of a file no server of the batch serves
#define CLI_SERVERS_CHANGED_NONE 0xffffffffu

// The batch, under its lock, each part in a cell of its size given back as the batch ends: the tree's files one after
// another, each ended by a 0, and a place for each; the files to check again; each changed file's toolchain; the
// toolchains the batch is of, the language each serves, and how many there are and may be; and the languages known
typedef struct
{
    char *tree_paths;
    const char **tree;
    unsigned int trees;
    const char **again;
    unsigned int agains;
    unsigned int *file_tool;
    char (*tool)[CLI_NAME_LONGEST];
    char (*tool_language)[CLI_NAME_LONGEST];
    unsigned int tools;
    unsigned int tools_most;
    CliLanguages *languages;
} CliServersChanged;

static CliServersChanged s_batch;
static CliServerSpec s_spec;
static CliLock s_lock = CLI_LOCK_INIT;

// What the servers hear of a batch: the line it is written on, whether a change is written on it yet, and the line
// the URIs of the files gone are put on, each ended by a 0
typedef struct
{
    CliLine *events;
    int written;
    CliLine *gone;
} CliServersEvents;

// a file of the tree put on the line that is `context`, ended by a 0
static void changed_tree_file(void *context, const char *file)
{
    cli_line_counted((CliLine *)context, file,
                     cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}) + 1ull);
}

// the toolchain whose server serves the language `file` opens as: its place among the batch's, or
// CLI_SERVERS_CHANGED_NONE where none does
static unsigned int changed_tool_of(const char *file)
{
    char id[CLI_NAME_LONGEST];
    if (!cli_plugins_language_for(s_batch.languages, file, id))
    {
        return CLI_SERVERS_CHANGED_NONE;
    }
    for (unsigned int at = 0u; at < s_batch.tools; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_batch.tool_language[at], .other = id, .cap = CLI_CELLS_POOL}))
        {
            return (s_batch.tool[at][0] != '\0') ? at : CLI_SERVERS_CHANGED_NONE;
        }
    }
    if (s_batch.tools == s_batch.tools_most)
    {
        return CLI_SERVERS_CHANGED_NONE;
    }
    const unsigned int at = s_batch.tools;
    memor.cpy(&(MemoriaCfg){.dst = s_batch.tool_language[at],
                            .src = id,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}) + 1u});
    s_batch.tool[at][0] = '\0';
    if (cli_servers_spec_for(id, &s_spec))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = s_batch.tool[at],
                          .src = s_spec.tool,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_spec.tool, .cap = CLI_CELLS_POOL}) + 1u});
    }
    s_batch.tools += 1u;
    return (s_batch.tool[at][0] != '\0') ? at : CLI_SERVERS_CHANGED_NONE;
}

// the stem of the file `file`: its name without its last extension, into `from` and `size`: 1, or 0 where it has none
static int changed_stem(const char *file, const char **from, unsigned long long *size)
{
    const char *name = file;
    for (const char *at = file; *at != '\0'; at += 1)
    {
        name = ((*at == '/') || (*at == '\\')) ? at + 1 : name;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    if ((length == 0ull) || (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "..", .cap = CLI_CELLS_POOL})))
    {
        return 0;
    }
    unsigned long long dot = length;
    for (unsigned long long at = 1ull; at < length; at += 1ull)
    {
        dot = (name[at] == '.') ? at : dot;
    }
    *from = name;
    *size = dot;
    return 1;
}

// whether `size` bytes of `stem` stand in the line as a word, with no letter, digit or `_` either side
static int changed_word(const char *line, unsigned long long length, const char *stem, unsigned long long size)
{
    unsigned long long from = 0ull;
    unsigned long long at = 0ull;
    while ((size > 0ull) && cli_text_find(line + from, length - from, stem, size, &at))
    {
        const unsigned long long start = from + at;
        const unsigned long long end = start + size;
        unsigned long long taken = 0ull;
        const unsigned int before = (start > 0ull) ? cli_text_letter_before(line, start, &taken) : 0u;
        const unsigned int after = (end < length) ? cli_text_letter(line, length, end, &taken) : 0u;
        const int bounded_before = (start == 0ull) || (!cli_text_alphanumeric(before) && (before != '_'));
        const int bounded_after = (end == length) || (!cli_text_alphanumeric(after) && (after != '_'));
        if (bounded_before && bounded_after)
        {
            return 1;
        }
        from = start + 1ull;
    }
    return 0;
}

int cli_servers_line_names(const char *line, unsigned long long length, const char *stem, unsigned long long size)
{
    static const char *const s_starts[5] = {"#include", "import ", "from ", "use ", "mod "};
    int starts = 0;
    for (unsigned int one = 0u; !starts && (one < 5u); one += 1u)
    {
        starts = cli_text_starts(line, length, s_starts[one]);
    }
    return starts && changed_word(line, length, stem, size);
}

// whether a line of the `size` bytes of `text` that includes, imports or uses another names one of the stems of the
// files of the toolchain `tool` changed, as a word
static int changed_text_names(const char *text, unsigned long long size, unsigned int tool, const char *const *files,
                              unsigned int count)
{
    unsigned long long at = 0ull;
    while (at < size)
    {
        unsigned long long end = at;
        while ((end < size) && (text[end] != '\n'))
        {
            end += 1ull;
        }
        const unsigned long long cut = ((end > at) && (text[end - 1ull] == '\r')) ? end - 1ull : end;
        const char *const line = text + at + cli_text_lead(text + at, cut - at);
        const unsigned long long length = (unsigned long long)((text + cut) - line);
        for (unsigned int file = 0u; file < count; file += 1u)
        {
            const char *stem = NULL;
            unsigned long long stem_size = 0ull;
            if ((s_batch.file_tool[file] == tool) && changed_stem(files[file], &stem, &stem_size) &&
                cli_servers_line_names(line, length, stem, stem_size))
            {
                return 1;
            }
        }
        at = end + 1ull;
    }
    return 0;
}

// the same of the file at `path`, read where it lies
static int changed_names(const char *path, unsigned int tool, const char *const *files, unsigned int count)
{
    unsigned long long size = 0ull;
    const unsigned char *const mapped = qry_member_file_mapped(path, &size);
    if (mapped == NULL)
    {
        return 0;
    }
    const int names = cli_utf8_whole((const char *)mapped, size) &&
                      changed_text_names((const char *)mapped, size, tool, files, count);
    qry_member_file_unmapped(mapped, size);
    return names;
}

// a key whose file is gone taken out of the diagnostics kept, said to the servers and its URI kept among the gone
static void changed_gone_found(void *context, const char *key, const char *beside, const char *text,
                               unsigned long long size)
{
    (void)text;
    (void)size;
    CliServersEvents *const events = context;
    char path[CLI_PATH_LONGEST];
    if (cli_lsp_path_of(beside, cellul.len(&(CatenaFinitaCfg){.src = beside, .cap = CLI_CELLS_POOL}), path) &&
        !query_is_file(path) && !query_is_folder(path))
    {
        cli_line_counted(events->gone, beside,
                         cellul.len(&(CatenaFinitaCfg){.src = beside, .cap = CLI_CELLS_POOL}) + 1ull);
        cli_line_text(events->events, events->written ? ",{\"uri\":" : "{\"uri\":");
        cli_json_quoted(events->events, beside, cellul.len(&(CatenaFinitaCfg){.src = beside, .cap = CLI_CELLS_POOL}));
        cli_line_text(events->events, ",\"type\":3}");
        events->written = 1;
        cli_keyed_remove(&g_cli_servers_published, key);
    }
}

// each server told of the batch, and a server that checks a file at a time given again each file the editor holds
static void changed_servers_told(const char *events, unsigned long long size)
{
    static CliLsp *s_server[CLI_SERVERS_MOST];
    static int s_whole[CLI_SERVERS_MOST];
    unsigned int count = 0u;
    cli_lock_take(&g_cli_servers.lock);
    for (unsigned int at = 0u; at < CLI_SERVERS_MOST; at += 1u)
    {
        if (g_cli_servers.running[at].used)
        {
            s_server[count] = g_cli_servers.running[at].lsp;
            s_whole[count] = (g_cli_servers.running[at].spec.checks[0] != '\0');
            cli_lsp_held(s_server[count]);
            count += 1u;
        }
    }
    cli_lock_give(&g_cli_servers.lock);
    cli_lock_take(&g_cli_servers_held_lock);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char quiet[256];
        CliLine ignored = {quiet, sizeof(quiet), 0ull};
        (void)cli_lsp_notified(s_server[at], "workspace/didChangeWatchedFiles", events, size, &ignored);
        if (!s_whole[at])
        {
            cli_servers_read_again_held(s_server[at]);
        }
        cli_lsp_given(s_server[at]);
    }
    cli_lock_give(&g_cli_servers_held_lock);
}

// the file of the URI `uri`, gone, said to have no diagnostics
static void changed_gone_told(const char *uri, const CliServersEmit *emit)
{
    char path[CLI_PATH_LONGEST];
    if ((emit->told == NULL) ||
        !cli_lsp_path_of(uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), path))
    {
        return;
    }
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    CliLine told;
    cli_pieces_begun(&pieces, &told, piece, sizeof(piece));
    cli_line_text(&told, "{\"path\":");
    char shown[CLI_PATH_LONGEST];
    CliLine where = {shown, sizeof(shown), 0ull};
    cli_servers_tree_path(emit->tree, path, &where);
    cli_json_quoted_lossy(&told, (const unsigned char *)shown, cli_line_fits(&where) ? where.at : 0ull);
    cli_line_text(&told, ",\"items\":[]}");
    unsigned long long size = 0ull;
    char *const cleared = cli_pieces_joined(&pieces, &told, "a gone file's diagnostics", &size);
    if (cleared != NULL)
    {
        emit->told(emit->context, "lsp-diagnostics", cleared, size);
    }
    cli_cell_given(cleared);
}

// the servers told of the files changed under `root` and of each file gone, and each gone said to have no diagnostics
static void changed_told(const char *root, const char *const *files, unsigned int count, const CliServersEmit *emit)
{
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    CliLine line;
    cli_pieces_begun(&pieces, &line, piece, sizeof(piece));
    char gone_piece[CLI_SERVERS_PIECE];
    CliPieces gone_pieces;
    CliLine gone;
    cli_pieces_begun(&gone_pieces, &gone, gone_piece, sizeof(gone_piece));
    CliServersEvents events = {&line, 0, &gone};
    cli_line_text(&line, "{\"changes\":[");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char path[CLI_PATH_LONGEST];
        char uri[CLI_PATH_LONGEST];
        cli_full(root, files[at], path);
        cli_servers_uri_of(path, uri);
        cli_line_text(&line, events.written ? ",{\"uri\":" : "{\"uri\":");
        cli_json_quoted(&line, uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}));
        cli_line_text(&line, ",\"type\":2}");
        events.written = 1;
    }
    cli_lock_take(&g_cli_servers_published_lock);
    cli_keyed_each(&g_cli_servers_published, changed_gone_found, &events);
    cli_lock_give(&g_cli_servers_published_lock);
    cli_line_text(&line, "]}");
    unsigned long long size = 0ull;
    char *const heard = cli_pieces_joined(&pieces, &line, "the changes the servers hear", &size);
    if ((heard != NULL) && events.written)
    {
        changed_servers_told(heard, size);
    }
    cli_cell_given(heard);
    unsigned long long gone_size = 0ull;
    char *const uris = cli_pieces_joined(&gone_pieces, &gone, "the files gone", &gone_size);
    for (unsigned long long at = 0ull; (uris != NULL) && (at < gone_size);
         at += cellul.len(&(CatenaFinitaCfg){.src = uris + at, .cap = CLI_CELLS_POOL}) + 1ull)
    {
        changed_gone_told(uris + at, emit);
    }
    cli_cell_given(uris);
}

// the batch's parts given back
static void changed_given(void)
{
    cli_cell_given(s_batch.tree_paths);
    cli_cell_given(s_batch.tree);
    cli_cell_given(s_batch.again);
    cli_cell_given(s_batch.file_tool);
    cli_cell_given(s_batch.tool);
    cli_cell_given(s_batch.tool_language);
    cli_cell_given(s_batch.languages);
    memor.set(&(MemoriaCfg){.dst = &s_batch, .val = 0, .bytes = sizeof(s_batch)});
}

// the tree's files under `root` listed and the batch's parts taken for `count` files changed: 1, or 0 where the cells
// cannot meet them
static int changed_taken(const char *root, unsigned int count)
{
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    CliLine line;
    cli_pieces_begun(&pieces, &line, piece, sizeof(piece));
    (void)cli_servers_check_tree_files(root, changed_tree_file, &line);
    unsigned long long size = 0ull;
    s_batch.tree_paths = cli_pieces_joined(&pieces, &line, "the tree's files", &size);
    for (unsigned long long at = 0ull; (s_batch.tree_paths != NULL) && (at < size); at += 1ull)
    {
        s_batch.trees += (s_batch.tree_paths[at] == '\0') ? 1u : 0u;
    }
    const unsigned long long places = (unsigned long long)s_batch.trees + 1ull;
    s_batch.tree = (s_batch.tree_paths != NULL) ? cli_cell_taken(places * sizeof(char *), "the tree's files") : NULL;
    s_batch.again =
        (s_batch.tree != NULL) ? cli_cell_taken((places + count) * sizeof(char *), "the files checked again") : NULL;
    s_batch.file_tool =
        (s_batch.again != NULL)
            ? cli_cell_taken(((unsigned long long)count + 1ull) * sizeof(unsigned int), "a toolchain each")
            : NULL;
    s_batch.languages =
        (s_batch.file_tool != NULL) ? cli_cell_taken(sizeof(CliLanguages), "the languages known") : NULL;
    if (s_batch.languages == NULL)
    {
        return 0;
    }
    cli_plugins_languages(s_batch.languages);
    s_batch.tools_most = s_batch.languages->count;
    const unsigned long long names = ((unsigned long long)s_batch.tools_most + 1ull) * CLI_NAME_LONGEST;
    s_batch.tool = cli_cell_taken(names, "the batch's toolchains");
    s_batch.tool_language = (s_batch.tool != NULL) ? cli_cell_taken(names, "the languages they serve") : NULL;
    unsigned long long at = 0ull;
    for (unsigned int file = 0u; file < s_batch.trees; file += 1u)
    {
        s_batch.tree[file] = s_batch.tree_paths + at;
        at += cellul.len(&(CatenaFinitaCfg){.src = s_batch.tree_paths + at, .cap = CLI_CELLS_POOL}) + 1ull;
    }
    return s_batch.tool_language != NULL;
}

void cli_servers_changed(const char *root, const char *const *files, unsigned int count)
{
    static CliServersEmit s_emit;
    cli_lock_take(&s_lock);
    if (!cli_servers_check_files_changed(root, files, count, &s_emit))
    {
        cli_lock_give(&s_lock);
        return;
    }
    changed_told(root, files, count, &s_emit);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char path[CLI_PATH_LONGEST];
        cli_full(root, files[at], path);
        unsigned long long size = 0ull;
        const unsigned char *const mapped = qry_member_file_mapped(path, &size);
        const int whole = (mapped != NULL) && cli_utf8_whole((const char *)mapped, size);
        cli_servers_inspect(root, path, whole ? (const char *)mapped : NULL, whole ? size : 0ull, &s_emit);
        if (mapped != NULL)
        {
            qry_member_file_unmapped(mapped, size);
        }
        if (whole)
        {
            cli_servers_run_checkers(root, path, NULL, 0ull, 1, &s_emit);
        }
    }
    if (!changed_taken(root, count))
    {
        changed_given();
        cli_lock_give(&s_lock);
        return;
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char path[CLI_PATH_LONGEST];
        cli_full(root, files[at], path);
        if (query_is_file(path))
        {
            s_batch.again[s_batch.agains] = files[at];
            s_batch.agains += 1u;
        }
    }
    int stems = 0;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *stem = NULL;
        unsigned long long size = 0ull;
        s_batch.file_tool[at] = changed_tool_of(files[at]);
        stems |= (s_batch.file_tool[at] != CLI_SERVERS_CHANGED_NONE) && changed_stem(files[at], &stem, &size);
    }
    for (unsigned int other = 0u; stems && (other < s_batch.trees); other += 1u)
    {
        int again = 0;
        for (unsigned int at = 0u; !again && (at < s_batch.agains); at += 1u)
        {
            again = (cellul.eq(
                &(CatenaFinitaCfg){.src = s_batch.again[at], .other = s_batch.tree[other], .cap = CLI_CELLS_POOL}));
        }
        const unsigned int tool = again ? CLI_SERVERS_CHANGED_NONE : changed_tool_of(s_batch.tree[other]);
        char path[CLI_PATH_LONGEST];
        cli_full(root, s_batch.tree[other], path);
        if ((tool != CLI_SERVERS_CHANGED_NONE) && changed_names(path, tool, files, count))
        {
            s_batch.again[s_batch.agains] = s_batch.tree[other];
            s_batch.agains += 1u;
        }
    }
    (void)cli_servers_check_files(root, s_batch.again, s_batch.agains, &s_emit);
    changed_given();
    cli_lock_give(&s_lock);
}
