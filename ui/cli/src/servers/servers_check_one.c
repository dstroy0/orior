// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_check_one.c: one file of the tree's check: handed to its server as the disk stores it, the server queried for
// its diagnostics or, where it refuses to be queried, waited on for them and for any it gives after them, then let go
// where the check still opens it, its clearing as it closes not passed on. A server given the tree's slices is handed
// the file a window at a time, each window's slice cut as one walk of the file reaches it (servers_slice.c), and its
// diagnostics on the window's own lines gathered; the file's, every window's, are passed on once it is let go. The
// files a server has open for the check and not for the editor are tracked by their URIs; a server is written to about
// a file only with them open, by the check or by the editor. What the check receives is tracked apart, each file by its
// key: the file whose first diagnostics wake its checker, the file whose diagnostics it gathers, and the files it let
// go; its lock is never acquired while a server is written to, as the thread that reads a server takes it
#include "../command_line/cells.h"
#include "../command_line/file_writing.h"
#include "servers_internal.h"

// the most files open for the check and let go by it at once; the most servers that refuse to be queried for a file's
// diagnostics; and the bytes a sliced file's diagnostics are begun in, doubling as they come
#define CLI_SERVERS_CHECK_OPEN_MAX (1u << 16u)
#define CLI_SERVERS_REFUSED 64u
#define CLI_SERVERS_GATHERED_FIRST 4096u

// A checker's wait: whether it waits, the key of the file it waits on, how often it was woken, and its wake; and
// whether it gathers that file's diagnostics a window of its slices at a time, and the last its server gave, moved to
// the file's own lines, a growing line
typedef struct
{
    int waiting;
    char key[CLI_PATH_MAX_LEN];
    unsigned int signals;
    CliWake wake;
    int gathering;
    VerbaLinea gathered;
} CliServersWait;

static CliKeyed s_check_open = {"open checks", {NULL}, 0u, CLI_SERVERS_CHECK_OPEN_MAX};
CliLock g_cli_servers_check_open_lock = CLI_LOCK_INIT;

static CliKeyed s_letting = {"checks let go", {NULL}, 0u, CLI_SERVERS_CHECK_OPEN_MAX};
static CliServersWait s_wait[CLI_SERVERS_CHECKING];
static CliLock s_letting_lock = CLI_LOCK_INIT;

static unsigned long long s_refused[CLI_SERVERS_REFUSED];
static unsigned int s_refusing;
static CliLock s_refused_lock = CLI_LOCK_INIT;

int cli_servers_check_open(const char *uri)
{
    const char *beside = NULL;
    unsigned long long size = 0ull;
    return cli_keyed_get(&s_check_open, uri, &beside, &size) != NULL;
}

void cli_servers_check_open_set(const char *uri, int open)
{
    if (open)
    {
        (void)cli_keyed_put(&s_check_open, uri, "", "", 0ull);
        return;
    }
    cli_keyed_remove(&s_check_open, uri);
}

void cli_servers_check_open_cleared(void)
{
    cli_keyed_clear(&s_check_open);
}

// a file a server has open put on the line that is `context`, ended by a 0
static void check_one_open_put(void *context, const char *path, const char *text, unsigned long long length)
{
    (void)text;
    (void)length;
    mmgr_line_put_n((VerbaLinea *)context, path,
                    cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1ull);
}

void cli_servers_reread_open(CliLsp *server)
{
    // the files the server has open, one after another each ended by a 0, in a cell of their size
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    VerbaLinea line;
    cli_pieces_begun(&pieces, &line, piece, sizeof(piece), &g_cli_servers_cells);
    cli_lsp_open_each(server, check_one_open_put, &line);
    unsigned long long size = 0ull;
    char *const open = cli_pieces_joined(&pieces, &line, "the files a server has open", &size);
    for (unsigned long long at = 0ull; (open != NULL) && (at < size);
         at += cellul.len(&(CatenaFinitaCfg){.src = open + at, .cap = CLI_CELLS_POOL}) + 1u)
    {
        char uri[CLI_PATH_MAX_LEN];
        cli_servers_uri_of(open + at, uri);
        if (!cli_servers_check_open(uri))
        {
            char quiet[256];
            VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
            (void)cli_lsp_read_again(server, open + at, &ignored);
        }
    }
    cli_cell_release(open, &g_cli_servers_cells);
}

