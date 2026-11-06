// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_slice.c: the files the editor has open whose server is given the tree's slice for the lines in view in place
// of the file's text (symbols_slice.c). Each is stored by its key: the view the page last handed, its first and last
// lines and the lines opening the regions its first stands inside, and the slice's map, the file and line each line of
// the slice came from. A place in the file goes to the server as the slice's line that came from it, and a place the
// server answers about the slice comes back as the file and line its line came from, none for a bracket the slice
// closes. The slice is cut again and given as the file's next version as its text changes and as the page hands its
// view, and the check gives a file it slices a window at a time the same way. Each slice is given under the sending
// lock, its map stored with the version it is given as before it is given; the slices' lock is taken only to store and
// read them and never while a server is written to, as the thread that reads a server takes it to move the
// diagnostics published of a version to the file's lines by the map of that version, or leave them out
#include "../code/symbols.h"
#include "../command_line/callback_cfg.h"
#include "../text/text.h"
#include "servers_internal.h"

// A sliced file's head as it is stored: its view's first and last lines and how many lines open regions, how many lines
// its slice has, the bytes of the tree's root and of the paths after it, the number of the cut it is, a cut whose text
// is the last's saving its number, its text's hash, and the version of the file it was given its server as. The
// openers, the lines, the root and the paths follow it in that order, the file's own path in the tree the first path,
// at place 0
typedef struct
{
    unsigned int first;
    unsigned int last;
    unsigned int openers;
    unsigned int lines;
    unsigned long long root_size;
    unsigned long long paths_size;
    unsigned long long cut;
    unsigned long long hash;
    unsigned long long version;
} CliServersSliceHead;
// A slice being cut: its text, its lines' map and the paths they came from, each a line; the path the walk gave the
// last line as and its place among the paths; and how many lines are written
typedef struct
{
    VerbaLinea text;
    VerbaLinea map;
    VerbaLinea paths;
    const char *last_input;
    unsigned int last_position;
    unsigned int lines;
} CliServersSlicing;
// the last line in view of a file the page has not handed its view of, and the lines of each window the check slices a
// file into, a view's height; the most lines a view's openers name, the most files sliced at once, and the bytes a
// slice's text, map and paths are each written in
#define CLI_SERVERS_SLICE_LAST 63u
#define CLI_SERVERS_SLICE_HEIGHT (CLI_SERVERS_SLICE_LAST + 1u)
#define CLI_SERVERS_SLICE_OPENERS 64u
#define CLI_SERVERS_SLICES 256u
#define CLI_SERVERS_SLICE_CAPACITY (4ULL << 20u)
#define CLI_SERVERS_SLICE_MAPPED_MORE 4096ULL

// One line of a slice: the place of the path it came from among the slice's paths, CLI_JSON_NONE for none, and its
// line there
typedef struct
{
    unsigned int path;
    unsigned int line;
} CliServersSliceLine;

// the sliced files, by their keys with their paths, the last version each was published diagnostics of, and how many
// slices were cut, each cut's number, under the slices' lock; and the lock a slice is cut, stored and given its server
// under
static CliKeyed s_slices = {"slices", {NULL}, 0u, CLI_SERVERS_SLICES};
static CliKeyed s_published = {"slices published", {NULL}, 0u, CLI_SERVERS_SLICES};
static unsigned long long s_cuts;

typedef struct
{
    CliServersSlicing *slicing;
    const char *path;
    const char *root;
    const char *key;
    CliServersSliceHead *head;
    const unsigned int *opener;
    const CliServersSlicing *slicing_read;
    unsigned long long version;
    int always;
    int *fresh;
    const char *text;
    unsigned long long size;
    unsigned long long *slice_size;
    VerbaLinea *err_msg;
    const char *stored;
    const CliServersSliceHead *head_read;
    unsigned int at;
    unsigned int line;
    unsigned int *into;
    const CliJson *json;
    unsigned int one;
    unsigned int from;
    unsigned int to;
    VerbaLinea *out;
    unsigned long long length;
    unsigned long long otherwise;
} ServersSliceCtx;
// the place among the paths of the slice being cut of `path`, put after them where it is not among them, CLI_JSON_NONE
// for none
EMBED_INLINE unsigned int slice_path_position(const ServersSliceCtx *args)
{
    if (args->path == NULL)
    {
        return CLI_JSON_NONE;
    }
    if (args->path == args->slicing->last_input)
    {
        return args->slicing->last_position;
    }
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES});
    unsigned long long at = 0ULL;
    int found = 0;
    while (!found && (at < args->slicing->paths.at))
    {
        const char *const one = args->slicing->paths.out + at;
        const unsigned long long one_size = cellul.len(&(CatenaFinitaCfg){.src = one, .cap = CLI_POOL_BYTES});
        found = (one_size == size) &&
                (memor.cmp(&(MemoriaCfg){.src = one, .other = args->path, .bytes = (size_t)size}) == 0);
        at += found ? 0ULL : (one_size + 1ULL);
    }
    if (!found)
    {
        // a path is below CLI_PATH_MAX_LEN bytes
        mmgr_line_put_n(&args->slicing->paths, args->path, (size_t)size);
        mmgr_line_ch(&args->slicing->paths, '\0');
    }
    args->slicing->last_input = args->path;
    // the paths of one slice are below 2^32 bytes, its lines naming few files
    args->slicing->last_position = (unsigned int)at;
    return args->slicing->last_position;
}

