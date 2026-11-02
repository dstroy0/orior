// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_changed.c: a batch of changes made on the disk while the tree's check is on: each server is notified of them,
// and a server that checks a file at a time is given each file the editor has open again, which it then reads against a
// header or a module that changed under it; a file gone loses its diagnostics; each file changed is read again by the
// inspections and the checkers; and the files changed are checked again, with the files of the tree whose includes or
// imports name one of them as a word, by a file of the same toolchain
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../plugins/plugins.h"
#include "../text/text.h"
#include "servers_internal.h"

// the toolchain of a file no server of the round serves
#define CLI_SERVERS_CHANGED_NONE 0xffffffffu

// The round, under its lock, each part in a cell of its size given back as the round ends: the tree's files one after
// another, each ended by a 0, and a place for each; the files to check again; each changed file's toolchain; the
// toolchains the round is of, the language each serves, and how many there are and may be; and the languages known
typedef struct
{
    char *tree_paths;
    const char **tree;
    unsigned int trees;
    const char **again;
    unsigned int agains;
    unsigned int *file_tool;
    char (*tool)[CLI_NAME_MAX_LEN];
    char (*tool_language)[CLI_NAME_MAX_LEN];
    unsigned int tools;
    unsigned int tools_most;
    CliLanguages *languages;
} CliServersChanged;

static CliServersChanged s_batch;
static CliServerSpec s_spec;
static CliLock s_lock = CLI_LOCK_INIT;

// What the servers receive of a round: the line it is written on, whether a change is written on it yet, and the line
// the URIs of the files gone are put on, each ended by a 0
typedef struct
{
    VerbaLinea *events;
    int written;
    VerbaLinea *gone;
} CliServersEvents;

typedef struct
{
    const char *file;
    const char **from;
    unsigned long long *size_pointer;
    const char *line;
    unsigned long long length;
    const char *stem;
    unsigned long long size;
    const char *text;
    unsigned int tool;
    const char *const *files;
    unsigned int count;
    const char *path;
    const char *events;
    const char *uri;
    const CliServersSink *sink;
    const char *root;
} ServersChangedCtx;
// the toolchain whose server serves the language `file` opens as: its place among the round's, or
// CLI_SERVERS_CHANGED_NONE where none does
EMBED_INLINE unsigned int changed_tool_get(const ServersChangedCtx *args)
{
    char id[CLI_NAME_MAX_LEN];
    if (!EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg,
                    .languages_read = s_batch.languages, .path = args->file, .id_out = id))
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
    if (EMBED_CALL(cli_servers_language_internal.spec_for, CliServersLanguageCtx, .language = id, .spec = &s_spec))
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
EMBED_INLINE int changed_stem(const ServersChangedCtx *args)
{
    const char **from = args->from;
    unsigned long long *size = args->size_pointer;
    const char *name = args->file;
    const unsigned long long name_past =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->file,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}), .set = "/\\");
    name = (name_past != 0ull) ? (args->file + name_past) : name;
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
EMBED_INLINE int changed_word(const ServersChangedCtx *args)
{
    unsigned long long length = args->length;
    unsigned long long size = args->size;
    unsigned long long from = 0ull;
    unsigned long long at = 0ull;
    while ((size > 0ull) &&
           EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = args->line + from, .length_value = length - from,
                      .wanted = args->stem, .size = size, .at_pointer = &at))
    {
        const unsigned long long start = from + at;
        const unsigned long long end = start + size;
        unsigned long long taken = 0ull;
        const unsigned int before = (start > 0ull)
                                        ? EMBED_CALL(cli_text_length.letter_before, CliTextLengthCfg,
                                                     .text = args->line, .end_value = start, .length = &taken)
                                        : 0u;
        const unsigned int after = (end < length) ? EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = args->line,
                                                               .size = length, .at = end, .length = &taken)
                                                  : 0u;
        const int bounded_before =
            (start == 0ull) ||
            (!EMBED_CALL(cli_text_table.alphanumeric, CliTextCfg, .letter = before) && (before != '_'));
        const int bounded_after =
            (end == length) ||
            (!EMBED_CALL(cli_text_table.alphanumeric, CliTextCfg, .letter = after) && (after != '_'));
        if (bounded_before && bounded_after)
        {
            return 1;
        }
        from = start + 1ull;
    }
    return 0;
}