void cli_servers_let_go_cleared(const char *key)
{
    cli_lock_take(&s_letting_lock);
    cli_keyed_remove(&s_letting, key);
    cli_lock_give(&s_letting_lock);
}

void cli_servers_woken(const char *key)
{
    cli_lock_take(&s_letting_lock);
    for (unsigned int at = 0u; at < CLI_SERVERS_CHECKING; at += 1u)
    {
        if (s_wait[at].waiting &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_wait[at].key, .other = key, .cap = CLI_CELLS_POOL})))
        {
            s_wait[at].signals += 1u;
            cli_wake_all(&s_wait[at].wake);
        }
    }
    cli_lock_give(&s_letting_lock);
}

int cli_servers_gathered(const char *key, const char *all, unsigned long long size)
{
    cli_lock_take(&s_letting_lock);
    int gathered = 0;
    for (unsigned int at = 0u; at < CLI_SERVERS_CHECKING; at += 1u)
    {
        if (s_wait[at].gathering &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_wait[at].key, .other = key, .cap = CLI_CELLS_POOL})))
        {
            s_wait[at].gathered.at = 0u;
            // the diagnostics of one slice are below 2^32 bytes
            mmgr_line_put_n(&s_wait[at].gathered, all, (size_t)size);
            s_wait[at].signals += 1u;
            cli_wake_all(&s_wait[at].wake);
            gathered = 1;
        }
    }
    cli_lock_give(&s_letting_lock);
    return gathered;
}

void cli_servers_gathering_ended(const char *key)
{
    cli_lock_take(&s_letting_lock);
    for (unsigned int at = 0u; at < CLI_SERVERS_CHECKING; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_wait[at].key, .other = key, .cap = CLI_CELLS_POOL}))
        {
            s_wait[at].gathering = 0;
        }
    }
    cli_lock_give(&s_letting_lock);
}

int cli_servers_letting_go(const char *key)
{
    cli_lock_take(&s_letting_lock);
    const char *beside = NULL;
    unsigned long long size = 0ull;
    const int letting = cli_keyed_get(&s_letting, key, &beside, &size) != NULL;
    cli_keyed_remove(&s_letting, key);
    cli_lock_give(&s_letting_lock);
    return letting;
}

void cli_servers_wait_on(unsigned int checker, const char *key)
{
    cli_lock_take(&s_letting_lock);
    s_wait[checker].waiting = 1;
    s_wait[checker].signals = 0u;
    memor.cpy(&(MemoriaCfg){.dst = s_wait[checker].key,
                            .src = key,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u});
    cli_lock_give(&s_letting_lock);
}

int cli_servers_was_woken(unsigned int checker)
{
    cli_lock_take(&s_letting_lock);
    const int woken = s_wait[checker].signals > 0u;
    s_wait[checker].signals = 0u;
    s_wait[checker].waiting = 0;
    cli_lock_give(&s_letting_lock);
    return woken;
}

void cli_servers_let_go_of(const char *key)
{
    cli_lock_take(&s_letting_lock);
    (void)cli_keyed_put(&s_letting, key, "", "", 0ull);
    cli_lock_give(&s_letting_lock);
}

// whether the checker `checker` was woken within `milliseconds`, its wakes counted off; under the received lock
static int check_one_woken_within(unsigned int checker, unsigned long long milliseconds)
{
    const unsigned long long until = cli_clock_milliseconds() + milliseconds;
    while (s_wait[checker].signals == 0u)
    {
        const unsigned long long now = cli_clock_milliseconds();
        if (now >= until)
        {
            return 0;
        }
        (void)cli_wake_wait_for(&s_wait[checker].wake, &s_letting_lock, until - now);
    }
    s_wait[checker].signals = 0u;
    return 1;
}

