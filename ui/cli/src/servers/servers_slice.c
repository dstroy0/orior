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
#include "servers_internal.h"

// the last line in view of a file the page has not handed its view of, and the lines of each window the check slices a
// file into, a view's height; the most lines a view's openers name, the most files sliced at once, and the bytes a
// slice's text, map and paths are each begun in, doubling as they fill
#define CLI_SERVERS_SLICE_LAST 63u
#define CLI_SERVERS_SLICE_HEIGHT (CLI_SERVERS_SLICE_LAST + 1u)
#define CLI_SERVERS_SLICE_OPENERS 64u
#define CLI_SERVERS_SLICES 256u
#define CLI_SERVERS_SLICE_FIRST_BYTES 4096u

// One line of a slice: the place of the path it came from among the slice's paths, CLI_JSON_NONE for none, and its
// line there
typedef struct
{
    unsigned int path;
    unsigned int line;
} CliServersSliceLine;

// A sliced file's head as it is stored: its view's first and last lines and how many lines open regions, how many lines
// its slice has, the bytes of the tree's root and of the paths after it, the number of the cut it is, a cut whose text
// is the last's keeping its number, its text's hash, and the version of the file it was given its server as. The
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

// the sliced files, by their keys with their paths, the last version each was published diagnostics of, and how many
// slices were cut, each cut's number, under the slices' lock; and the lock a slice is cut, stored and given its server
// under
static CliKeyed s_slices = {"slices", {NULL}, 0u, CLI_SERVERS_SLICES};
static CliKeyed s_published = {"slices published", {NULL}, 0u, CLI_SERVERS_SLICES};
static unsigned long long s_cuts;
static CliLock s_slices_lock = CLI_LOCK_INIT;
static CliLock s_sending_lock = CLI_LOCK_INIT;

// A slice being cut: its text, its lines' map and the paths they came from, each a growing line; the path the walk gave
// the last line as and its place among the paths; and how many lines are written
typedef struct
{
    VerbaLinea text;
    VerbaLinea map;
    VerbaLinea paths;
    const char *last_given;
    unsigned int last_place;
    unsigned int lines;
} CliServersSlicing;

// the place among the paths of the slice being cut of `path`, put after them where it is not among them, CLI_JSON_NONE
// for none
static unsigned int slice_path_place(CliServersSlicing *slicing, const char *path)
{
    if (path == NULL)
    {
        return CLI_JSON_NONE;
    }
    if (path == slicing->last_given)
    {
        return slicing->last_place;
    }
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    unsigned long long at = 0ull;
    int found = 0;
    while (!found && (at < slicing->paths.at))
    {
        const char *const one = slicing->paths.out + at;
        const unsigned long long one_size = cellul.len(&(CatenaFinitaCfg){.src = one, .cap = CLI_CELLS_POOL});
        found = (one_size == size) && (memor.cmp(&(MemoriaCfg){.src = one, .other = path, .bytes = (size_t)size}) == 0);
        at += found ? 0ull : (one_size + 1ull);
    }
    if (!found)
    {
        // a path is below CLI_PATH_MAX_LEN bytes
        mmgr_line_put_n(&slicing->paths, path, (size_t)size);
        mmgr_line_ch(&slicing->paths, '\0');
    }
    slicing->last_given = path;
    // the paths of one slice are below 2^32 bytes, its lines naming few files
    slicing->last_place = (unsigned int)at;
    return slicing->last_place;
}

// one line of the slice put after those before it, with where it came from
static void slice_line_put(void *context, const char *path, unsigned int line, const char *text,
                           unsigned long long length)
{
    CliServersSlicing *const slicing = context;
    // a line of a text is below 2^32 bytes
    mmgr_line_put_n(&slicing->text, text, (size_t)length);
    mmgr_line_ch(&slicing->text, '\n');
    const CliServersSliceLine one = {slice_path_place(slicing, path), line};
    mmgr_line_put_n(&slicing->map, (const char *)&one, sizeof(one));
    slicing->lines += 1u;
}