int cli_servers_line_names(const CliServersSizeLengthCtx *args)
{
    static const char *const s_starts[5] = {"#include", "import ", "from ", "use ", "mod "};
    int starts = 0;
    for (unsigned int one = 0u; !starts && (one < 5u); one += 1u)
    {
        starts = EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = args->line_text,
                            .size = args->length, .prefix = s_starts[one]);
    }
    return starts && EMBED_CALL(changed_word, ServersChangedCtx, .line = args->line_text, .length = args->length,
                                .stem = args->stem, .size = args->size);
}

// whether a line of the `size` bytes of `text` that includes, imports or uses another names one of the stems of the
// files of the toolchain `tool` changed, as a word
EMBED_INLINE int changed_text_names(const ServersChangedCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned long long at = 0ull;
    while (at < size)
    {
        unsigned long long end = at;
        end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text, .at = end,
                         .size = size, .set = "\n");
        const unsigned long long cut = ((end > at) && (text[end - 1ull] == '\r')) ? end - 1ull : end;
        const char *const line =
            text + at +
            EMBED_CALL(cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg, .text = text + at, .size = cut - at);
        const unsigned long long length = (unsigned long long)((text + cut) - line);
        for (unsigned int file = 0u; file < args->count; file += 1u)
        {
            const char *stem = NULL;
            unsigned long long stem_size = 0ull;
            if ((s_batch.file_tool[file] == args->tool) &&
                EMBED_CALL(changed_stem, ServersChangedCtx, .file = args->files[file], .from = &stem,
                           .size_pointer = &stem_size) &&
                EMBED_CALL(cli_servers_size_length_internal.line_names, CliServersSizeLengthCtx, .line_text = line,
                           .length = length, .stem = stem, .size = stem_size))
            {
                return 1;
            }
        }
        at = end + 1ull;
    }
    return 0;
}

// the same of the file at `path`, read where it lies
EMBED_INLINE int changed_names(const ServersChangedCtx *args)
{
    const char *path = args->path;
    unsigned long long size = 0ull;
    const unsigned char *const mapped = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = path,
                                                   .size_pointer = &size, .owner = &g_cli_servers_cells);
    if (mapped == NULL)
    {
        return 0;
    }
    const int names =
        EMBED_CALL(cli_json_tree_size.utf8_whole, CliJsonTreeSizeCfg, .bytes = (const char *)mapped, .size = size) &&
        EMBED_CALL(changed_text_names, ServersChangedCtx, .text = (const char *)mapped, .size = size,
                   .tool = args->tool, .files = args->files, .count = args->count);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = mapped, .owner = &g_cli_servers_cells);
    return names;
}

// a key whose file is gone taken out of the diagnostics stored, reported to the servers and its URI recorded among the
// gone
static void changed_gone_find(const CliEachKeyBesideTextSizeCfg *args)
{
    const char *key = args->key;
    const char *text = args->text;
    (void)text;
    CliServersEvents *const events = args->context;
    char path[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_lsp_length_uri.path_get, CliLspLengthUriCfg, .uri = args->beside,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->beside, .cap = CLI_CELLS_POOL}),
                   .path_out = path) &&
        !EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path) &&
        !EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = path))
    {
        mmgr_line_put_n(events->gone, args->beside,
                        cellul.len(&(CatenaFinitaCfg){.src = args->beside, .cap = CLI_CELLS_POOL}) + 1ull);
        mmgr_line_put(events->events, events->written ? ",{\"uri\":" : "{\"uri\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = events->events,
                   .text = args->beside,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->beside, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(events->events, ",\"type\":3}");
        events->written = 1;
        EMBED_CALL(cli_keyed_key_beside.remove, CliKeyedKeyBesideCfg, .keyed_out = &g_cli_servers_published,
                   .key = key);
    }
}