// the checker waited on its server to give a file's diagnostics, and for any it gives after them
static void check_one_wait(unsigned int checker)
{
    cli_lock_take(&s_letting_lock);
    if (check_one_woken_within(checker, CLI_SERVERS_PATIENCE))
    {
        while (check_one_woken_within(checker, CLI_SERVERS_QUIET))
        {
        }
    }
    cli_lock_give(&s_letting_lock);
}

// whether the server of process `pid` refused to be queried for a file's diagnostics; and it recorded as one that did
static int check_one_refused(unsigned long long pid, int record)
{
    cli_lock_take(&s_refused_lock);
    int refused = 0;
    for (unsigned int at = 0u; !refused && (at < s_refusing); at += 1u)
    {
        refused = (s_refused[at] == pid);
    }
    if (record && !refused && (s_refusing < CLI_SERVERS_REFUSED))
    {
        s_refused[s_refusing] = pid;
        s_refusing += 1u;
    }
    cli_lock_give(&s_refused_lock);
    return refused;
}

// the file handed to its server as the disk stores it, `size` bytes of `text`: 1, or 0 where the editor has it open or
// it could not be given
static int check_one_given(unsigned int checker, const char *path, const char *uri, const char *key,
                           const CliServersWaiting *one, const char *text, unsigned long long size)
{
    cli_lock_take(&g_cli_servers_check_open_lock);
    const int open = cli_lsp_has(one->server, path);
    if (open && !cli_servers_check_open(uri))
    {
        cli_lock_give(&g_cli_servers_check_open_lock);
        return 0;
    }
    cli_lock_take(&s_letting_lock);
    s_wait[checker].waiting = 1;
    memor.cpy(&(MemoriaCfg){.dst = s_wait[checker].key,
                            .src = key,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u});
    s_wait[checker].signals = 0u;
    cli_keyed_remove(&s_letting, key);
    cli_lock_give(&s_letting_lock);
    cli_servers_check_open_set(uri, 1);
    char quiet[1024];
    VerbaLinea err_msg = {quiet, sizeof(quiet), 0ull};
    const int sent = open ? cli_lsp_changed(one->server, path, text, size, &err_msg)
                          : cli_lsp_opened(one->server, path, one->id, text, size, &err_msg);
    if (!sent)
    {
        cli_servers_check_open_set(uri, 0);
        cli_lock_take(&s_letting_lock);
        s_wait[checker].waiting = 0;
        cli_lock_give(&s_letting_lock);
    }
    cli_lock_give(&g_cli_servers_check_open_lock);
    return sent;
}