// the slicing's lines given back
static void slice_lines_given(CliServersSlicing *slicing)
{
    g_cli_cells->persistent_buf_release(slicing->text.out, &g_cli_servers_cells);
    g_cli_cells->persistent_buf_release(slicing->map.out, &g_cli_servers_cells);
    g_cli_cells->persistent_buf_release(slicing->paths.out, &g_cli_servers_cells);
}

// the slicing's lines made, each growing, none written: 1, or 0 where the cells cannot meet them, those made given back
// with the rest by slice_lines_given
static int slice_lines_made(CliServersSlicing *slicing)
{
    *slicing = (CliServersSlicing){{NULL, 0ull, 0ull, NULL, NULL, 0ull},
                                   {NULL, 0ull, 0ull, NULL, NULL, 0ull},
                                   {NULL, 0ull, 0ull, NULL, NULL, 0ull},
                                   NULL,
                                   CLI_JSON_NONE,
                                   0u};
    return cli_growing_line_made(&slicing->text, CLI_SERVERS_SLICE_FIRST_BYTES, &g_cli_servers_cells) &&
           cli_growing_line_made(&slicing->map, CLI_SERVERS_SLICE_FIRST_BYTES, &g_cli_servers_cells) &&
           cli_growing_line_made(&slicing->paths, CLI_SERVERS_SLICE_FIRST_BYTES, &g_cli_servers_cells);
}

// the slice `slicing` holds stored by `key` with the view of `head`, its openers `opener`, and its map, the file at
// `path` in the tree at `root`, under the slices' lock: its number and version `head`'s where its text is the text
// `head` was cut as and it is not given `always`, else the next number and `version`, the version it is to be given as,
// with `*fresh` 1: 1, or 0 where the cells cannot meet it
static int slice_kept(const char *root, const char *path, const char *key, CliServersSliceHead *head,
                      const unsigned int *opener, const CliServersSlicing *slicing, unsigned long long version,
                      int always, int *fresh)
{
    head->lines = slicing->lines;
    head->root_size = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1ull;
    head->paths_size = slicing->paths.at;
    const unsigned long long hash = cli_servers_hash_of(slicing->text.out, slicing->text.at);
    *fresh = always || (head->cut == 0ull) || (head->hash != hash);
    cli_lock_take(&s_slices_lock);
    // a file opened afresh has had nothing published of it by the server it is opened in
    if (*fresh && (version == 1ull))
    {
        cli_keyed_remove(&s_published, key);
    }
    s_cuts += *fresh ? 1ull : 0ull;
    head->cut = *fresh ? s_cuts : head->cut;
    head->version = *fresh ? version : head->version;
    head->hash = hash;
    const unsigned long long openers_size = (unsigned long long)head->openers * sizeof(unsigned int);
    const unsigned long long stored_size =
        sizeof(*head) + openers_size + slicing->map.at + head->root_size + head->paths_size;
    char *const stored = g_cli_cells->temporary_buf_alloc(stored_size, &g_cli_servers_cells);
    if (stored != NULL)
    {
        unsigned long long at = 0ull;
        memor.cpy(&(MemoriaCfg){.dst = stored, .src = head, .bytes = sizeof(*head)});
        at += sizeof(*head);
        // the openers are at most CLI_SERVERS_SLICE_OPENERS, and each part below was written into a cell of its size
        memor.cpy(&(MemoriaCfg){.dst = stored + at, .src = opener, .bytes = (size_t)openers_size});
        at += openers_size;
        memor.cpy(&(MemoriaCfg){.dst = stored + at, .src = slicing->map.out, .bytes = slicing->map.at});
        at += slicing->map.at;
        memor.cpy(&(MemoriaCfg){.dst = stored + at, .src = root, .bytes = (size_t)head->root_size});
        at += head->root_size;
        memor.cpy(&(MemoriaCfg){.dst = stored + at, .src = slicing->paths.out, .bytes = slicing->paths.at});
    }
    const int kept = (stored != NULL) && cli_keyed_put(&s_slices, key, path, stored, stored_size);
    cli_lock_give(&s_slices_lock);
    g_cli_cells->persistent_buf_release(stored, &g_cli_servers_cells);
    return kept;
}

