// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_check_one.c: one file of the tree's check: handed to its server as the disk holds it, the server asked for
// its diagnostics or, where it refuses to be asked, waited on for them and for any it gives after them, then let go
// where the check still holds it, its clearing as it closes not passed on. The files a server holds for the check and
// not for the editor are kept by their URIs; a server is written to about a file only with them held, by the check or
// by the editor. What the check hears is kept apart, each file by its key: the file whose first diagnostics wake its
// checker, and the files it let go; its lock is never held while a server is written to, as the thread that reads a
// server takes it
#include "../command_line/cells.h"
#include "servers_internal.h"

// the most files held for the check and let go by it at once; and the most servers that refuse to be asked for a
// file's diagnostics
#define CLI_SERVERS_HELD (1u << 16u)
#define CLI_SERVERS_REFUSED 64u

// A checker's wait: whether it waits, the key of the file it waits on, how often it was woken, and its wake
typedef struct
{
    int waiting;
    char key[CLI_PATH_LONGEST];
    unsigned int signals;
    CliWake wake;
} CliServersWait;

static CliKeyed s_held = {"held checks", {NULL}, 0u, CLI_SERVERS_HELD};
CliLock g_cli_servers_held_lock = CLI_LOCK_INIT;

static CliKeyed s_letting = {"checks let go", {NULL}, 0u, CLI_SERVERS_HELD};
static CliServersWait s_wait[CLI_SERVERS_CHECKING];
static CliLock s_heard_lock = CLI_LOCK_INIT;

static unsigned long long s_refused[CLI_SERVERS_REFUSED];
static unsigned int s_refusing;
static CliLock s_refused_lock = CLI_LOCK_INIT;

int cli_servers_held(const char *uri)
{
    const char *beside = NULL;
    unsigned long long size = 0ull;
    return cli_keyed_get(&s_held, uri, &beside, &size) != NULL;
}

void cli_servers_hold(const char *uri, int held)
{
    if (held)
    {
        (void)cli_keyed_put(&s_held, uri, "", "", 0ull);
        return;
    }
    cli_keyed_remove(&s_held, uri);
}

void cli_servers_held_cleared(void)
{
    cli_keyed_clear(&s_held);
}

// a file a server has open put on the line that is `context`, ended by a 0
static void check_one_open_kept(void *context, const char *path, const char *text, unsigned long long length)
{
    (void)text;
    (void)length;
    cli_line_counted((CliLine *)context, path,
                     cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1ull);
}

void cli_servers_read_again_held(CliLsp *server)
{
    // the files the server has open, one after another each ended by a 0, in a cell of their size
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    CliLine line;
    cli_pieces_begun(&pieces, &line, piece, sizeof(piece));
    cli_lsp_held_each(server, check_one_open_kept, &line);
    unsigned long long size = 0ull;
    char *const open = cli_pieces_joined(&pieces, &line, "the files a server has open", &size);
    for (unsigned long long at = 0ull; (open != NULL) && (at < size);
         at += cellul.len(&(CatenaFinitaCfg){.src = open + at, .cap = CLI_CELLS_POOL}) + 1u)
    {
        char uri[CLI_PATH_LONGEST];
        cli_servers_uri_of(open + at, uri);
        if (!cli_servers_held(uri))
        {
            char quiet[256];
            CliLine ignored = {quiet, sizeof(quiet), 0ull};
            (void)cli_lsp_read_again(server, open + at, &ignored);
        }
    }
    cli_cell_given(open);
}

void cli_servers_heard_opened(const char *key)
{
    cli_lock_take(&s_heard_lock);
    cli_keyed_remove(&s_letting, key);
    cli_lock_give(&s_heard_lock);
}

void cli_servers_woken(const char *key)
{
    cli_lock_take(&s_heard_lock);
    for (unsigned int at = 0u; at < CLI_SERVERS_CHECKING; at += 1u)
    {
        if (s_wait[at].waiting &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_wait[at].key, .other = key, .cap = CLI_CELLS_POOL})))
        {
            s_wait[at].signals += 1u;
            cli_wake_all(&s_wait[at].wake);
        }
    }
    cli_lock_give(&s_heard_lock);
}