// the server queried for the file at `path`'s diagnostics, those it gives whole retained and passed on, or for a file
// it is given in slices moved to the file's own lines and gathered with the window's, or waited on to give them where
// it refuses to be queried
static void check_one_request(unsigned int checker, const char *path, const char *uri, const char *key,
                              const CliServersWaiting *one, const CliServersSink *sink)
{
    const unsigned long long pid = cli_lsp_pid(one->server);
    if (check_one_refused(pid, 0))
    {
        check_one_wait(checker);
        return;
    }
    char params_piece[CLI_PATH_MAX_LEN];
    CliPieces params_pieces;
    VerbaLinea params;
    cli_pieces_begun(&params_pieces, &params, params_piece, sizeof(params_piece), &g_cli_servers_cells);
    mmgr_line_put(&params, "{\"textDocument\":{\"uri\":");
    cli_json_quoted(&params, uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&params, "}}");
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    VerbaLinea result;
    cli_pieces_begun(&pieces, &result, piece, sizeof(piece), &g_cli_servers_cells);
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (!cli_servers_request(one->server, "textDocument/diagnostic", &params_pieces, &params, CLI_SERVERS_PATIENCE,
                             &result, &err_msg))
    {
        cli_pieces_given(&pieces);
        unsigned long long at = 0ull;
        if (!cli_text_find(why, err_msg.at, "had no answer", 13ull, &at))
        {
            (void)check_one_refused(pid, 1);
            check_one_wait(checker);
        }
        return;
    }
    unsigned long long size = 0ull;
    char *const answer = cli_pieces_joined(&pieces, &result, "a server's answer to a check", &size);
    unsigned long long kind_at = 0ull;
    unsigned long long kind_length = 0ull;
    unsigned long long items_at = 0ull;
    unsigned long long items_length = 0ull;
    char kind[16];
    if ((answer == NULL) || !cli_lsp_member(answer, size, "kind", &kind_at, &kind_length) ||
        !cli_lsp_string(answer + kind_at, kind_length, kind, sizeof(kind)) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "full", .cap = CLI_CELLS_POOL})))
    {
        cli_cell_release(answer, &g_cli_servers_cells);
        return;
    }
    const int has_items = cli_lsp_member(answer, size, "items", &items_at, &items_length) &&
                          (answer[items_at + cli_text_lead(answer + items_at, items_length)] == '[');
    const char *const items = has_items ? answer + items_at : "[]";
    const unsigned long long items_size = has_items ? items_length : 2ull;
    if (cli_servers_sliced(path))
    {
        unsigned long long mapped_size = 0ull;
        char *const mapped =
            cli_servers_slice_diagnostics(path, "{}", 2ull, items, items_size, &mapped_size, &g_cli_servers_cells);
        if (mapped != NULL)
        {
            (void)cli_servers_gathered(key, mapped, mapped_size);
        }
        cli_cell_release(mapped, &g_cli_servers_cells);
        cli_cell_release(answer, &g_cli_servers_cells);
        return;
    }
    cli_lock_take(&g_cli_servers_published_lock);
    (void)cli_keyed_put(&g_cli_servers_published, key, uri, items, items_size);
    cli_lock_give(&g_cli_servers_published_lock);
    cli_cell_release(answer, &g_cli_servers_cells);
    cli_servers_emit_whole(key, uri, sink);
}

// A file the check gives its server in slices: its checker, path, URI and key, its server and language, where what
// the check reports goes, whether a window of it was given and whether one could not be, and its diagnostics on every
// window's own lines, a growing line, and whether one is written
typedef struct
{
    unsigned int checker;
    const char *path;
    const char *uri;
    const char *key;
    const CliServersWaiting *one;
    const CliServersSink *sink;
    int given;
    int stopped;
    VerbaLinea all;
    int written;
} CliServersCheckSliced;

// the diagnostics `gathered`, `size` bytes, those on the lines `first` to `last` put after those the file has
static void check_one_window_kept(CliServersCheckSliced *sliced, const char *gathered, unsigned long long size,
                                  unsigned int first, unsigned int last)
{
    CliJson json;
    cli_json_begun(&json, NULL, 0u, NULL, 0ull);
    char quiet[256];
    VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
    const unsigned int array = cli_json_taken(&json, gathered, size, 0u, 0ull, &ignored, &g_cli_servers_cells);
    for (unsigned int one = cli_json_is(&json, array, CFG_JSON_ARRAY) ? json.node[array].first : CLI_JSON_NONE;
         one != CLI_JSON_NONE; one = json.node[one].next)
    {
        const unsigned int range = cli_json_member(&json, one, "range");
        const CliTextPlace start = cli_servers_place(&json, cli_json_member(&json, range, "start"));
        if ((start.line < first) || (start.line > last))
        {
            continue;
        }
        mmgr_line_put(&sliced->all, sliced->written ? "," : "");
        cli_json_write(&json, one, &sliced->all, 0u, 0);
        sliced->written = 1;
    }
    cli_json_given(&json);
}