// the slice of the file at `path` in the tree at `root` cut from `size` bytes of `text` for the view of `head`, its
// openers `opener`, and stored by `key` with the view and its map as slice_kept stores it, to be given as `version`
// where `*fresh` is 1: its text, in a cell the caller gives back with g_cli_cells->persistent_buf_release, its size
// into `slice_size`, or NULL with why given
static char *slice_cut(const char *root, const char *path, const char *key, CliServersSliceHead *head,
                       const unsigned int *opener, const char *text, unsigned long long size,
                       unsigned long long version, int always, int *fresh, unsigned long long *slice_size,
                       VerbaLinea *err_msg)
{
    char own[CLI_PATH_MAX_LEN];
    VerbaLinea own_line = {own, sizeof(own), 0ull};
    cli_servers_tree_path(root, path, &own_line);
    CliServersSlicing slicing;
    const int made = slice_lines_made(&slicing) && (mmgr_line_finish(&own_line) != 0ull);
    if (made)
    {
        (void)slice_path_place(&slicing, own);
    }
    const int cut = made && cli_symbols_slice_each(own, text, size, head->first, head->last, opener, head->openers,
                                                   slice_line_put, &slicing, err_msg);
    const int whole = cut && mmgr_line_ok(&slicing.text) && mmgr_line_ok(&slicing.map) && mmgr_line_ok(&slicing.paths);
    const int kept = whole && slice_kept(root, path, key, head, opener, &slicing, version, always, fresh);
    if (!kept)
    {
        // a walk that could not cut it gave why already
        if (!made || cut)
        {
            mmgr_line_put(err_msg, "the slice could not be cut: the cells cannot meet it");
        }
        slice_lines_given(&slicing);
        return NULL;
    }
    *slice_size = slicing.text.at;
    g_cli_cells->persistent_buf_release(slicing.map.out, &g_cli_servers_cells);
    g_cli_cells->persistent_buf_release(slicing.paths.out, &g_cli_servers_cells);
    return slicing.text.out;
}

// the slice stored by `key` read under the slices' lock, its head into `head`: its bytes, NULL where none is stored
static const char *slice_stored(const char *key, CliServersSliceHead *head)
{
    const char *beside = NULL;
    unsigned long long size = 0ull;
    const char *const stored = cli_keyed_get(&s_slices, key, &beside, &size);
    if ((stored == NULL) || (size < sizeof(*head)))
    {
        return NULL;
    }
    memor.cpy(&(MemoriaCfg){.dst = head, .src = stored, .bytes = sizeof(*head)});
    return stored;
}

// the line at `at` of the slice stored as `stored` with `head`
static CliServersSliceLine slice_line_at(const char *stored, const CliServersSliceHead *head, unsigned int at)
{
    CliServersSliceLine one = {CLI_JSON_NONE, 0u};
    const unsigned long long place = sizeof(*head) + ((unsigned long long)head->openers * sizeof(unsigned int)) +
                                     ((unsigned long long)at * sizeof(one));
    memor.cpy(&(MemoriaCfg){.dst = &one, .src = stored + place, .bytes = sizeof(one)});
    return one;
}

// the tree's root of the slice stored as `stored` with `head`, and the paths after it
static const char *slice_root(const char *stored, const CliServersSliceHead *head)
{
    return stored + sizeof(*head) + ((unsigned long long)head->openers * sizeof(unsigned int)) +
           ((unsigned long long)head->lines * sizeof(CliServersSliceLine));
}