// one line of the slice put after those before it, with where it came from
static void slice_line_append(const CliSliceLineCfg *args)
{
    const char *path = args->path;
    CliServersSlicing *const slicing = args->context;
    // a line of a text is below 2^32 bytes
    mmgr_line_put_n(&slicing->text, args->text, (size_t)args->length);
    mmgr_line_ch(&slicing->text, '\n');
    const CliServersSliceLine one = {EMBED_CALL(slice_path_position, ServersSliceCtx, .slicing = slicing, .path = path),
                                     args->line};
    mmgr_line_put_n(&slicing->map, (const char *)&one, sizeof(one));
    slicing->lines += 1u;
}

// the slicing's lines made, none written: 1, or 0 where the cells cannot meet them, those made given back with the rest
// through persistent_buf_release
EMBED_INLINE int slice_lines_init(const ServersSliceCtx *args)
{
    CliServersSlicing *slicing = args->slicing;
    char *const text_cell = g_cli_guard->temporary_buf_alloc(CLI_SERVERS_SLICE_CAPACITY, g_cli_servers_owner);
    char *const map_cell = g_cli_guard->temporary_buf_alloc(CLI_SERVERS_SLICE_CAPACITY, g_cli_servers_owner);
    char *const paths_cell = g_cli_guard->temporary_buf_alloc(CLI_SERVERS_SLICE_CAPACITY, g_cli_servers_owner);
    *slicing = (CliServersSlicing){mmgr_line_over(text_cell, CLI_SERVERS_SLICE_CAPACITY),
                                   mmgr_line_over(map_cell, CLI_SERVERS_SLICE_CAPACITY),
                                   mmgr_line_over(paths_cell, CLI_SERVERS_SLICE_CAPACITY),
                                   NULL,
                                   CLI_JSON_NONE,
                                   0u};
    return (text_cell != NULL) && (map_cell != NULL) && (paths_cell != NULL);
}

// the slice `slicing` holds stored by `key` with the view of `head`, its openers `opener`, and its map, the file at
// `path` in the tree at `root`, under the slices' lock: its number and version `head`'s where its text is the text
// `head` was cut as and it is not given `always`, else the next number and `version`, the version it is to be given as,
// with `*fresh` 1: 1, or 0 where the cells cannot meet it
static int slice_save(const ServersSliceCtx *args)
{
    const char *key = args->key;
    unsigned long long version = args->version;
    int *fresh = args->fresh;
    args->head->lines = args->slicing_read->lines;
    args->head->root_size = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_POOL_BYTES}) + 1ULL;
    args->head->paths_size = args->slicing_read->paths.at;
    const unsigned long long hash =
        EMBED_CALL(cli_servers_read_state_internal.hash_get, CliServersReadStateCtx,
                   .text = args->slicing_read->text.out, .size = args->slicing_read->text.at);
    *fresh = args->always || (args->head->cut == 0ULL) || (args->head->hash != hash);
    // a file opened afresh has had nothing published of it by the server it is opened in
    if (*fresh && (version == 1ULL))
    {
        EMBED_CALL(cli_keyed_key_adjacent.remove, CliKeyedKeyAdjacentCfg, .keyed_out = &s_published, .key = key);
    }
    s_cuts += *fresh ? 1ULL : 0ULL;
    args->head->cut = *fresh ? s_cuts : args->head->cut;
    args->head->version = *fresh ? version : args->head->version;
    args->head->hash = hash;
    const unsigned long long openers_size = (unsigned long long)args->head->openers * sizeof(unsigned int);
    const unsigned long long stored_size = sizeof(*args->head) + openers_size + args->slicing_read->map.at +
                                           args->head->root_size + args->head->paths_size;
    char *const stored = g_cli_guard->temporary_buf_alloc(stored_size, g_cli_servers_owner);
    if (stored != NULL)
    {
        unsigned long long at = 0ULL;
        memor.cpy(&(MemoriaCfg){.dst = stored, .src = args->head, .bytes = sizeof(*args->head)});
        at += sizeof(*args->head);
        // the openers are at most CLI_SERVERS_SLICE_OPENERS, and each part below was written into a cell of its size
        memor.cpy(&(MemoriaCfg){.dst = stored + at, .src = args->opener, .bytes = (size_t)openers_size});
        at += openers_size;
        memor.cpy(
            &(MemoriaCfg){.dst = stored + at, .src = args->slicing_read->map.out, .bytes = args->slicing_read->map.at});
        at += args->slicing_read->map.at;
        memor.cpy(&(MemoriaCfg){.dst = stored + at, .src = args->root, .bytes = (size_t)args->head->root_size});
        at += args->head->root_size;
        memor.cpy(&(MemoriaCfg){
            .dst = stored + at, .src = args->slicing_read->paths.out, .bytes = args->slicing_read->paths.at});
    }
    const int saved = (stored != NULL) &&
                      EMBED_CALL(cli_keyed_key_adjacent.put, CliKeyedKeyAdjacentCfg, .keyed_out = &s_slices, .key = key,
                                 .adjacent_value = args->path, .text = stored, .size_value = stored_size);
    g_cli_guard->persistent_buf_release(stored, g_cli_servers_owner);
    return saved;
}