// a window of the file given its server as its slice `cut`, the server queried or waited on for its diagnostics, and
// those on the window's own lines kept: 1 to cut the next, 0 where the editor took the file or it could not be given
static int check_one_window(void *context, const CliServersCut *cut, unsigned int first, unsigned int last)
{
    CliServersCheckSliced *const sliced = (CliServersCheckSliced *)context;
    const unsigned int checker = sliced->checker;
    cli_lock_take(&g_cli_servers_check_open_lock);
    const int open = cli_lsp_has(sliced->one->server, sliced->path);
    if (open && !cli_servers_check_open(sliced->uri))
    {
        cli_lock_give(&g_cli_servers_check_open_lock);
        sliced->stopped = 1;
        return 0;
    }
    cli_lock_take(&s_letting_lock);
    s_wait[checker].waiting = 1;
    memor.cpy(&(MemoriaCfg){.dst = s_wait[checker].key,
                            .src = sliced->key,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = sliced->key, .cap = CLI_CELLS_POOL}) + 1u});
    s_wait[checker].signals = 0u;
    s_wait[checker].gathering = 1;
    s_wait[checker].gathered.at = 0u;
    cli_keyed_remove(&s_letting, sliced->key);
    cli_lock_give(&s_letting_lock);
    cli_servers_check_open_set(sliced->uri, 1);
    char quiet[1024];
    VerbaLinea err_msg = {quiet, sizeof(quiet), 0ull};
    const int sent = cli_servers_cut_sent(sliced->one->server, open ? NULL : sliced->one->id, cut, &err_msg);
    if (!sent && !open)
    {
        cli_servers_check_open_set(sliced->uri, 0);
    }
    cli_lock_give(&g_cli_servers_check_open_lock);
    if (!sent)
    {
        sliced->stopped = 1;
        return 0;
    }
    sliced->given = 1;
    check_one_request(checker, sliced->path, sliced->uri, sliced->key, sliced->one, sliced->sink);
    // the window's diagnostics taken from the wait, as the thread that reads the server may write them again
    cli_lock_take(&s_letting_lock);
    const unsigned long long size = mmgr_line_ok(&s_wait[checker].gathered) ? s_wait[checker].gathered.at : 0ull;
    char *const gathered = (size > 0ull) ? cli_lane_alloc(size, &g_cli_servers_cells) : NULL;
    if (gathered != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = gathered, .src = s_wait[checker].gathered.out, .bytes = (size_t)size});
    }
    cli_lock_give(&s_letting_lock);
    if (gathered != NULL)
    {
        check_one_window_kept(sliced, gathered, size, first, last);
    }
    cli_cell_release(gathered, &g_cli_servers_cells);
    return 1;
}

// one file checked by the checker `checker` a window of its slices at a time, its `size` bytes of `text` as the disk
// stores them, then let go where the check still opens it and its diagnostics, every window's, retained and passed on
static void check_one_sliced(unsigned int checker, const char *path, const char *uri, const char *key, const char *text,
                             unsigned long long size, const CliServersWaiting *one, const CliServersSink *sink)
{
    CliServersCheckSliced sliced = {checker, path, uri, key, one, sink, 0, 0, {NULL, 0ull, 0ull, NULL, NULL, 0ull}, 0};
    cli_lock_take(&s_letting_lock);
    s_wait[checker].gathering = 0;
    s_wait[checker].gathered = (VerbaLinea){NULL, 0ull, 0ull, NULL, NULL, 0ull};
    const int gathers =
        cli_growing_line_made(&s_wait[checker].gathered, CLI_SERVERS_GATHERED_FIRST, &g_cli_servers_cells);
    cli_lock_give(&s_letting_lock);
    char quiet[1024];
    VerbaLinea err_msg = {quiet, sizeof(quiet), 0ull};
    const int made = gathers && cli_growing_line_made(&sliced.all, CLI_SERVERS_GATHERED_FIRST, &g_cli_servers_cells);
    mmgr_line_ch(&sliced.all, '[');
    // the server waited on while the file is sliced, from its first window to its letting go
    cli_servers_ask_begun(one->server);
    const int cut = made && cli_servers_slices_cut(sink->tree, path, text, size, check_one_window, &sliced, &err_msg);
    // the file let go before its gathering ends: what its server gives as it closes is the clearing let go
    cli_lock_take(&g_cli_servers_check_open_lock);
    const int ours = cli_servers_check_open(uri);
    const int editors = !ours && cli_lsp_has(one->server, path);
    cli_servers_check_open_set(uri, 0);
    cli_lock_take(&s_letting_lock);
    s_wait[checker].waiting = 0;
    if (ours)
    {
        (void)cli_keyed_put(&s_letting, key, "", "", 0ull);
    }
    cli_lock_give(&s_letting_lock);
    if (ours)
    {
        (void)cli_lsp_closed(one->server, path, &err_msg);
        cli_servers_slice_closed(path);
    }
    cli_lock_give(&g_cli_servers_check_open_lock);
    cli_lock_take(&s_letting_lock);
    s_wait[checker].gathering = 0;
    cli_lock_give(&s_letting_lock);
    cli_servers_ask_ended(one->server);
    mmgr_line_ch(&sliced.all, ']');
    if (cut && !sliced.stopped && !editors && mmgr_line_ok(&sliced.all))
    {
        cli_lock_take(&g_cli_servers_published_lock);
        (void)cli_keyed_put(&g_cli_servers_published, key, uri, sliced.all.out, sliced.all.at);
        cli_lock_give(&g_cli_servers_published_lock);
        cli_servers_emit_whole(key, uri, sink);
    }
    cli_cell_release(sliced.all.out, &g_cli_servers_cells);
    cli_lock_take(&s_letting_lock);
    cli_cell_release(s_wait[checker].gathered.out, &g_cli_servers_cells);
    s_wait[checker].gathered = (VerbaLinea){NULL, 0ull, 0ull, NULL, NULL, 0ull};
    cli_lock_give(&s_letting_lock);
}

