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
#include "servers_internal.h"

// A file the check gives its server in slices: its checker, path, URI and key, its server and language, where what the
// check reports goes, whether a window of it was given and whether one could not be, and its diagnostics on every
// window's own lines, a line, and whether one is written
typedef struct
{
    unsigned int checker;
    const char *path;
    const char *uri;
    const char *key;
    const CliServersWaiting *one;
    const CliServersSink *sink;
    int supplied;
    int stopped;
    VerbaLinea all;
    int written;
} CliServersCheckSliced;
// the most files open for the check and let go by it at once; the most servers that refuse to be queried for a file's
// diagnostics; and the bytes a sliced file's diagnostics are written in
#define CLI_SERVERS_CHECK_OPEN_MAX (1u << 16u)
#define CLI_SERVERS_REFUSED 64u
#define CLI_SERVERS_GATHERED_CAPACITY (4ull << 20u)

// A checker's wait: whether it waits, the key of the file it waits on, how often it was woken, and its wake; and
// whether it gathers that file's diagnostics a window of its slices at a time, and the last its server gave, moved to
// the file's own lines, a line
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

static CliKeyed s_letting = {"checks let go", {NULL}, 0u, CLI_SERVERS_CHECK_OPEN_MAX};
static CliServersWait s_wait[CLI_SERVERS_CHECKING];

static unsigned long long s_refused[CLI_SERVERS_REFUSED];
static unsigned int s_refusing;

int cli_servers_check_open(const CliServersUriCtx *args)
{
    const char *adjacent = NULL;
    unsigned long long size = 0ull;
    return EMBED_CALL(cli_keyed_key_adjacent.get, CliKeyedKeyAdjacentCfg, .keyed = &s_check_open, .key = args->uri,
                      .adjacent = &adjacent, .size = &size) != NULL;
}

void cli_servers_check_open_set(const CliServersUriCtx *args)
{
    if (args->open)
    {
        (void)EMBED_CALL(cli_keyed_key_adjacent.put, CliKeyedKeyAdjacentCfg, .keyed_out = &s_check_open,
                         .key = args->uri, .adjacent_value = "", .text = "", .size_value = 0ull);
        return;
    }
    EMBED_CALL(cli_keyed_key_adjacent.remove, CliKeyedKeyAdjacentCfg, .keyed_out = &s_check_open, .key = args->uri);
}

void cli_servers_check_open_clear(void)
{
    EMBED_CALL(cli_keyed_name.clear, CliKeyedNameCfg, .keyed_out = &s_check_open);
}

void cli_servers_reread_open(const CliServersServerCommandCtx *args)
{
    unsigned long long size = 0ull;
    char *const open = EMBED_CALL(cli_lsp_size.open_uris_alloc, CliLspSizeCfg, .lsp = args->server, .size = &size,
                                  .owner = &g_cli_servers_cells);
    for (unsigned long long at = 0ull; (open != NULL) && (at < size);
         at += cellul.len(&(CatenaFinitaCfg){.src = open + at, .cap = CLI_CELLS_POOL}) + 1u)
    {
        const char *const uri = open + at;
        char path[CLI_PATH_MAX_LEN];
        if (!EMBED_CALL(cli_servers_uri_internal.check_open, CliServersUriCtx, .uri = uri) &&
            EMBED_CALL(cli_lsp_length_uri.path_get, CliLspLengthUriCfg, .uri = uri,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), .path_out = path))
        {
            char quiet[256];
            VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
            (void)EMBED_CALL(cli_lsp_err_msg_text.read_again, CliLspErrMsgTextCfg, .lsp = args->server, .path = path,
                             .err_msg = &ignored);
        }
    }
    g_cli_cells->persistent_buf_release(open, &g_cli_servers_cells);
}

void cli_servers_let_go_clear(const CliServersKeyCheckerCtx *args)
{
    EMBED_CALL(cli_keyed_key_adjacent.remove, CliKeyedKeyAdjacentCfg, .keyed_out = &s_letting, .key = args->key);
}