// each server notified of the round, and a server that checks a file at a time given again each file the editor has
// open
EMBED_INLINE void changed_servers_send(const ServersChangedCtx *args)
{
    static CliLsp *s_server[CLI_SERVERS_MOST];
    static int s_whole[CLI_SERVERS_MOST];
    unsigned int count = 0u;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_servers.lock);
    for (unsigned int at = 0u; at < CLI_SERVERS_MOST; at += 1u)
    {
        if (g_cli_servers.running[at].used)
        {
            s_server[count] = g_cli_servers.running[at].lsp;
            s_whole[count] = (g_cli_servers.running[at].spec.checks[0] != '\0');
            EMBED_CALL(cli_lsp_context.ref, CliLspContextCfg, .lsp = s_server[count]);
            count += 1u;
        }
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_servers.lock);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_servers_check_open_lock);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char quiet[256];
        VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
        (void)EMBED_CALL(cli_lsp_err_msg_length.notify, CliLspErrMsgLengthCfg, .lsp = s_server[at],
                         .method = "workspace/didChangeWatchedFiles", .params = args->events, .length = args->size,
                         .err_msg = &ignored);
        if (!s_whole[at])
        {
            EMBED_CALL(cli_servers_server_command_internal.reread_open, CliServersServerCommandCtx,
                       .server = s_server[at]);
        }
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = s_server[at]);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_servers_check_open_lock);
}

// the file of the URI `uri`, gone, reported to have no diagnostics
EMBED_INLINE void changed_gone_emit(const ServersChangedCtx *args)
{
    const char *uri = args->uri;
    char path[CLI_PATH_MAX_LEN];
    if ((args->sink->emit == NULL) ||
        !EMBED_CALL(cli_lsp_length_uri.path_get, CliLspLengthUriCfg, .uri = uri,
                    .length = cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), .path_out = path))
    {
        return;
    }
    char shown[CLI_PATH_MAX_LEN];
    VerbaLinea where = {shown, sizeof(shown), 0ull};
    EMBED_CALL(cli_servers_path_err_msg_internal.tree_path, CliServersPathErrMsgCtx, .tree = args->sink->tree,
               .path = path, .out = &where);
    const unsigned long long shown_size = mmgr_line_ok(&where) ? where.at : 0ull;
    // the path with every byte escaped, and the members around it
    const unsigned long long room = (shown_size * 6ull) + 32ull;
    char *const cleared = g_cli_cells->temporary_buf_alloc(room, &g_cli_servers_cells);
    VerbaLinea event_json = mmgr_line_over(cleared, room);
    mmgr_line_put(&event_json, "{\"path\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &event_json,
               .text_bytes = (const unsigned char *)shown, .length = shown_size);
    mmgr_line_put(&event_json, ",\"items\":[]}");
    if (mmgr_line_ok(&event_json))
    {
        EMBED_CALL(args->sink->emit, CliEachEventJsonSizeCfg, .context = args->sink->context,
                   .event = "lsp-diagnostics", .json = event_json.out, .size = event_json.at);
    }
    g_cli_cells->persistent_buf_release(cleared, &g_cli_servers_cells);
}

// the servers notified of the files changed under `root` and of each file gone, and each gone reported to have no
// diagnostics
EMBED_INLINE void changed_emit(const ServersChangedCtx *args)
{
    const char *root = args->root;
    // a change's URI is at most a path's, written with every byte escaped; a URI gone is one stored beside its key
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_servers_published_lock);
    const unsigned long long stored =
        EMBED_CALL(cli_keyed_table.listed_size, CliKeyedCfg, .keyed = &g_cli_servers_published);
    const unsigned long long gone_room = stored + 1ull;
    const unsigned long long room = (((unsigned long long)args->count + g_cli_servers_published.entries) * 32ull) +
                                    ((unsigned long long)args->count * CLI_PATH_MAX_LEN * 6ull) + (stored * 6ull) +
                                    32ull;
    char *const uris = g_cli_cells->temporary_buf_alloc(gone_room, &g_cli_servers_cells);
    char *const changes_json = g_cli_cells->temporary_buf_alloc(room, &g_cli_servers_cells);
    VerbaLinea gone = mmgr_line_over(uris, gone_room);
    VerbaLinea line = mmgr_line_over(changes_json, room);
    CliServersEvents events = {&line, 0, &gone};
    mmgr_line_put(&line, "{\"changes\":[");
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        char path[CLI_PATH_MAX_LEN];
        char uri[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = args->files[at], .joined = path);
        EMBED_CALL(cli_servers_path_uri_internal.uri_get, CliServersPathUriCtx, .path = path, .uri_out = uri);
        mmgr_line_put(&line, events.written ? ",{\"uri\":" : "{\"uri\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &line, .text = uri,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(&line, ",\"type\":2}");
        events.written = 1;
    }
    EMBED_CALL(cli_keyed_each_table.each, CliKeyedEachCfg, .keyed = &g_cli_servers_published, .each = changed_gone_find,
               .context = &events);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_servers_published_lock);
    mmgr_line_put(&line, "]}");
    if (mmgr_line_ok(&line) && events.written)
    {
        EMBED_CALL(changed_servers_send, ServersChangedCtx, .events = line.out, .size = line.at);
    }
    g_cli_cells->persistent_buf_release(changes_json, &g_cli_servers_cells);
    const unsigned long long gone_size = mmgr_line_ok(&gone) ? gone.at : 0ull;
    for (unsigned long long at = 0ull; at < gone_size;
         at += cellul.len(&(CatenaFinitaCfg){.src = uris + at, .cap = CLI_CELLS_POOL}) + 1ull)
    {
        EMBED_CALL(changed_gone_emit, ServersChangedCtx, .uri = uris + at, .sink = args->sink);
    }
    g_cli_cells->persistent_buf_release(uris, &g_cli_servers_cells);
}