// the line of the slice stored as `stored` with `head` that came from the file's own line `line`: 1 with it in
// `into`, or 0 where none did
static int slice_line_in(const char *stored, const CliServersSliceHead *head, unsigned int line, unsigned int *into)
{
    for (unsigned int at = 0u; at < head->lines; at += 1u)
    {
        const CliServersSliceLine one = slice_line_at(stored, head, at);
        if ((one.path == 0u) && (one.line == line))
        {
            *into = at;
            return 1;
        }
    }
    return 0;
}

int cli_servers_slice_sent(CliLsp *server, const char *root, const char *path, const char *id, const char *text,
                           unsigned long long size, VerbaLinea *err_msg)
{
    char key[CLI_PATH_MAX_LEN];
    cli_servers_key_of(path, key);
    cli_lock_take(&s_sending_lock);
    cli_lock_take(&s_slices_lock);
    CliServersSliceHead head = {0u, CLI_SERVERS_SLICE_LAST, 0u, 0u, 0ull, 0ull, 0ull, 0ull, 0ull};
    unsigned int opener[CLI_SERVERS_SLICE_OPENERS];
    char stored_root[CLI_PATH_MAX_LEN] = "";
    const char *const stored = slice_stored(key, &head);
    const int known = stored != NULL;
    if (known)
    {
        // the stored view's openers are at most CLI_SERVERS_SLICE_OPENERS and its root below CLI_PATH_MAX_LEN bytes
        memor.cpy(&(MemoriaCfg){
            .dst = opener, .src = stored + sizeof(head), .bytes = (size_t)head.openers * sizeof(unsigned int)});
        memor.cpy(&(MemoriaCfg){.dst = stored_root, .src = slice_root(stored, &head), .bytes = (size_t)head.root_size});
    }
    cli_lock_give(&s_slices_lock);
    // a file is first sliced as it opens, with the tree it opens in, and given whole as it opens; a change outside the
    // slice leaves the server's text as it was, and it is told nothing
    const unsigned long long version = (id != NULL) ? 1ull : (cli_lsp_version(server, path) + 1ull);
    int fresh = 0;
    unsigned long long slice_size = 0ull;
    char *const slice = ((root != NULL) || known)
                            ? slice_cut((root != NULL) ? root : stored_root, path, key, &head, opener, text, size,
                                        version, id != NULL, &fresh, &slice_size, err_msg)
                            : NULL;
    const int sent =
        (slice != NULL) && (!fresh || ((id != NULL) ? cli_lsp_opened(server, path, id, slice, slice_size, err_msg)
                                                    : cli_lsp_changed(server, path, slice, slice_size, err_msg)));
    cli_lock_give(&s_sending_lock);
    g_cli_cells->persistent_buf_release(slice, &g_cli_servers_cells);
    if (sent && fresh)
    {
        cli_servers_ask_noted(server);
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
static void slices_line_put(void *context, const char *path, unsigned int line, const char *text,
                            unsigned long long length)
{
    slice_line_put(&((CliServersCut *)context)->slicing, path, line, text, length);
}

// a window's slice handed on with its view, and its lines begun again for the next, the file's own path the first of
// its paths
static int slices_window(void *context, unsigned int first, unsigned int last, const unsigned int *opener,
                         unsigned int openers)
{
    CliServersCut *const cut = (CliServersCut *)context;
    const unsigned int named = (openers < CLI_SERVERS_SLICE_OPENERS) ? openers : CLI_SERVERS_SLICE_OPENERS;
    // the openers named are at most CLI_SERVERS_SLICE_OPENERS
    memor.cpy(&(MemoriaCfg){.dst = cut->opener, .src = opener, .bytes = (size_t)named * sizeof(unsigned int)});
    cut->head = (CliServersSliceHead){first, last, named, 0u, 0ull, 0ull, 0ull, 0ull, 0ull};
    cut->refused =
        !mmgr_line_ok(&cut->slicing.text) || !mmgr_line_ok(&cut->slicing.map) || !mmgr_line_ok(&cut->slicing.paths);
    const int going = !cut->refused && cut->window(cut->context, cut, first, last);
    cut->slicing.text.at = 0u;
    cut->slicing.map.at = 0u;
    cut->slicing.paths.at = 0u;
    cut->slicing.last_given = NULL;
    cut->slicing.last_place = CLI_JSON_NONE;
    cut->slicing.lines = 0u;
    (void)slice_path_place(&cut->slicing, cut->own);
    return going;
}

int cli_servers_slices_cut(const char *root, const char *path, const char *text, unsigned long long size,
                           CliServersWindow window, void *context, VerbaLinea *err_msg)
{
    // a window's slice is cut and handed on where the walk reaches it, the walk's cells kept while it is
    CliServersCut *const cut = g_cli_cells->temporary_buf_alloc(sizeof(CliServersCut), &g_cli_servers_cells);
    if (cut == NULL)
    {
        mmgr_line_put(err_msg, "the slices could not be cut: the cells cannot meet them");
        return 0;
    }
    cut->root = root;
    cut->path = path;
    cut->window = window;
    cut->context = context;
    cut->refused = 0;
    VerbaLinea own_line = {cut->own, sizeof(cut->own), 0ull};
    cli_servers_tree_path(root, path, &own_line);
    cli_servers_key_of(path, cut->key);
    const int made = slice_lines_made(&cut->slicing) && (mmgr_line_finish(&own_line) != 0ull);
    if (made)
    {
        (void)slice_path_place(&cut->slicing, cut->own);
    }
    const int walked = made && cli_symbols_slices_each(cut->own, text, size, CLI_SERVERS_SLICE_HEIGHT, slices_line_put,
                                                       slices_window, cut, err_msg);
    const int whole = walked && !cut->refused;
    // a walk that could not cut them gave why already
    if (!made || cut->refused)
    {
        mmgr_line_put(err_msg, "the slices could not be cut: the cells cannot meet them");
    }
    slice_lines_given(&cut->slicing);
    g_cli_cells->persistent_buf_release(cut, &g_cli_servers_cells);
    return whole;
}

int cli_servers_cut_sent(CliLsp *server, const char *id, const CliServersCut *cut, VerbaLinea *err_msg)
{
    cli_lock_take(&s_sending_lock);
    CliServersSliceHead head = cut->head;
    const unsigned long long version = (id != NULL) ? 1ull : (cli_lsp_version(server, cut->path) + 1ull);
    int fresh = 0;
    const int kept = slice_kept(cut->root, cut->path, cut->key, &head, cut->opener, &cut->slicing, version, 1, &fresh);
    const int sent =
        kept &&
        ((id != NULL) ? cli_lsp_opened(server, cut->path, id, cut->slicing.text.out, cut->slicing.text.at, err_msg)
                      : cli_lsp_changed(server, cut->path, cut->slicing.text.out, cut->slicing.text.at, err_msg));
    cli_lock_give(&s_sending_lock);
    if (sent)
    {
        cli_servers_ask_noted(server);
    }
    if (!kept)
    {
        mmgr_line_put(err_msg, "the slice could not be kept: the cells cannot meet it");
    }
    return sent;
}

int cli_servers_sliced(const char *path)
{
    char key[CLI_PATH_MAX_LEN];
    cli_servers_key_of(path, key);
    cli_lock_take(&s_slices_lock);
    CliServersSliceHead head;
    const int sliced = slice_stored(key, &head) != NULL;
    cli_lock_give(&s_slices_lock);
    return sliced;
}

void cli_servers_slice_closed(const char *path)
{
    char key[CLI_PATH_MAX_LEN];
    cli_servers_key_of(path, key);
    cli_lock_take(&s_sending_lock);
    cli_lock_take(&s_slices_lock);
    cli_keyed_remove(&s_slices, key);
    cli_keyed_remove(&s_published, key);
    cli_lock_give(&s_slices_lock);
    cli_lock_give(&s_sending_lock);
}

// One sliced file read for whether it owes its diagnostics, and whether any does
typedef struct
{
    int owed;
} CliServersOwing;

// a sliced file stored as `text`, `size` bytes, owing its diagnostics where its version is above the last published
static void slice_owing(void *context, const char *key, const char *beside, const char *text, unsigned long long size)
{
    (void)beside;
    CliServersOwing *const owing = (CliServersOwing *)context;
    CliServersSliceHead head;
    if (owing->owed || (size < sizeof(head)))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = &head, .src = text, .bytes = sizeof(head)});
    const char *ignored = NULL;
    unsigned long long published_size = 0ull;
    const char *const published = cli_keyed_get(&s_published, key, &ignored, &published_size);
    unsigned long long version = 0ull;
    if ((published != NULL) && (published_size == sizeof(version)))
    {
        memor.cpy(&(MemoriaCfg){.dst = &version, .src = published, .bytes = sizeof(version)});
    }
    owing->owed = head.version > version;
}