void cli_servers_wake(const CliServersKeyCheckerCtx *args)
{
    for (unsigned int at = 0u; at < CLI_SERVERS_CHECKING; at += 1u)
    {
        if (s_wait[at].waiting &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_wait[at].key, .other = args->key, .cap = CLI_CELLS_POOL})))
        {
            s_wait[at].signals += 1u;
            EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_wait[at].wake);
        }
    }
}

int cli_servers_gather(const CliServersSizeKeyCtx *args)
{
    int gathered = 0;
    for (unsigned int at = 0u; at < CLI_SERVERS_CHECKING; at += 1u)
    {
        if (s_wait[at].gathering &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_wait[at].key, .other = args->key, .cap = CLI_CELLS_POOL})))
        {
            s_wait[at].gathered.at = 0u;
            // the diagnostics of one slice are below 2^32 bytes
            mmgr_line_put_n(&s_wait[at].gathered, args->all, (size_t)args->size);
            s_wait[at].signals += 1u;
            EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_wait[at].wake);
            gathered = 1;
        }
    }
    return gathered;
}

void cli_servers_gathering_end(const CliServersKeyCheckerCtx *args)
{
    for (unsigned int at = 0u; at < CLI_SERVERS_CHECKING; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_wait[at].key, .other = args->key, .cap = CLI_CELLS_POOL}))
        {
            s_wait[at].gathering = 0;
        }
    }
}

int cli_servers_letting_go(const CliServersKeyCheckerCtx *args)
{
    const char *adjacent = NULL;
    unsigned long long size = 0ull;
    const int letting = EMBED_CALL(cli_keyed_key_adjacent.get, CliKeyedKeyAdjacentCfg, .keyed = &s_letting,
                                   .key = args->key, .adjacent = &adjacent, .size = &size) != NULL;
    EMBED_CALL(cli_keyed_key_adjacent.remove, CliKeyedKeyAdjacentCfg, .keyed_out = &s_letting, .key = args->key);
    return letting;
}

void cli_servers_wait_on(const CliServersKeyCheckerCtx *args)
{
    s_wait[args->checker].waiting = 1;
    s_wait[args->checker].signals = 0u;
    memor.cpy(&(MemoriaCfg){.dst = s_wait[args->checker].key,
                            .src = args->key,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_CELLS_POOL}) + 1u});
}

int cli_servers_was_wake(const CliServersKeyCheckerCtx *args)
{
    const int woken = s_wait[args->checker].signals > 0u;
    s_wait[args->checker].signals = 0u;
    s_wait[args->checker].waiting = 0;
    return woken;
}

void cli_servers_let_go_get(const CliServersKeyCheckerCtx *args)
{
    (void)EMBED_CALL(cli_keyed_key_adjacent.put, CliKeyedKeyAdjacentCfg, .keyed_out = &s_letting, .key = args->key,
                     .adjacent_value = "", .text = "", .size_value = 0ull);
}

typedef struct
{
    unsigned int checker;
    unsigned long long milliseconds;
    unsigned long long pid;
    int record;
    const char *path;
    const char *uri;
    const char *key;
    const CliServersWaiting *one;
    const char *text;
    unsigned long long size;
    const CliServersSink *sink;
    CliServersCheckSliced *sliced;
    const char *gathered;
    unsigned int first;
    unsigned int last;
} ServersCheckOneCtx;
// whether the checker `checker` was woken within `milliseconds`, its wakes counted off; under the received lock
EMBED_INLINE int check_one_woken_within(const ServersCheckOneCtx *args)
{
    unsigned long long milliseconds = args->milliseconds;
    const unsigned long long until = cli_clock_milliseconds() + milliseconds;
    while (s_wait[args->checker].signals == 0u)
    {
        const unsigned long long now = cli_clock_milliseconds();
        if (now >= until)
        {
            return 0;
        }
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &s_wait[args->checker].wake,
                         .milliseconds_u64 = until - now);
    }
    s_wait[args->checker].signals = 0u;
    return 1;
}