// the slice of the file at `path` in the tree at `root` cut from `size` bytes of `text` for the view of `head`, its
// openers `opener`, and stored by `key` with the view and its map as slice_save stores it, to be given as `version`
// where `*fresh` is 1: its text, in a cell the caller gives back with g_cli_guard->persistent_buf_release, its size
// into `slice_size`, or NULL with why given
static char *slice_cut(const ServersSliceCtx *args)
{
    const char *path = args->path;
    const unsigned int *opener = args->opener;
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned long long *slice_size = args->slice_size;
    VerbaLinea *err_msg = args->err_msg;
    char own[CLI_PATH_MAX_LEN];
    VerbaLinea own_line = {own, sizeof(own), 0ULL};
    EMBED_CALL(cli_servers_path_err_msg_internal.tree_path, CliServersPathErrMsgCtx, .tree = args->root, .path = path,
               .out = &own_line);
    CliServersSlicing slicing;
    const int made =
        EMBED_CALL(slice_lines_init, ServersSliceCtx, .slicing = &slicing) && (mmgr_line_finish(&own_line) != 0ULL);
    if (made)
    {
        (void)EMBED_CALL(slice_path_position, ServersSliceCtx, .slicing = &slicing, .path = own);
    }
    const int cut = made && EMBED_CALL(cli_symbols_path.slice_each, CliSymbolsPathCfg, .path = own, .text = text,
                                       .size = size, .first = args->head->first, .last = args->head->last,
                                       .opener = opener, .openers = args->head->openers,
                                       .each_slice_line = slice_line_append, .context = &slicing, .err_msg = err_msg);
    const int integer =
        cut && mmgr_line_ok(&slicing.text) && mmgr_line_ok(&slicing.map) && mmgr_line_ok(&slicing.paths);
    const int saved =
        integer && EMBED_CALL(slice_save, ServersSliceCtx, .root = args->root, .path = path, .key = args->key,
                              .head = args->head, .opener = opener, .slicing_read = &slicing, .version = args->version,
                              .always = args->always, .fresh = args->fresh);
    if (!saved)
    {
        // a walk that could not cut it gave why already
        if (!made || cut)
        {
            mmgr_line_put(err_msg, "the slice could not be cut: the cells cannot meet it");
        }
        g_cli_guard->persistent_buf_release(slicing.text.out, g_cli_servers_owner);
        g_cli_guard->persistent_buf_release(slicing.map.out, g_cli_servers_owner);
        g_cli_guard->persistent_buf_release(slicing.paths.out, g_cli_servers_owner);
        return NULL;
    }
    *slice_size = slicing.text.at;
    g_cli_guard->persistent_buf_release(slicing.map.out, g_cli_servers_owner);
    g_cli_guard->persistent_buf_release(slicing.paths.out, g_cli_servers_owner);
    return slicing.text.out;
}

// the slice stored by `key` read under the slices' lock, its head into `head`: its bytes, NULL where none is stored
EMBED_INLINE const char *slice_store(const ServersSliceCtx *args)
{
    const char *key = args->key;
    const char *adjacent = NULL;
    unsigned long long size = 0ULL;
    const char *const stored = EMBED_CALL(cli_keyed_key_adjacent.get, CliKeyedKeyAdjacentCfg, .keyed = &s_slices,
                                          .key = key, .adjacent = &adjacent, .size = &size);
    if ((stored == NULL) || (size < sizeof(*args->head)))
    {
        return NULL;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->head, .src = stored, .bytes = sizeof(*args->head)});
    return stored;
}

// the line at `at` of the slice stored as `stored` with `head`
EMBED_INLINE CliServersSliceLine slice_line_at(const ServersSliceCtx *args)
{
    CliServersSliceLine one = {CLI_JSON_NONE, 0u};
    const unsigned long long place = sizeof(*args->head_read) +
                                     ((unsigned long long)args->head_read->openers * sizeof(unsigned int)) +
                                     ((unsigned long long)args->at * sizeof(one));
    memor.cpy(&(MemoriaCfg){.dst = &one, .src = args->stored + place, .bytes = sizeof(one)});
    return one;
}

// the tree's root of the slice stored as `stored` with `head`, and the paths after it
EMBED_INLINE const char *slice_root(const ServersSliceCtx *args)
{
    return args->stored + sizeof(*args->head_read) +
           ((unsigned long long)args->head_read->openers * sizeof(unsigned int)) +
           ((unsigned long long)args->head_read->lines * sizeof(CliServersSliceLine));
}

// the line of the slice stored as `stored` with `head` that came from the file's own line `line`: 1 with it in
// `into`, or 0 where none did
EMBED_INLINE int slice_line_in(const ServersSliceCtx *args)
{
    unsigned int *into = args->into;
    for (unsigned int at = 0u; at < args->head_read->lines; at += 1u)
    {
        const CliServersSliceLine one =
            EMBED_CALL(slice_line_at, ServersSliceCtx, .stored = args->stored, .head_read = args->head_read, .at = at);
        if ((one.path == 0u) && (one.line == args->line))
        {
            *into = at;
            return 1;
        }
    }
    return 0;
}