// the round's parts given back
static void changed_free(void)
{
    g_cli_cells->persistent_buf_release(s_batch.tree_paths, &g_cli_servers_cells);
    g_cli_cells->persistent_buf_release(s_batch.tree, &g_cli_servers_cells);
    g_cli_cells->persistent_buf_release(s_batch.again, &g_cli_servers_cells);
    g_cli_cells->persistent_buf_release(s_batch.file_tool, &g_cli_servers_cells);
    g_cli_cells->persistent_buf_release(s_batch.tool, &g_cli_servers_cells);
    g_cli_cells->persistent_buf_release(s_batch.tool_language, &g_cli_servers_cells);
    if (s_batch.languages != NULL)
    {
        EMBED_CALL(cli_plugins_table.languages_free, CliPluginsCfg, .languages = s_batch.languages);
    }
    g_cli_cells->persistent_buf_release(s_batch.languages, &g_cli_servers_cells);
    memor.set(&(MemoriaCfg){.dst = &s_batch, .val = 0, .bytes = sizeof(s_batch)});
}

// the tree's files under `root` listed and the round's parts taken for `count` files changed: 1, or 0 where the cells
// cannot meet them
EMBED_INLINE int changed_take(const ServersChangedCtx *args)
{
    const char *root = args->root;
    unsigned long long size = 0ull;
    s_batch.tree_paths = EMBED_CALL(cli_servers_root_size_internal.check_tree_files, CliServersRootSizeCtx,
                                    .root = root, .size_pointer = &size);
    for (unsigned long long at = 0ull; (s_batch.tree_paths != NULL) && (at < size); at += 1ull)
    {
        s_batch.trees += (s_batch.tree_paths[at] == '\0') ? 1u : 0u;
    }
    const unsigned long long places = (unsigned long long)s_batch.trees + 1ull;
    s_batch.tree = (s_batch.tree_paths != NULL)
                       ? g_cli_cells->temporary_buf_alloc(places * sizeof(char *), &g_cli_servers_cells)
                       : NULL;
    s_batch.again = (s_batch.tree != NULL) ? g_cli_cells->temporary_buf_alloc((places + args->count) * sizeof(char *),
                                                                              &g_cli_servers_cells)
                                           : NULL;
    s_batch.file_tool = (s_batch.again != NULL)
                            ? g_cli_cells->temporary_buf_alloc(
                                  ((unsigned long long)args->count + 1ull) * sizeof(unsigned int), &g_cli_servers_cells)
                            : NULL;
    s_batch.languages = (s_batch.file_tool != NULL)
                            ? g_cli_cells->temporary_buf_alloc(sizeof(CliLanguages), &g_cli_servers_cells)
                            : NULL;
    if (s_batch.languages == NULL)
    {
        return 0;
    }
    EMBED_CALL(cli_plugins_table.languages, CliPluginsCfg, .languages = s_batch.languages);
    s_batch.tools_most = s_batch.languages->count;
    const unsigned long long names = ((unsigned long long)s_batch.tools_most + 1ull) * CLI_NAME_MAX_LEN;
    s_batch.tool = g_cli_cells->temporary_buf_alloc(names, &g_cli_servers_cells);
    s_batch.tool_language =
        (s_batch.tool != NULL) ? g_cli_cells->temporary_buf_alloc(names, &g_cli_servers_cells) : NULL;
    unsigned long long at = 0ull;
    for (unsigned int file = 0u; file < s_batch.trees; file += 1u)
    {
        s_batch.tree[file] = s_batch.tree_paths + at;
        at += cellul.len(&(CatenaFinitaCfg){.src = s_batch.tree_paths + at, .cap = CLI_CELLS_POOL}) + 1ull;
    }
    return s_batch.tool_language != NULL;
}