// the checker waited on its server to give a file's diagnostics, and for any it gives after them
EMBED_INLINE void check_one_wait(const ServersCheckOneCtx *args)
{
    if (EMBED_CALL(check_one_woken_within, ServersCheckOneCtx, .checker = args->checker,
                   .milliseconds = CLI_SERVERS_PATIENCE))
    {
        while (EMBED_CALL(check_one_woken_within, ServersCheckOneCtx, .checker = args->checker,
                          .milliseconds = CLI_SERVERS_QUIET))
        {
        }
    }
}

// whether the server of process `pid` refused to be queried for a file's diagnostics; and it recorded as one that did
EMBED_INLINE int check_one_reject(const ServersCheckOneCtx *args)
{
    int refused = 0;
    for (unsigned int at = 0u; !refused && (at < s_refusing); at += 1u)
    {
        refused = (s_refused[at] == args->pid);
    }
    if (args->record && !refused && (s_refusing < CLI_SERVERS_REFUSED))
    {
        s_refused[s_refusing] = args->pid;
        s_refusing += 1u;
    }
    return refused;
}

// the file handed to its server as the disk stores it, `size` bytes of `text`: 1, or 0 where the editor has it open or
// it could not be given
EMBED_INLINE int check_one_free(const ServersCheckOneCtx *args)
{
    const char *path = args->path;
    const char *uri = args->uri;
    const char *key = args->key;
    const char *text = args->text;
    const int open = EMBED_CALL(cli_lsp_path_out.has, CliLspPathOutCfg, .lsp = args->one->server, .path = path);
    if (open && !EMBED_CALL(cli_servers_uri_internal.check_open, CliServersUriCtx, .uri = uri))
    {
        return 0;
    }
    s_wait[args->checker].waiting = 1;
    memor.cpy(&(MemoriaCfg){.dst = s_wait[args->checker].key,
                            .src = key,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u});
    s_wait[args->checker].signals = 0u;
    EMBED_CALL(cli_keyed_key_adjacent.remove, CliKeyedKeyAdjacentCfg, .keyed_out = &s_letting, .key = key);
    EMBED_CALL(cli_servers_uri_internal.check_open_set, CliServersUriCtx, .uri = uri, .open = 1);
    char quiet[1024];
    VerbaLinea err_msg = {quiet, sizeof(quiet), 0ull};
    const int sent =
        open ? EMBED_CALL(cli_lsp_err_msg_text.change, CliLspErrMsgTextCfg, .lsp = args->one->server, .path = path,
                          .text = text, .length = args->size, .err_msg = &err_msg)
             : EMBED_CALL(cli_lsp_err_msg_text.open, CliLspErrMsgTextCfg, .lsp = args->one->server, .path = path,
                          .language_id = args->one->id, .text = text, .length = args->size, .err_msg = &err_msg);
    if (!sent)
    {
        EMBED_CALL(cli_servers_uri_internal.check_open_set, CliServersUriCtx, .uri = uri, .open = 0);
        s_wait[args->checker].waiting = 0;
    }
    return sent;
}