int cli_servers_slice_send(const CliServersPathSizeRootCtx *args)
{
    const char *path = args->path;
    const char *text = args->text;
    VerbaLinea *err_msg = args->err_msg;
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_servers_path_key_internal.key_get, CliServersPathKeyCtx, .path = path, .key_out = key);
    CliServersSliceHead head = {0u, CLI_SERVERS_SLICE_LAST, 0u, 0u, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL};
    unsigned int opener[CLI_SERVERS_SLICE_OPENERS];
    char stored_root[CLI_PATH_MAX_LEN] = "";
    const char *const stored = EMBED_CALL(slice_store, ServersSliceCtx, .key = key, .head = &head);
    const int known = stored != NULL;
    if (known)
    {
        // the stored view's openers are at most CLI_SERVERS_SLICE_OPENERS and its root below CLI_PATH_MAX_LEN bytes
        memor.cpy(&(MemoriaCfg){
            .dst = opener, .src = stored + sizeof(head), .bytes = (size_t)head.openers * sizeof(unsigned int)});
        memor.cpy(&(MemoriaCfg){.dst = stored_root,
                                .src = EMBED_CALL(slice_root, ServersSliceCtx, .stored = stored, .head_read = &head),
                                .bytes = (size_t)head.root_size});
    }
    // a file is first sliced as it opens, with the tree it opens in, and given whole as it opens; a change outside the
    // slice leaves the server's text as it was, and it is told nothing
    const unsigned long long version =
        (args->id != NULL)
            ? 1ULL
            : (EMBED_CALL(cli_lsp_path_out.version, CliLspPathOutCfg, .lsp = args->server, .path = path) + 1ULL);
    int fresh = 0;
    unsigned long long slice_size = 0ULL;
    char *slice;
    if ((args->root != NULL) || known)
    {
        slice = EMBED_CALL(slice_cut, ServersSliceCtx, .root = (args->root != NULL) ? args->root : stored_root,
                           .path = path, .key = key, .head = &head, .opener = opener, .text = text, .size = args->size,
                           .version = version, .always = args->id != NULL, .fresh = &fresh, .slice_size = &slice_size,
                           .err_msg = err_msg);
    }
    else
    {
        slice = NULL;
    }
    const int sent =
        (slice != NULL) &&
        (!fresh || ((args->id != NULL)
                        ? EMBED_CALL(cli_lsp_err_msg_text.open, CliLspErrMsgTextCfg, .lsp = args->server, .path = path,
                                     .language_id = args->id, .text = slice, .length = slice_size, .err_msg = err_msg)
                        : EMBED_CALL(cli_lsp_err_msg_text.change, CliLspErrMsgTextCfg, .lsp = args->server,
                                     .path = path, .text = slice, .length = slice_size, .err_msg = err_msg)));
    g_cli_guard->persistent_buf_release(slice, g_cli_servers_owner);
    if (sent && fresh)
    {
        EMBED_CALL(cli_servers_internal.request_note, CliServersCtx, .server_read = args->server);
    }
    return sent;
}

// A window's slice of a file the check gives its server in slices: the tree's root, the file's path, its path in the
// tree and its key; the window's head and openers, and its slice's lines; where each window goes; and whether the
// cells refused a window
struct CliServersCut
{
    const char *root;
    const char *path;
    char own[CLI_PATH_MAX_LEN];
    char key[CLI_PATH_MAX_LEN];
    CliServersSliceHead head;
    unsigned int opener[CLI_SERVERS_SLICE_OPENERS];
    CliServersSlicing slicing;
    CliServersWindow window;
    void *context;
    int refused;
};

// one line of a window's slice put after those before it
static void slices_line_append(const CliSliceLineCfg *args)
{
    EMBED_CALL(slice_line_append, CliSliceLineCfg, .context = &((CliServersCut *)args->context)->slicing,
               .path = args->path, .line = args->line, .text = args->text, .length = args->length);
}

// a window's slice handed on with its view, and its lines begun again for the next, the file's own path the first of
// its paths
static int slices_window(const CliSliceWindowCfg *args)
{
    CliServersCut *const cut = (CliServersCut *)args->context;
    const unsigned int named = (args->openers < CLI_SERVERS_SLICE_OPENERS) ? args->openers : CLI_SERVERS_SLICE_OPENERS;
    // the openers named are at most CLI_SERVERS_SLICE_OPENERS
    memor.cpy(&(MemoriaCfg){.dst = cut->opener, .src = args->opener, .bytes = (size_t)named * sizeof(unsigned int)});
    cut->head = (CliServersSliceHead){args->first, args->last, named, 0u, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL};
    cut->refused =
        !mmgr_line_ok(&cut->slicing.text) || !mmgr_line_ok(&cut->slicing.map) || !mmgr_line_ok(&cut->slicing.paths);
    const int going = !cut->refused && EMBED_CALL(cut->window, CliServersWindowCfg, .context = cut->context, .cut = cut,
                                                  .first = args->first, .last = args->last);
    cut->slicing.text.at = 0u;
    cut->slicing.map.at = 0u;
    cut->slicing.paths.at = 0u;
    cut->slicing.last_input = NULL;
    cut->slicing.last_position = CLI_JSON_NONE;
    cut->slicing.lines = 0u;
    (void)EMBED_CALL(slice_path_position, ServersSliceCtx, .slicing = &cut->slicing, .path = cut->own);
    return going;
}