int cli_servers_slices_owed(void)
{
    CliServersOwing owing = {0};
    cli_lock_take(&s_slices_lock);
    cli_keyed_each(&s_slices, slice_owing, &owing);
    cli_lock_give(&s_slices_lock);
    return owing.owed;
}

int cli_servers_view(const char *path, unsigned int first, unsigned int last, const unsigned int *opener,
                     unsigned int openers, VerbaLinea *out, VerbaLinea *err_msg)
{
    char key[CLI_PATH_MAX_LEN];
    cli_servers_key_of(path, key);
    CliLsp *const server = cli_servers_sliced(path) ? cli_servers_owner_started(path) : NULL;
    unsigned long long size = 0ull;
    char *const text = (server != NULL) ? cli_servers_text_taken(path, &size) : NULL;
    if (text == NULL)
    {
        if (server != NULL)
        {
            cli_lsp_unref(server);
        }
        mmgr_line_put(out, "false");
        return 1;
    }
    const unsigned int named = (openers < CLI_SERVERS_SLICE_OPENERS) ? openers : CLI_SERVERS_SLICE_OPENERS;
    cli_lock_take(&s_sending_lock);
    cli_lock_take(&s_slices_lock);
    CliServersSliceHead head;
    const char *const stored = slice_stored(key, &head);
    const int known = stored != NULL;
    // a view the server was last given its slice of is cut again only as the text changes
    int same = known && (head.first == first) && (head.last == last) && (head.openers == named);
    for (unsigned int at = 0u; same && (at < named); at += 1u)
    {
        unsigned int was = 0u;
        memor.cpy(&(MemoriaCfg){.dst = &was,
                                .src = stored + sizeof(head) + ((unsigned long long)at * sizeof(unsigned int)),
                                .bytes = sizeof(was)});
        same = was == opener[at];
    }
    char root[CLI_PATH_MAX_LEN];
    if (known && !same)
    {
        // the stored root is below CLI_PATH_MAX_LEN bytes
        memor.cpy(&(MemoriaCfg){.dst = root, .src = slice_root(stored, &head), .bytes = (size_t)head.root_size});
    }
    cli_lock_give(&s_slices_lock);
    // a view whose slice is the slice the server was last given leaves the server's text as it was
    int sent = 1;
    int cut = 0;
    if (known && !same)
    {
        head = (CliServersSliceHead){
            first, (last >= first) ? last : first, named, 0u, 0ull, 0ull, head.cut, head.hash, head.version};
        unsigned long long slice_size = 0ull;
        char *const slice = slice_cut(root, path, key, &head, opener, text, size, cli_lsp_version(server, path) + 1ull,
                                      0, &cut, &slice_size, err_msg);
        cut = (slice != NULL) && cut;
        sent = (slice != NULL) && (!cut || cli_lsp_changed(server, path, slice, slice_size, err_msg));
        g_cli_cells->persistent_buf_release(slice, &g_cli_servers_cells);
    }
    cli_lock_give(&s_sending_lock);
    if (sent && cut)
    {
        cli_servers_ask_noted(server);
    }
    g_cli_cells->persistent_buf_release(text, &g_cli_servers_cells);
    cli_lsp_unref(server);
    if (!sent)
    {
        return 0;
    }
    mmgr_line_put(out, cut ? "true" : "false");
    return 1;
}