// the server queried for the file at `path`'s diagnostics, those it gives whole retained and passed on, or for a file
// it is given in slices moved to the file's own lines and gathered with the window's, or waited on to give them where
// it refuses to be queried
static void check_one_request(const ServersCheckOneCtx *args)
{
    const char *path = args->path;
    const char *uri = args->uri;
    const char *key = args->key;
    const CliServersSink *sink = args->sink;
    const unsigned long long pid = EMBED_CALL(cli_lsp_size_kind.pid, CliLspSizeKindCfg, .lsp_read = args->one->server);
    if (EMBED_CALL(check_one_reject, ServersCheckOneCtx, .pid = pid, .record = 0))
    {
        check_one_wait(args);
        return;
    }
    char params_text[CLI_SERVERS_PARAMS];
    VerbaLinea params = {params_text, sizeof(params_text), 0ull};
    mmgr_line_put(&params, "{\"textDocument\":{\"uri\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &params, .text = uri,
               .length = cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&params, "}}");
    CliLspAnswer reply;
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (!EMBED_CALL(cli_servers_err_msg_internal.request, CliServersErrMsgCtx, .server = args->one->server,
                    .method = "textDocument/diagnostic", .params = &params, .wait = CLI_SERVERS_PATIENCE,
                    .answer = &reply, .err_msg = &err_msg))
    {
        unsigned long long at = 0ull;
        if (!EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = why, .length_value = err_msg.at,
                        .required = "had no answer", .size = 13ull, .at_pointer = &at))
        {
            (void)EMBED_CALL(check_one_reject, ServersCheckOneCtx, .pid = pid, .record = 1);
            check_one_wait(args);
        }
        return;
    }
    const char *const answer = reply.text;
    const unsigned long long size = reply.size;
    unsigned long long kind_at = 0ull;
    unsigned long long kind_length = 0ull;
    unsigned long long items_at = 0ull;
    unsigned long long items_length = 0ull;
    char kind[16];
    if (!EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = answer, .size_value = size, .name = "kind",
                    .start = &kind_at, .length_pointer = &kind_length) ||
        !EMBED_CALL(cli_lsp_capacity.string, CliLspCapacityCfg, .text = answer + kind_at, .length = kind_length,
                    .out_capacity = kind, .capacity = sizeof(kind)) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "full", .cap = CLI_CELLS_POOL})))
    {
        g_cli_cells->persistent_buf_release(reply.message, &g_cli_servers_cells);
        return;
    }
    const int has_items = EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = answer, .size_value = size,
                                     .name = "items", .start = &items_at, .length_pointer = &items_length) &&
                          (answer[items_at + EMBED_CALL(cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg,
                                                        .text = answer + items_at, .size = items_length)] == '[');
    const char *const items = has_items ? answer + items_at : "[]";
    const unsigned long long items_size = has_items ? items_length : 2ull;
    if (EMBED_CALL(cli_servers_path_lsp_internal.slice, CliServersPathLspCtx, .path = path))
    {
        unsigned long long mapped_size = 0ull;
        char *const mapped = EMBED_CALL(cli_servers_path_size_internal.slice_diagnostics, CliServersPathSizeCtx,
                                        .path = path, .params_text = "{}", .params_size = 2ull, .all = items,
                                        .size = items_size, .mapped_size = &mapped_size, .owner = &g_cli_servers_cells);
        if (mapped != NULL)
        {
            (void)EMBED_CALL(cli_servers_size_key_internal.gather, CliServersSizeKeyCtx, .key = key, .all = mapped,
                             .size = mapped_size);
        }
        g_cli_cells->persistent_buf_release(mapped, &g_cli_servers_cells);
        g_cli_cells->persistent_buf_release(reply.message, &g_cli_servers_cells);
        return;
    }
    (void)EMBED_CALL(cli_keyed_key_adjacent.put, CliKeyedKeyAdjacentCfg, .keyed_out = &g_cli_servers_published,
                     .key = key, .adjacent_value = uri, .text = items, .size_value = items_size);
    g_cli_cells->persistent_buf_release(reply.message, &g_cli_servers_cells);
    EMBED_CALL(cli_servers_sink_key_internal.emit_file, CliServersSinkKeyCtx, .key = key, .uri = uri, .sink = sink);
}