int cli_servers_slices_cut(const CliServersPathSizeRootErrMsgCtx *args)
{
    const char *root = args->root;
    const char *path = args->path;
    CliServersWindow window = args->window;
    void *context = args->context;
    // a window's slice is cut and handed on where the walk reaches it, the walk's cells saved while it is
    CliServersCut *const cut = g_cli_guard->temporary_buf_alloc(sizeof(CliServersCut), g_cli_servers_owner);
    if (cut == NULL)
    {
        mmgr_line_put(args->err_msg, "the slices could not be cut: the cells cannot meet them");
        return 0;
    }
    cut->root = root;
    cut->path = path;
    cut->window = window;
    cut->context = context;
    cut->refused = 0;
    VerbaLinea own_line = {cut->own, sizeof(cut->own), 0ULL};
    EMBED_CALL(cli_servers_path_err_msg_internal.tree_path, CliServersPathErrMsgCtx, .tree = root, .path = path,
               .out = &own_line);
    EMBED_CALL(cli_servers_path_key_internal.key_get, CliServersPathKeyCtx, .path = path, .key_out = cut->key);
    const int made = EMBED_CALL(slice_lines_init, ServersSliceCtx, .slicing = &cut->slicing) &&
                     (mmgr_line_finish(&own_line) != 0ULL);
    if (made)
    {
        (void)EMBED_CALL(slice_path_position, ServersSliceCtx, .slicing = &cut->slicing, .path = cut->own);
    }
    const int walked = made && EMBED_CALL(cli_symbols_path_text_size_context_err_msg.slices_each,
                                          CliSymbolsPathTextSizeContextErrMsgCfg, .path = cut->own, .text = args->text,
                                          .size = args->size, .height = CLI_SERVERS_SLICE_HEIGHT,
                                          .each_slice_line = slices_line_append, .window = slices_window,
                                          .context = cut, .err_msg = args->err_msg);
    const int integer = walked && !cut->refused;
    // a walk that could not cut them gave why already
    if (!made || cut->refused)
    {
        mmgr_line_put(args->err_msg, "the slices could not be cut: the cells cannot meet them");
    }
    g_cli_guard->persistent_buf_release(cut->slicing.text.out, g_cli_servers_owner);
    g_cli_guard->persistent_buf_release(cut->slicing.map.out, g_cli_servers_owner);
    g_cli_guard->persistent_buf_release(cut->slicing.paths.out, g_cli_servers_owner);
    g_cli_guard->persistent_buf_release(cut, g_cli_servers_owner);
    return integer;
}

int cli_servers_cut_send(const CliServersErrMsgServerCtx *args)
{
    VerbaLinea *err_msg = args->err_msg;
    CliServersSliceHead head = args->cut->head;
    const unsigned long long version =
        (args->id != NULL)
            ? 1ULL
            : (EMBED_CALL(cli_lsp_path_out.version, CliLspPathOutCfg, .lsp = args->server, .path = args->cut->path) +
               1ULL);
    int fresh = 0;
    const int saved = EMBED_CALL(slice_save, ServersSliceCtx, .root = args->cut->root, .path = args->cut->path,
                                 .key = args->cut->key, .head = &head, .opener = args->cut->opener,
                                 .slicing_read = &args->cut->slicing, .version = version, .always = 1, .fresh = &fresh);
    const int sent =
        saved && ((args->id != NULL) ? EMBED_CALL(cli_lsp_err_msg_text.open, CliLspErrMsgTextCfg, .lsp = args->server,
                                                  .path = args->cut->path, .language_id = args->id,
                                                  .text = args->cut->slicing.text.out,
                                                  .length = args->cut->slicing.text.at, .err_msg = err_msg)
                                     : EMBED_CALL(cli_lsp_err_msg_text.change, CliLspErrMsgTextCfg, .lsp = args->server,
                                                  .path = args->cut->path, .text = args->cut->slicing.text.out,
                                                  .length = args->cut->slicing.text.at, .err_msg = err_msg));
    if (sent)
    {
        EMBED_CALL(cli_servers_internal.request_note, CliServersCtx, .server_read = args->server);
    }
    if (!saved)
    {
        mmgr_line_put(err_msg, "the slice could not be kept: the cells cannot meet it");
    }
    return sent;
}

int cli_servers_slice(const CliServersPathLspCtx *args)
{
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_servers_path_key_internal.key_get, CliServersPathKeyCtx, .path = args->path, .key_out = key);
    CliServersSliceHead head;
    const int sliced = EMBED_CALL(slice_store, ServersSliceCtx, .key = key, .head = &head) != NULL;
    return sliced;
}