int cli_servers_slice_in(const char *path, unsigned int line, unsigned int *into)
{
    char key[CLI_PATH_MAX_LEN];
    cli_servers_key_of(path, key);
    cli_lock_take(&s_slices_lock);
    CliServersSliceHead head;
    const char *const stored = slice_stored(key, &head);
    *into = line;
    const int found = (stored == NULL) || slice_line_in(stored, &head, line, into);
    cli_lock_give(&s_slices_lock);
    return found;
}

int cli_servers_slice_out(const char *path, unsigned int *line, char *from)
{
    VerbaLinea from_line = {from, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&from_line, path);
    (void)mmgr_line_finish(&from_line);
    char key[CLI_PATH_MAX_LEN];
    cli_servers_key_of(path, key);
    cli_lock_take(&s_slices_lock);
    CliServersSliceHead head;
    const char *const stored = slice_stored(key, &head);
    int moved = 1;
    if (stored != NULL)
    {
        const CliServersSliceLine one =
            (*line < head.lines) ? slice_line_at(stored, &head, *line) : (CliServersSliceLine){CLI_JSON_NONE, 0u};
        moved = (one.path == CLI_JSON_NONE) ? 0 : ((one.path == 0u) ? 1 : 2);
        *line = one.line;
        if (moved == 2)
        {
            const char *const root = slice_root(stored, &head);
            cli_full(root, root + head.root_size + one.path, from);
        }
    }
    cli_lock_give(&s_slices_lock);
    return moved;
}