int cli_servers_letting_go(const char *key)
{
    cli_lock_take(&s_heard_lock);
    const char *beside = NULL;
    unsigned long long size = 0ull;
    const int letting = cli_keyed_get(&s_letting, key, &beside, &size) != NULL;
    cli_keyed_remove(&s_letting, key);
    cli_lock_give(&s_heard_lock);
    return letting;
}

void cli_servers_wait_on(unsigned int checker, const char *key)
{
    cli_lock_take(&s_heard_lock);
    s_wait[checker].waiting = 1;
    s_wait[checker].signals = 0u;
    memor.cpy(&(MemoriaCfg){.dst = s_wait[checker].key,
                            .src = key,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u});
    cli_lock_give(&s_heard_lock);
}

int cli_servers_was_woken(unsigned int checker)
{
    cli_lock_take(&s_heard_lock);
    const int woken = s_wait[checker].signals > 0u;
    s_wait[checker].signals = 0u;
    s_wait[checker].waiting = 0;
    cli_lock_give(&s_heard_lock);
    return woken;
}

void cli_servers_let_go_of(const char *key)
{
    cli_lock_take(&s_heard_lock);
    (void)cli_keyed_put(&s_letting, key, "", "", 0ull);
    cli_lock_give(&s_heard_lock);
}

// whether the checker `checker` was woken within `milliseconds`, its wakes counted off; under the heard lock
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
        (void)cli_wake_wait_for(&s_wait[checker].wake, &s_heard_lock, until - now);
    }
    s_wait[checker].signals = 0u;
    return 1;
}

// the checker waited on its server to give a file's diagnostics, and for any it gives after them
static void check_one_wait(unsigned int checker)
{
    cli_lock_take(&s_heard_lock);
    if (check_one_woken_within(checker, CLI_SERVERS_PATIENCE))
    {
        while (check_one_woken_within(checker, CLI_SERVERS_QUIET))
        {
        }
    }
    cli_lock_give(&s_heard_lock);
}

// whether the server of process `pid` refused to be asked for a file's diagnostics; and it kept as one that did
static int check_one_refused(unsigned long long pid, int keep)
{
    cli_lock_take(&s_refused_lock);
    int refused = 0;
    for (unsigned int at = 0u; !refused && (at < s_refusing); at += 1u)
    {
        refused = (s_refused[at] == pid);
    }
    if (keep && !refused && (s_refusing < CLI_SERVERS_REFUSED))
    {
        s_refused[s_refusing] = pid;
        s_refusing += 1u;
    }
    cli_lock_give(&s_refused_lock);
    return refused;
}

// the file handed to its server as the disk holds it, `size` bytes of `text`: 1, or 0 where the editor holds it or it
// could not be given
static int check_one_given(unsigned int checker, const char *path, const char *uri, const char *key,
                           const CliServersWaiting *one, const char *text, unsigned long long size)
{
    cli_lock_take(&g_cli_servers_held_lock);
    const int open = cli_lsp_has(one->server, path);
    if (open && !cli_servers_held(uri))
    {
        cli_lock_give(&g_cli_servers_held_lock);
        return 0;
    }
    cli_lock_take(&s_heard_lock);
    s_wait[checker].waiting = 1;
    memor.cpy(&(MemoriaCfg){.dst = s_wait[checker].key,
                            .src = key,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u});
    s_wait[checker].signals = 0u;
    cli_keyed_remove(&s_letting, key);
    cli_lock_give(&s_heard_lock);
    cli_servers_hold(uri, 1);
    char quiet[1024];
    CliLine said = {quiet, sizeof(quiet), 0ull};
    const int sent = open ? cli_lsp_changed(one->server, path, text, size, &said)
                          : cli_lsp_opened(one->server, path, one->id, text, size, &said);
    if (!sent)
    {
        cli_servers_hold(uri, 0);
        cli_lock_take(&s_heard_lock);
        s_wait[checker].waiting = 0;
        cli_lock_give(&s_heard_lock);
    }
    cli_lock_give(&g_cli_servers_held_lock);
    return sent;
}