void cli_servers_slice_close(const CliServersPathLspCtx *args)
{
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_servers_path_key_internal.key_get, CliServersPathKeyCtx, .path = args->path, .key_out = key);
    EMBED_CALL(cli_keyed_key_adjacent.remove, CliKeyedKeyAdjacentCfg, .keyed_out = &s_slices, .key = key);
    EMBED_CALL(cli_keyed_key_adjacent.remove, CliKeyedKeyAdjacentCfg, .keyed_out = &s_published, .key = key);
}

// One sliced file read for whether it owes its diagnostics, and whether any does
typedef struct
{
    int pending;
} CliServersOwing;

// a sliced file stored as `text`, `size` bytes, owing its diagnostics where its version is above the last published
static void slice_owing(const CliEachKeyAdjacentTextSizeCfg *args)
{
    const char *key = args->key;
    const char *adjacent = args->adjacent;
    unsigned long long size = args->size;
    (void)adjacent;
    CliServersOwing *const owing = (CliServersOwing *)args->context;
    CliServersSliceHead head;
    if (owing->pending || (size < sizeof(head)))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = &head, .src = args->text, .bytes = sizeof(head)});
    const char *ignored = NULL;
    unsigned long long published_size = 0ULL;
    const char *const published = EMBED_CALL(cli_keyed_key_adjacent.get, CliKeyedKeyAdjacentCfg, .keyed = &s_published,
                                             .key = key, .adjacent = &ignored, .size = &published_size);
    unsigned long long version = 0ULL;
    if ((published != NULL) && (published_size == sizeof(version)))
    {
        memor.cpy(&(MemoriaCfg){.dst = &version, .src = published, .bytes = sizeof(version)});
    }
    owing->pending = head.version > version;
}

int cli_servers_slices_pending(void)
{
    CliServersOwing owing = {0};
    EMBED_CALL(cli_keyed_each_table.each, CliKeyedEachCfg, .keyed = &s_slices, .each = slice_owing, .context = &owing);
    return owing.pending;
}

int cli_servers_view(const CliServersPathOutErrMsgCfg *args)
{
    const char *path = args->path;
    VerbaLinea *err_msg = args->err_msg;
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_servers_path_key_internal.key_get, CliServersPathKeyCtx, .path = path, .key_out = key);
    CliLsp *const server =
        EMBED_CALL(cli_servers_path_lsp_internal.slice, CliServersPathLspCtx, .path = path)
            ? EMBED_CALL(cli_servers_path_lsp_internal.owner_start, CliServersPathLspCtx, .path = path)
            : NULL;
    unsigned long long size = 0ULL;
    char *const text = (server != NULL) ? EMBED_CALL(cli_servers_path_size_kind.text_alloc, CliServersPathSizeKindCfg,
                                                     .path = path, .size_pointer = &size)
                                        : NULL;
    if (text == NULL)
    {
        if (server != NULL)
        {
            EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
        }
        mmgr_line_put(args->out, "false");
        return 1;
    }
    const unsigned int named = (args->openers < CLI_SERVERS_SLICE_OPENERS) ? args->openers : CLI_SERVERS_SLICE_OPENERS;
    CliServersSliceHead head;
    const char *const stored = EMBED_CALL(slice_store, ServersSliceCtx, .key = key, .head = &head);
    const int known = stored != NULL;
    // a view the server was last given its slice of is cut again only as the text changes
    int same = known && (head.first == args->first) && (head.last == args->last) && (head.openers == named);
    unsigned int at = 0u;
    while (same && (at < named))
    {
        unsigned int was = 0u;
        memor.cpy(&(MemoriaCfg){.dst = &was,
                                .src = stored + sizeof(head) + ((unsigned long long)at * sizeof(unsigned int)),
                                .bytes = sizeof(was)});
        same = was == args->opener[at];
        at += 1u;
    }
    char root[CLI_PATH_MAX_LEN];
    if (known && !same)
    {
        // the stored root is below CLI_PATH_MAX_LEN bytes
        memor.cpy(&(MemoriaCfg){.dst = root,
                                .src = EMBED_CALL(slice_root, ServersSliceCtx, .stored = stored, .head_read = &head),
                                .bytes = (size_t)head.root_size});
    }
    // a view whose slice is the slice the server was last given leaves the server's text as it was
    int sent = 1;
    int cut = 0;
    if (known && !same)
    {
        head = (CliServersSliceHead){args->first, (args->last >= args->first) ? args->last : args->first,
                                     named,       0u,
                                     0ULL,        0ULL,
                                     head.cut,    head.hash,
                                     head.version};
        unsigned long long slice_size = 0ULL;
        char *const slice = EMBED_CALL(
            slice_cut, ServersSliceCtx, .root = root, .path = path, .key = key, .head = &head, .opener = args->opener,
            .text = text, .size = size,
            .version = EMBED_CALL(cli_lsp_path_out.version, CliLspPathOutCfg, .lsp = server, .path = path) + 1ULL,
            .always = 0, .fresh = &cut, .slice_size = &slice_size, .err_msg = err_msg);
        cut = (slice != NULL) && cut;
        sent = (slice != NULL) &&
               (!cut || EMBED_CALL(cli_lsp_err_msg_text.change, CliLspErrMsgTextCfg, .lsp = server, .path = path,
                                   .text = slice, .length = slice_size, .err_msg = err_msg));
        g_cli_guard->persistent_buf_release(slice, g_cli_servers_owner);
    }
    if (sent && cut)
    {
        EMBED_CALL(cli_servers_internal.request_note, CliServersCtx, .server_read = server);
    }
    g_cli_guard->persistent_buf_release(text, g_cli_servers_owner);
    EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
    if (!sent)
    {
        return 0;
    }
    mmgr_line_put(args->out, cut ? "true" : "false");
    return 1;
}