// one file checked by the checker `checker`, its `size` bytes of `text` as the disk stores them; NULL where it is not
// read whole, and the check let go
static void check_one_run(unsigned int checker, const char *path, const char *text, unsigned long long size,
                          const CliServersWaiting *one, const CliServersSink *sink)
{
    char uri[CLI_PATH_MAX_LEN];
    char key[CLI_PATH_MAX_LEN];
    cli_servers_uri_of(path, uri);
    cli_lsp_key_of(uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), key);
    char quiet[1024];
    VerbaLinea err_msg = {quiet, sizeof(quiet), 0ull};
    if ((text == NULL) || (size > CLI_SERVERS_LARGEST) || !cli_utf8_whole(text, size))
    {
        cli_lock_take(&g_cli_servers_check_open_lock);
        if (cli_servers_check_open(uri))
        {
            cli_servers_check_open_set(uri, 0);
            (void)cli_lsp_closed(one->server, path, &err_msg);
        }
        cli_lock_give(&g_cli_servers_check_open_lock);
        return;
    }
    if (cli_servers_slicing(one->server))
    {
        check_one_sliced(checker, path, uri, key, text, size, one, sink);
        return;
    }
    if (!check_one_given(checker, path, uri, key, one, text, size))
    {
        return;
    }
    check_one_request(checker, path, uri, key, one, sink);
    cli_lock_take(&g_cli_servers_check_open_lock);
    const int ours = cli_servers_check_open(uri);
    cli_servers_check_open_set(uri, 0);
    cli_lock_take(&s_letting_lock);
    s_wait[checker].waiting = 0;
    if (ours)
    {
        (void)cli_keyed_put(&s_letting, key, "", "", 0ull);
    }
    cli_lock_give(&s_letting_lock);
    if (ours)
    {
        (void)cli_lsp_closed(one->server, path, &err_msg);
    }
    cli_lock_give(&g_cli_servers_check_open_lock);
}

void cli_servers_check_one(unsigned int checker, const char *path, const CliServersWaiting *one,
                           const CliServersSink *sink)
{
    // the file read where it lies for as long as it is checked
    unsigned long long size = 0ull;
    const unsigned char *const mapped = cli_file_read(path, &size);
    check_one_run(checker, path, (const char *)mapped, size, one, sink);
    if (mapped != NULL)
    {
        cli_file_let_go(mapped, size);
    }
}