// the server asked for the file's diagnostics, those it gives whole kept and passed on, or waited on to give them
// where it refuses to be asked
static void check_one_asked(unsigned int checker, const char *uri, const char *key, const CliServersWaiting *one,
                            const CliServersEmit *emit)
{
    const unsigned long long pid = cli_lsp_pid(one->server);
    if (check_one_refused(pid, 0))
    {
        check_one_wait(checker);
        return;
    }
    char params_piece[CLI_PATH_LONGEST];
    CliPieces params_pieces;
    CliLine params;
    cli_pieces_begun(&params_pieces, &params, params_piece, sizeof(params_piece));
    cli_line_text(&params, "{\"textDocument\":{\"uri\":");
    cli_json_quoted(&params, uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}));
    cli_line_text(&params, "}}");
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    CliLine result;
    cli_pieces_begun(&pieces, &result, piece, sizeof(piece));
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    if (!cli_servers_asked(one->server, "textDocument/diagnostic", &params_pieces, &params, CLI_SERVERS_PATIENCE,
                           &result, &said))
    {
        cli_pieces_given(&pieces);
        unsigned long long at = 0ull;
        if (!cli_text_find(why, said.at, "had no answer", 13ull, &at))
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
        cli_cell_given(answer);
        return;
    }
    const int has_items = cli_lsp_member(answer, size, "items", &items_at, &items_length) &&
                          (answer[items_at + cli_text_lead(answer + items_at, items_length)] == '[');
    cli_lock_take(&g_cli_servers_published_lock);
    (void)cli_keyed_put(&g_cli_servers_published, key, uri, has_items ? answer + items_at : "[]",
                        has_items ? items_length : 2ull);
    cli_lock_give(&g_cli_servers_published_lock);
    cli_cell_given(answer);
    cli_servers_emit_whole(key, uri, emit);
}

// one file checked by the checker `checker`, its `size` bytes of `text` as the disk holds them; NULL where it is not
// read whole, and the check let go
static void check_one_held(unsigned int checker, const char *path, const char *text, unsigned long long size,
                           const CliServersWaiting *one, const CliServersEmit *emit)
{
    char uri[CLI_PATH_LONGEST];
    char key[CLI_PATH_LONGEST];
    cli_servers_uri_of(path, uri);
    cli_lsp_key_of(uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), key);
    char quiet[1024];
    CliLine said = {quiet, sizeof(quiet), 0ull};
    if ((text == NULL) || (size > CLI_SERVERS_LARGEST) || !cli_utf8_whole(text, size))
    {
        cli_lock_take(&g_cli_servers_held_lock);
        if (cli_servers_held(uri))
        {
            cli_servers_hold(uri, 0);
            (void)cli_lsp_closed(one->server, path, &said);
        }
        cli_lock_give(&g_cli_servers_held_lock);
        return;
    }
    if (!check_one_given(checker, path, uri, key, one, text, size))
    {
        return;
    }
    check_one_asked(checker, uri, key, one, emit);
    cli_lock_take(&g_cli_servers_held_lock);
    const int ours = cli_servers_held(uri);
    cli_servers_hold(uri, 0);
    cli_lock_take(&s_heard_lock);
    s_wait[checker].waiting = 0;
    if (ours)
    {
        (void)cli_keyed_put(&s_letting, key, "", "", 0ull);
    }
    cli_lock_give(&s_heard_lock);
    if (ours)
    {
        (void)cli_lsp_closed(one->server, path, &said);
    }
    cli_lock_give(&g_cli_servers_held_lock);
}

void cli_servers_check_one(unsigned int checker, const char *path, const CliServersWaiting *one,
                           const CliServersEmit *emit)
{
    // the file read where it lies for as long as it is checked
    unsigned long long size = 0ull;
    const unsigned char *const mapped = qry_member_file_mapped(path, &size);
    check_one_held(checker, path, (const char *)mapped, size, one, emit);
    if (mapped != NULL)
    {
        qry_member_file_unmapped(mapped, size);
    }
}