// the object `one` written with the lines `from` and `to` in place of those of its range, its other members as they are
static void slice_ranged_written(const CliJson *json, unsigned int one, unsigned int from, unsigned int to,
                                 VerbaLinea *out)
{
    const unsigned int range = cli_json_member(json, one, "range");
    const CliTextPlace start = cli_servers_place(json, cli_json_member(json, range, "start"));
    const CliTextPlace end = cli_servers_place(json, cli_json_member(json, range, "end"));
    mmgr_line_put(out, "{\"range\":{\"start\":{\"line\":");
    mmgr_line_decimal(out, from, 1u);
    mmgr_line_put(out, ",\"character\":");
    mmgr_line_decimal(out, start.col, 1u);
    mmgr_line_put(out, "},\"end\":{\"line\":");
    mmgr_line_decimal(out, to, 1u);
    mmgr_line_put(out, ",\"character\":");
    mmgr_line_decimal(out, end.col, 1u);
    mmgr_line_put(out, "}}");
    for (unsigned int member = json->node[one].first; member != CLI_JSON_NONE; member = json->node[member].next)
    {
        const char *const name = json->text + json->node[member].key;
        if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "range", .cap = CLI_CELLS_POOL}))
        {
            continue;
        }
        mmgr_line_ch(out, ',');
        cli_json_quoted(out, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
        mmgr_line_ch(out, ':');
        cli_json_write(json, member, out, 0u, 0);
    }
    mmgr_line_ch(out, '}');
}

int cli_servers_slice_ranged_in(const char *path, const CliJson *json, unsigned int one, VerbaLinea *out)
{
    const unsigned int range = cli_json_member(json, one, "range");
    const CliTextPlace start = cli_servers_place(json, cli_json_member(json, range, "start"));
    const CliTextPlace end = cli_servers_place(json, cli_json_member(json, range, "end"));
    char key[CLI_PATH_MAX_LEN];
    cli_servers_key_of(path, key);
    cli_lock_take(&s_slices_lock);
    CliServersSliceHead head;
    const char *const stored = slice_stored(key, &head);
    unsigned int from = start.line;
    unsigned int to = end.line;
    const int inside = (stored == NULL) ||
                       (slice_line_in(stored, &head, start.line, &from) && slice_line_in(stored, &head, end.line, &to));
    cli_lock_give(&s_slices_lock);
    if (inside)
    {
        slice_ranged_written(json, one, from, to, out);
    }
    return inside;
}