int cli_servers_slice_in(const CliServersPathLineCtx *args)
{
    unsigned int *into = args->into;
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_servers_path_key_internal.key_get, CliServersPathKeyCtx, .path = args->path, .key_out = key);
    CliServersSliceHead head;
    const char *const stored = EMBED_CALL(slice_store, ServersSliceCtx, .key = key, .head = &head);
    *into = args->line;
    const int found = (stored == NULL) || EMBED_CALL(slice_line_in, ServersSliceCtx, .stored = stored,
                                                     .head_read = &head, .line = args->line, .into = into);
    return found;
}

int cli_servers_slice_out(const CliServersPathLineFromCtx *args)
{
    unsigned int *line = args->line_pointer;
    VerbaLinea from_line = {args->from_capacity, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&from_line, args->path);
    (void)mmgr_line_finish(&from_line);
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_servers_path_key_internal.key_get, CliServersPathKeyCtx, .path = args->path, .key_out = key);
    CliServersSliceHead head;
    const char *const stored = EMBED_CALL(slice_store, ServersSliceCtx, .key = key, .head = &head);
    int moved = 1;
    if (stored != NULL)
    {
        const CliServersSliceLine one =
            (*line < head.lines)
                ? EMBED_CALL(slice_line_at, ServersSliceCtx, .stored = stored, .head_read = &head, .at = *line)
                : (CliServersSliceLine){CLI_JSON_NONE, 0u};
        if (one.path == CLI_JSON_NONE)
        {
            moved = 0;
        }
        else if (one.path == 0u)
        {
            moved = 1;
        }
        else
        {
            moved = 2;
        }
        *line = one.line;
        if (moved == 2)
        {
            const char *const root = EMBED_CALL(slice_root, ServersSliceCtx, .stored = stored, .head_read = &head);
            EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = root + head.root_size + one.path,
                       .joined = args->from_capacity);
        }
    }
    return moved;
}

// the object `one` written with the lines `from` and `to` in place of those of its range, its other members as they are
EMBED_INLINE void slice_ranged_write(const ServersSliceCtx *args)
{
    const CliJson *json = args->json;
    VerbaLinea *out = args->out;
    const unsigned int range = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->one, .name = "range");
    const CliTextPosition start =
        EMBED_CALL(cli_servers_json_node_otherwise_internal.place, CliServersJsonNodeOtherwiseCtx, .json = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = range, .name = "start"));
    const CliTextPosition end =
        EMBED_CALL(cli_servers_json_node_otherwise_internal.place, CliServersJsonNodeOtherwiseCtx, .json = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = range, .name = "end"));
    mmgr_line_put(out, "{\"range\":{\"start\":{\"line\":");
    mmgr_line_decimal(out, args->from, 1u);
    mmgr_line_put(out, ",\"character\":");
    mmgr_line_decimal(out, start.col, 1u);
    mmgr_line_put(out, "},\"end\":{\"line\":");
    mmgr_line_decimal(out, args->to, 1u);
    mmgr_line_put(out, ",\"character\":");
    mmgr_line_decimal(out, end.col, 1u);
    mmgr_line_put(out, "}}");
    for (unsigned int member = json->node[args->one].first; member != CLI_JSON_NONE; member = json->node[member].next)
    {
        const char *const name = json->text + json->node[member].key;
        if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "range", .cap = CLI_POOL_BYTES}))
        {
            continue;
        }
        mmgr_line_ch(out, ',');
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = name,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}));
        mmgr_line_ch(out, ':');
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = member,
                   .out = out, .indent = 0u, .sorted = 0);
    }
    mmgr_line_ch(out, '}');
}

int cli_servers_slice_ranged_in(const CliServersPathJsonCtx *args)
{
    const unsigned int range = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = args->json, .object = args->one, .name = "range");
    const CliTextPosition start =
        EMBED_CALL(cli_servers_json_node_otherwise_internal.place, CliServersJsonNodeOtherwiseCtx, .json = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = range, .name = "start"));
    const CliTextPosition end =
        EMBED_CALL(cli_servers_json_node_otherwise_internal.place, CliServersJsonNodeOtherwiseCtx, .json = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = range, .name = "end"));
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_servers_path_key_internal.key_get, CliServersPathKeyCtx, .path = args->path, .key_out = key);
    CliServersSliceHead head;
    const char *const stored = EMBED_CALL(slice_store, ServersSliceCtx, .key = key, .head = &head);
    unsigned int from = start.line;
    unsigned int to = end.line;
    const int inside = (stored == NULL) || (EMBED_CALL(slice_line_in, ServersSliceCtx, .stored = stored,
                                                       .head_read = &head, .line = start.line, .into = &from) &&
                                            EMBED_CALL(slice_line_in, ServersSliceCtx, .stored = stored,
                                                       .head_read = &head, .line = end.line, .into = &to));
    if (inside)
    {
        EMBED_CALL(slice_ranged_write, ServersSliceCtx, .json = args->json, .one = args->one, .from = from, .to = to,
                   .out = args->out);
    }
    return inside;
}