// the diagnostics `gathered`, `size` bytes, those on the lines `first` to `last` put after those the file has
EMBED_INLINE void check_one_window_save(const ServersCheckOneCtx *args)
{
    unsigned long long size = args->size;
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ull);
    char quiet[256];
    VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
    const unsigned int array = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg,
                                          .json = &json, .text = args->gathered, .size = size, .nodes_more = 0u,
                                          .text_more = 0ull, .err_msg = &ignored, .owner = &g_cli_servers_cells);
    for (unsigned int one = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json,
                                       .node = array, .kind = CFG_JSON_ARRAY)
                                ? json.node[array].first
                                : CLI_JSON_NONE;
         one != CLI_JSON_NONE; one = json.node[one].next)
    {
        const unsigned int range = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = &json, .object = one, .name = "range");
        const CliTextPosition start =
            EMBED_CALL(cli_servers_json_node_otherwise_internal.place, CliServersJsonNodeOtherwiseCtx, .json = &json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = range, .name = "start"));
        if ((start.line < args->first) || (start.line > args->last))
        {
            continue;
        }
        mmgr_line_put(&args->sliced->all, args->sliced->written ? "," : "");
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &json, .node = one,
                   .out = &args->sliced->all, .indent = 0u, .sorted = 0);
        args->sliced->written = 1;
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
}

// a window of the file given its server as its slice `cut`, the server queried or waited on for its diagnostics, and
// those on the window's own lines saved: 1 to cut the next, 0 where the editor took the file or it could not be given
static int check_one_window(const CliServersWindowCfg *args)
{
    const CliServersCut *cut = args->cut;
    unsigned int first = args->first;
    unsigned int last = args->last;
    CliServersCheckSliced *const sliced = (CliServersCheckSliced *)args->context;
    const unsigned int checker = sliced->checker;
    const int open =
        EMBED_CALL(cli_lsp_path_out.has, CliLspPathOutCfg, .lsp = sliced->one->server, .path = sliced->path);
    if (open && !EMBED_CALL(cli_servers_uri_internal.check_open, CliServersUriCtx, .uri = sliced->uri))
    {
        sliced->stopped = 1;
        return 0;
    }
    s_wait[checker].waiting = 1;
    memor.cpy(&(MemoriaCfg){.dst = s_wait[checker].key,
                            .src = sliced->key,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = sliced->key, .cap = CLI_CELLS_POOL}) + 1u});
    s_wait[checker].signals = 0u;
    s_wait[checker].gathering = 1;
    s_wait[checker].gathered.at = 0u;
    EMBED_CALL(cli_keyed_key_adjacent.remove, CliKeyedKeyAdjacentCfg, .keyed_out = &s_letting, .key = sliced->key);
    EMBED_CALL(cli_servers_uri_internal.check_open_set, CliServersUriCtx, .uri = sliced->uri, .open = 1);
    char quiet[1024];
    VerbaLinea err_msg = {quiet, sizeof(quiet), 0ull};
    const int sent =
        EMBED_CALL(cli_servers_err_msg_server_internal.cut_send, CliServersErrMsgServerCtx,
                   .server = sliced->one->server, .id = open ? NULL : sliced->one->id, .cut = cut, .err_msg = &err_msg);
    if (!sent && !open)
    {
        EMBED_CALL(cli_servers_uri_internal.check_open_set, CliServersUriCtx, .uri = sliced->uri, .open = 0);
    }
    if (!sent)
    {
        sliced->stopped = 1;
        return 0;
    }
    sliced->supplied = 1;
    EMBED_CALL(check_one_request, ServersCheckOneCtx, .checker = checker, .path = sliced->path, .uri = sliced->uri,
               .key = sliced->key, .one = sliced->one, .sink = sliced->sink);
    // the window's diagnostics taken from the wait, as the thread that reads the server may write them again
    const unsigned long long size = mmgr_line_ok(&s_wait[checker].gathered) ? s_wait[checker].gathered.at : 0ull;
    char *const gathered = (size > 0ull) ? g_cli_cells->temporary_buf_alloc(size, &g_cli_servers_cells) : NULL;
    if (gathered != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = gathered, .src = s_wait[checker].gathered.out, .bytes = (size_t)size});
    }
    if (gathered != NULL)
    {
        EMBED_CALL(check_one_window_save, ServersCheckOneCtx, .sliced = sliced, .gathered = gathered, .size = size,
                   .first = first, .last = last);
    }
    g_cli_cells->persistent_buf_release(gathered, &g_cli_servers_cells);
    return 1;
}