// the number `length` bytes of `text` write, `otherwise` where they write none
static unsigned long long slice_number(const char *text, unsigned long long length, unsigned long long otherwise)
{
    unsigned long long value = 0ull;
    int any = 0;
    for (unsigned long long at = cli_text_lead(text, length); (at < length) && (text[at] >= '0') && (text[at] <= '9');
         at += 1ull)
    {
        value = (value * 10ull) + (unsigned long long)(text[at] - '0');
        any = 1;
    }
    return any ? value : otherwise;
}

char *cli_servers_slice_diagnostics(const char *path, const char *params, unsigned long long params_size,
                                    const char *all, unsigned long long size, unsigned long long *mapped_size,
                                    CellOwner *owner)
{
    // diagnostics of a version given before the slice stored was are of a slice the map no longer gives; a later
    // version given with no slice stored for it is the same text given again
    unsigned long long version_at = 0ull;
    unsigned long long version_length = 0ull;
    const int versioned = cli_lsp_member(params, params_size, "version", &version_at, &version_length);
    const unsigned long long version = versioned ? slice_number(params + version_at, version_length, 0ull) : 0ull;
    char key[CLI_PATH_MAX_LEN];
    cli_servers_key_of(path, key);
    cli_lock_take(&s_slices_lock);
    CliServersSliceHead head;
    const char *const found = slice_stored(key, &head);
    const char *const stored = ((found != NULL) && (!versioned || (version >= head.version))) ? found : NULL;
    if ((stored != NULL) && versioned)
    {
        (void)cli_keyed_put(&s_published, key, "", (const char *)&version, sizeof(version));
    }
    CliJson json;
    cli_json_begun(&json, NULL, 0u, NULL, 0ull);
    char quiet[256];
    VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
    const unsigned int array =
        (stored != NULL) ? cli_json_taken(&json, all, size, 0u, 0ull, &ignored, &g_cli_servers_cells) : CLI_JSON_NONE;
    VerbaLinea mapped = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
    const int made = cli_json_is(&json, array, CFG_JSON_ARRAY) &&
                     cli_growing_line_made(&mapped, (size_t)size + CLI_SERVERS_SLICE_FIRST_BYTES, owner);
    if (made)
    {
        mmgr_line_ch(&mapped, '[');
        int written = 0;
        for (unsigned int one = json.node[array].first; one != CLI_JSON_NONE; one = json.node[one].next)
        {
            const unsigned int range = cli_json_member(&json, one, "range");
            const CliTextPlace start = cli_servers_place(&json, cli_json_member(&json, range, "start"));
            const CliTextPlace end = cli_servers_place(&json, cli_json_member(&json, range, "end"));
            const CliServersSliceLine from = (start.line < head.lines) ? slice_line_at(stored, &head, start.line)
                                                                       : (CliServersSliceLine){CLI_JSON_NONE, 0u};
            const CliServersSliceLine to = (end.line < head.lines) ? slice_line_at(stored, &head, end.line)
                                                                   : (CliServersSliceLine){CLI_JSON_NONE, 0u};
            // a diagnostic on a line of another file, or on a bracket the slice closes, is of the slice and not the
            // file
            if ((from.path != 0u) || (to.path != 0u) || (to.line < from.line))
            {
                continue;
            }
            mmgr_line_put(&mapped, written ? "," : "");
            slice_ranged_written(&json, one, from.line, to.line, &mapped);
            written = 1;
        }
        mmgr_line_ch(&mapped, ']');
    }
    cli_lock_give(&s_slices_lock);
    cli_json_given(&json);
    if (!made || !mmgr_line_ok(&mapped))
    {
        g_cli_cells->persistent_buf_release(mapped.out, owner);
        return NULL;
    }
    *mapped_size = mapped.at;
    return mapped.out;
}