// the number `length` bytes of `text` write, `otherwise` where they write none
EMBED_INLINE unsigned long long slice_number(const ServersSliceCtx *args)
{
    const char *text = args->text;
    unsigned long long value = 0ULL;
    int any = 0;
    const unsigned long long lead =
        EMBED_CALL(cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg, .text = text, .size = args->length);
    unsigned long long end = lead;
    value = EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = text, .at = lead, .size = args->length,
                       .end = &end);
    any = end > lead;
    return any ? value : args->otherwise;
}

char *cli_servers_slice_diagnostics(const CliServersPathSizeCtx *args)
{
    unsigned long long size = args->size;
    unsigned long long *mapped_size = args->mapped_size;
    // diagnostics of a version given before the slice stored was are of a slice the map no longer gives; a later
    // version given with no slice stored for it is the same text given again
    unsigned long long version_at = 0ULL;
    unsigned long long version_length = 0ULL;
    const int versioned =
        EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->params_text, .size_value = args->params_size,
                   .name = "version", .start = &version_at, .length_pointer = &version_length);
    const unsigned long long version =
        versioned ? EMBED_CALL(slice_number, ServersSliceCtx, .text = args->params_text + version_at,
                               .length = version_length, .otherwise = 0ULL)
                  : 0ULL;
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_servers_path_key_internal.key_get, CliServersPathKeyCtx, .path = args->path, .key_out = key);
    CliServersSliceHead head = {0u};
    const char *const found = EMBED_CALL(slice_store, ServersSliceCtx, .key = key, .head = &head);
    const char *const stored = ((found != NULL) && (!versioned || (version >= head.version))) ? found : NULL;
    if ((stored != NULL) && versioned)
    {
        (void)EMBED_CALL(cli_keyed_key_adjacent.put, CliKeyedKeyAdjacentCfg, .keyed_out = &s_published, .key = key,
                         .adjacent_value = "", .text = (const char *)&version, .size_value = sizeof(version));
    }
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    char quiet[256];
    VerbaLinea ignored = {quiet, sizeof(quiet), 0ULL};
    const unsigned int array =
        (stored != NULL) ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg,
                                      .json = &json, .text = args->all, .size = size, .nodes_more = 0u,
                                      .text_more = 0ULL, .err_msg = &ignored, .owner = g_cli_servers_owner)
                         : CLI_JSON_NONE;
    // a diagnostic's numbers written again are at most as long again as those it came with
    const unsigned long long mapped_capacity = (2ULL * size) + CLI_SERVERS_SLICE_MAPPED_MORE;
    char *const mapped_cell = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json,
                                         .node = array, .kind = CFG_JSON_ARRAY)
                                  ? g_cli_guard->temporary_buf_alloc(mapped_capacity, args->owner)
                                  : NULL;
    VerbaLinea mapped = mmgr_line_over(mapped_cell, mapped_capacity);
    const int made = mapped_cell != NULL;
    if (made)
    {
        mmgr_line_ch(&mapped, '[');
        int written = 0;
        for (unsigned int one = json.node[array].first; one != CLI_JSON_NONE; one = json.node[one].next)
        {
            const unsigned int range = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                  .json_read = &json, .object = one, .name = "range");
            const CliTextPosition start = EMBED_CALL(
                cli_servers_json_node_otherwise_internal.place, CliServersJsonNodeOtherwiseCtx, .json = &json,
                .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                   .object = range, .name = "start"));
            const CliTextPosition end = EMBED_CALL(
                cli_servers_json_node_otherwise_internal.place, CliServersJsonNodeOtherwiseCtx, .json = &json,
                .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                   .object = range, .name = "end"));
            const CliServersSliceLine from =
                (start.line < head.lines)
                    ? EMBED_CALL(slice_line_at, ServersSliceCtx, .stored = stored, .head_read = &head, .at = start.line)
                    : (CliServersSliceLine){CLI_JSON_NONE, 0u};
            const CliServersSliceLine to =
                (end.line < head.lines)
                    ? EMBED_CALL(slice_line_at, ServersSliceCtx, .stored = stored, .head_read = &head, .at = end.line)
                    : (CliServersSliceLine){CLI_JSON_NONE, 0u};
            // a diagnostic on a line of another file, or on a bracket the slice closes, is of the slice and not the
            // file
            if ((from.path != 0u) || (to.path != 0u) || (to.line < from.line))
            {
                continue;
            }
            mmgr_line_put(&mapped, written ? "," : "");
            EMBED_CALL(slice_ranged_write, ServersSliceCtx, .json = &json, .one = one, .from = from.line, .to = to.line,
                       .out = &mapped);
            written = 1;
        }
        mmgr_line_ch(&mapped, ']');
    }
    g_cli_guard->persistent_buf_release(json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    if (!made || !mmgr_line_ok(&mapped))
    {
        g_cli_guard->persistent_buf_release(mapped.out, args->owner);
        return NULL;
    }
    *mapped_size = mapped.at;
    return mapped.out;
}