// one file checked by the checker `checker` a window of its slices at a time, its `size` bytes of `text` as the disk
// stores them, then let go where the check still opens it and its diagnostics, every window's, retained and passed on
EMBED_INLINE void check_one_slice(const ServersCheckOneCtx *args)
{
    const char *path = args->path;
    const char *uri = args->uri;
    const char *key = args->key;
    const char *text = args->text;
    unsigned long long size = args->size;
    const CliServersSink *sink = args->sink;
    CliServersCheckSliced sliced = {
        args->checker, path, uri, key, args->one, sink, 0, 0, {NULL, 0ull, 0ull, NULL, NULL, 0ull}, 0};
    s_wait[args->checker].gathering = 0;
    char *const gathered_cell = g_cli_cells->temporary_buf_alloc(CLI_SERVERS_GATHERED_CAPACITY, &g_cli_servers_cells);
    s_wait[args->checker].gathered = mmgr_line_over(gathered_cell, CLI_SERVERS_GATHERED_CAPACITY);
    char quiet[1024];
    VerbaLinea err_msg = {quiet, sizeof(quiet), 0ull};
    char *const all_cell = g_cli_cells->temporary_buf_alloc(CLI_SERVERS_GATHERED_CAPACITY, &g_cli_servers_cells);
    sliced.all = mmgr_line_over(all_cell, CLI_SERVERS_GATHERED_CAPACITY);
    const int made = (gathered_cell != NULL) && (all_cell != NULL);
    mmgr_line_ch(&sliced.all, '[');
    // the server waited on while the file is sliced, from its first window to its letting go
    EMBED_CALL(cli_servers_internal.request_begin, CliServersCtx, .server_read = args->one->server);
    const int cut =
        made && EMBED_CALL(cli_servers_path_size_root_err_msg_internal.slices_cut, CliServersPathSizeRootErrMsgCtx,
                           .root = sink->tree, .path = path, .text = text, .size = size, .window = check_one_window,
                           .context = &sliced, .err_msg = &err_msg);
    // the file let go before its gathering ends: what its server gives as it closes is the clearing let go
    const int ours = EMBED_CALL(cli_servers_uri_internal.check_open, CliServersUriCtx, .uri = uri);
    const int editors =
        !ours && EMBED_CALL(cli_lsp_path_out.has, CliLspPathOutCfg, .lsp = args->one->server, .path = path);
    EMBED_CALL(cli_servers_uri_internal.check_open_set, CliServersUriCtx, .uri = uri, .open = 0);
    s_wait[args->checker].waiting = 0;
    if (ours)
    {
        (void)EMBED_CALL(cli_keyed_key_adjacent.put, CliKeyedKeyAdjacentCfg, .keyed_out = &s_letting, .key = key,
                         .adjacent_value = "", .text = "", .size_value = 0ull);
    }
    if (ours)
    {
        (void)EMBED_CALL(cli_lsp_err_msg_text.close, CliLspErrMsgTextCfg, .lsp = args->one->server, .path = path,
                         .err_msg = &err_msg);
        EMBED_CALL(cli_servers_path_lsp_internal.slice_close, CliServersPathLspCtx, .path = path);
    }
    s_wait[args->checker].gathering = 0;
    EMBED_CALL(cli_servers_internal.request_end, CliServersCtx, .server_read = args->one->server);
    mmgr_line_ch(&sliced.all, ']');
    if (cut && !sliced.stopped && !editors && mmgr_line_ok(&sliced.all))
    {
        (void)EMBED_CALL(cli_keyed_key_adjacent.put, CliKeyedKeyAdjacentCfg, .keyed_out = &g_cli_servers_published,
                         .key = key, .adjacent_value = uri, .text = sliced.all.out, .size_value = sliced.all.at);
        EMBED_CALL(cli_servers_sink_key_internal.emit_file, CliServersSinkKeyCtx, .key = key, .uri = uri, .sink = sink);
    }
    g_cli_cells->persistent_buf_release(sliced.all.out, &g_cli_servers_cells);
    g_cli_cells->persistent_buf_release(s_wait[args->checker].gathered.out, &g_cli_servers_cells);
    s_wait[args->checker].gathered = (VerbaLinea){NULL, 0ull, 0ull, NULL, NULL, 0ull};
}