void cli_servers_on_change(const CliServersRootSinkCountCfg *args)
{
    static CliServersSink s_emit;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    if (!EMBED_CALL(cli_servers_files_count_internal.check_files_change, CliServersFilesCountCtx, .root = args->root,
                    .files = args->files, .count = args->count, .sink_out = &s_emit))
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
        return;
    }
    EMBED_CALL(changed_emit, ServersChangedCtx, .root = args->root, .files = args->files, .count = args->count,
               .sink = &s_emit);
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        char path[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = args->files[at], .joined = path);
        unsigned long long size = 0ull;
        const unsigned char *const mapped = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = path,
                                                       .size_pointer = &size, .owner = &g_cli_servers_cells);
        const int whole = (mapped != NULL) && EMBED_CALL(cli_json_tree_size.utf8_whole, CliJsonTreeSizeCfg,
                                                         .bytes = (const char *)mapped, .size = size);
        EMBED_CALL(cli_servers_path_size_root_sink_internal.inspect, CliServersPathSizeRootSinkCtx, .root = args->root,
                   .path = path, .text = whole ? (const char *)mapped : NULL, .size = whole ? size : 0ull,
                   .sink = &s_emit);
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = mapped,
                   .owner = &g_cli_servers_cells);
        if (whole)
        {
            EMBED_CALL(cli_servers_path_size_root_sink_internal.run_checkers, CliServersPathSizeRootSinkCtx,
                       .root = args->root, .path = path, .text = NULL, .size = 0ull, .disk = 1, .sink = &s_emit);
        }
    }
    if (!EMBED_CALL(changed_take, ServersChangedCtx, .root = args->root, .count = args->count))
    {
        changed_free();
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
        return;
    }
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        char path[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = args->files[at], .joined = path);
        if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path))
        {
            s_batch.again[s_batch.agains] = args->files[at];
            s_batch.agains += 1u;
        }
    }
    int stems = 0;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const char *stem = NULL;
        unsigned long long size = 0ull;
        s_batch.file_tool[at] = EMBED_CALL(changed_tool_get, ServersChangedCtx, .file = args->files[at]);
        stems |=
            (s_batch.file_tool[at] != CLI_SERVERS_CHANGED_NONE) &&
            EMBED_CALL(changed_stem, ServersChangedCtx, .file = args->files[at], .from = &stem, .size_pointer = &size);
    }
    for (unsigned int other = 0u; stems && (other < s_batch.trees); other += 1u)
    {
        int again = 0;
        for (unsigned int at = 0u; !again && (at < s_batch.agains); at += 1u)
        {
            again = (cellul.eq(
                &(CatenaFinitaCfg){.src = s_batch.again[at], .other = s_batch.tree[other], .cap = CLI_CELLS_POOL}));
        }
        const unsigned int tool = again ? CLI_SERVERS_CHANGED_NONE
                                        : EMBED_CALL(changed_tool_get, ServersChangedCtx, .file = s_batch.tree[other]);
        char path[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = s_batch.tree[other],
                   .joined = path);
        if ((tool != CLI_SERVERS_CHANGED_NONE) && EMBED_CALL(changed_names, ServersChangedCtx, .path = path,
                                                             .tool = tool, .files = args->files, .count = args->count))
        {
            s_batch.again[s_batch.agains] = s_batch.tree[other];
            s_batch.agains += 1u;
        }
    }
    (void)EMBED_CALL(cli_servers_files_internal.check_files, CliServersFilesCtx, .root = args->root,
                     .files = s_batch.again, .count = s_batch.agains, .sink = &s_emit);
    changed_free();
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}