// one file checked by the checker `checker`, its `size` bytes of `text` as the disk stores them; NULL where it is not
// read whole, and the check let go
EMBED_INLINE void check_one_run(const ServersCheckOneCtx *args)
{
    const char *path = args->path;
    const char *text = args->text;
    unsigned long long size = args->size;
    char uri[CLI_PATH_MAX_LEN];
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_servers_path_uri_internal.uri_get, CliServersPathUriCtx, .path = path, .uri_out = uri);
    EMBED_CALL(cli_lsp_length.key_get, CliLspLengthCfg, .uri = uri,
               .length = cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), .key = key);
    char quiet[1024];
    VerbaLinea err_msg = {quiet, sizeof(quiet), 0ull};
    if ((text == NULL) || (size > CLI_SERVERS_LARGEST) ||
        !EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg, .bytes = text, .size = size))
    {
        if (EMBED_CALL(cli_servers_uri_internal.check_open, CliServersUriCtx, .uri = uri))
        {
            EMBED_CALL(cli_servers_uri_internal.check_open_set, CliServersUriCtx, .uri = uri, .open = 0);
            (void)EMBED_CALL(cli_lsp_err_msg_text.close, CliLspErrMsgTextCfg, .lsp = args->one->server, .path = path,
                             .err_msg = &err_msg);
        }
        return;
    }
    if (EMBED_CALL(cli_servers_internal.slicing, CliServersCtx, .server_read = args->one->server))
    {
        EMBED_CALL(check_one_slice, ServersCheckOneCtx, .checker = args->checker, .path = path, .uri = uri, .key = key,
                   .text = text, .size = size, .one = args->one, .sink = args->sink);
        return;
    }
    if (!EMBED_CALL(check_one_free, ServersCheckOneCtx, .checker = args->checker, .path = path, .uri = uri, .key = key,
                    .one = args->one, .text = text, .size = size))
    {
        return;
    }
    EMBED_CALL(check_one_request, ServersCheckOneCtx, .checker = args->checker, .path = path, .uri = uri, .key = key,
               .one = args->one, .sink = args->sink);
    const int ours = EMBED_CALL(cli_servers_uri_internal.check_open, CliServersUriCtx, .uri = uri);
    EMBED_CALL(cli_servers_uri_internal.check_open_set, CliServersUriCtx, .uri = uri, .open = 0);
    s_wait[args->checker].waiting = 0;
    if (ours)
    {
        (void)EMBED_CALL(cli_keyed_key_adjacent.put, CliKeyedKeyAdjacentCfg, .keyed_out = &s_letting, .key = key,
                         .adjacent_value = "", .text = "", .size_value = 0ull);
    }
    if (ours)
    {
        (void)EMBED_CALL(cli_lsp_err_msg_text.close, CliLspErrMsgTextCfg, .lsp = args->one->server, .path = path,
                         .err_msg = &err_msg);
    }
}

void cli_servers_check_one(const CliServersPathSinkCtx *args)
{
    // the file read where it lies for as long as it is checked
    unsigned long long size = 0ull;
    const unsigned char *const mapped = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = args->path,
                                                   .size_pointer = &size, .owner = &g_cli_servers_cells);
    EMBED_CALL(check_one_run, ServersCheckOneCtx, .checker = args->checker, .path = args->path,
               .text = (const char *)mapped, .size = size, .one = args->one_servers_waiting_pointer,
               .sink = args->sink);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = mapped, .owner = &g_cli_servers_cells);
}
