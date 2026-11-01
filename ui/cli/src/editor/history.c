// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// history.c: a file's snapshots stored, listed and read back
#include "history.h"
#include "../command_line/cells.h"
#include "../text/regexp.h"
#include "../text/text.h"

// the most snapshots a file retains, and the most a folder of them is read as containing
#define CLI_HISTORY_RETAIN_MAX 100u

#define CLI_HISTORY_STAMPS 4096u

static const char *const s_no_home = "orior_cli has no folder of its own here";

// the stamps of a folder of snapshots, guarded by the lock while one is stored or listed
static unsigned long long s_stamps[CLI_HISTORY_STAMPS];
static CliLock s_lock = CLI_LOCK_INIT;

// whether the snapshot at `snapshot` contains `size` bytes of `text`, the snapshot mapped where it lies and let go
static int cli_snapshot_same(const char *snapshot, const char *text, unsigned long long size)
{
    unsigned long long read = 0ull;
    const unsigned char *const snapshot_bytes = cli_file_taken(snapshot, &read, &g_cli_editor_cells);
    const int same = (snapshot_bytes != NULL) && (read == size) &&
                     (memor.cmp(&(MemoriaCfg){.src = snapshot_bytes, .other = text, .bytes = (size_t)size}) == 0);
    cli_file_given(snapshot_bytes, &g_cli_editor_cells);
    return same;
}

// FNV-1a over the bytes, which names a folder the same on every run, sixteen hex digits
static void cli_hash_put(VerbaLinea *line, const char *text)
{
    unsigned long long value = 0xcbf29ce484222325ull;
    for (const char *at = text; at[0] != '\0'; at += 1)
    {
        value = (value ^ (unsigned char)at[0]) * 0x100000001b3ull;
    }
    mmgr_line_hex(line, value, 16u);
}

// the folder of a file's snapshots under orior's folder `home` into `folder`, sized for CLI_PATH_MAX_LEN: 1, or 0
// where it does not fit
static int cli_folder_of(const char *home, const char *root, const char *path, char *folder)
{
    const char *const slash =
        cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '/');
    const char *const name = (slash != NULL) ? (slash + 1) : path;
    unsigned int letters[CLI_PATH_MAX_LEN];
    const unsigned long long count = cli_regexp_letters(
        name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), letters, CLI_PATH_MAX_LEN);
    VerbaLinea line = {folder, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, home);
    mmgr_line_put(&line, "/history/");
    cli_hash_put(&line, root);
    mmgr_line_ch(&line, '/');
    cli_hash_put(&line, path);
    mmgr_line_ch(&line, '-');
    for (unsigned long long at = 0ull; at < count; at += 1ull)
    {
        const unsigned int letter = letters[at];
        if (cli_regexp_property(CLI_REGEXP_LETTER, letter) || cli_regexp_property(CLI_REGEXP_NUMBER, letter) ||
            (letter == '.') || (letter == '_') || (letter == '-'))
        {
            cli_utf8_put(&line, letter);
        }
    }
    const int fits = mmgr_line_ok(&line);
    mmgr_line_finish(&line);
    return fits;
}

// the path of the snapshot at `at` in `folder` into `path`, sized for CLI_PATH_MAX_LEN
static int cli_snapshot_path(const char *folder, unsigned long long at, char *path)
{
    VerbaLinea line = {path, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_decimal(&line, at, 1u);
    mmgr_line_put(&line, ".txt");
    const int fits = mmgr_line_ok(&line);
    mmgr_line_finish(&line);
    return fits;
}

// The stamps found in a folder: each `<number>.txt`
typedef struct
{
    unsigned int count;
} CliStamps;

static void cli_stamp_each(void *context, const char *name, int is_folder)
{
    CliStamps *const stamps = (CliStamps *)context;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    if (is_folder || (length <= 4u) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = name + length - 4u, .other = ".txt", .cap = CLI_CELLS_POOL})) ||
        (stamps->count >= CLI_HISTORY_STAMPS))
    {
        return;
    }
    unsigned long long value = 0ull;
    for (unsigned long long at = 0ull; at < (length - 4u); at += 1ull)
    {
        const char digit = name[at];
        if ((digit < '0') || (digit > '9') ||
            (value > ((0xffffffffffffffffull - (unsigned long long)(digit - '0')) / 10ull)))
        {
            return;
        }
        value = (value * 10ull) + (unsigned long long)(digit - '0');
    }
    s_stamps[stamps->count] = value;
    stamps->count += 1u;
}

// the stamps of a folder in order, the oldest first: their count
static unsigned int cli_stamps(const char *folder)
{
    CliStamps stamps = {0u};
    query_folder_each(folder, cli_stamp_each, &stamps);
    for (unsigned int at = 1u; at < stamps.count; at += 1u)
    {
        const unsigned long long stamp = s_stamps[at];
        unsigned int place = at;
        while ((place > 0u) && (s_stamps[place - 1u] > stamp))
        {
            s_stamps[place] = s_stamps[place - 1u];
            place -= 1u;
        }
        s_stamps[place] = stamp;
    }
    return stamps.count;
}

static int cli_snapshot_saved(const char *root, const char *path, const char *before, unsigned long long before_size,
                              const char *text, unsigned long long size, VerbaLinea *err_msg)
{
    char home[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    char snapshot[CLI_PATH_MAX_LEN];
    if (!cli_home_folder(home))
    {
        mmgr_line_put(err_msg, s_no_home);
        return 0;
    }
    if (!cli_folder_of(home, root, path, folder) || !query_folder_made(folder))
    {
        mmgr_line_put(err_msg, folder);
        mmgr_line_put(err_msg, ": it could not be made");
        return 0;
    }
    unsigned int stamp_count = cli_stamps(folder);
    unsigned long long at = cli_wall_milliseconds();
    const int before_differs =
        (before != NULL) &&
        ((before_size != size) || (memor.cmp(&(MemoriaCfg){.src = before, .other = text, .bytes = size}) != 0));
    if ((stamp_count == 0u) && before_differs)
    {
        if (!cli_snapshot_path(folder, at, snapshot) || !query_file_written(snapshot, before, before_size))
        {
            mmgr_line_put(err_msg, snapshot);
            mmgr_line_put(err_msg, ": it could not be written");
            return 0;
        }
        s_stamps[stamp_count] = at;
        stamp_count += 1u;
        at += 1ull;
    }
    if (stamp_count != 0u)
    {
        const unsigned long long last = s_stamps[stamp_count - 1u];
        if (cli_snapshot_path(folder, last, snapshot) && cli_snapshot_same(snapshot, text, size))
        {
            return 1;
        }
        at = (at > (last + 1ull)) ? at : (last + 1ull);
    }
    if (!cli_snapshot_path(folder, at, snapshot) || !query_file_written(snapshot, text, size))
    {
        mmgr_line_put(err_msg, snapshot);
        mmgr_line_put(err_msg, ": it could not be written");
        return 0;
    }
    stamp_count += (stamp_count < CLI_HISTORY_STAMPS) ? 1u : 0u;
    for (unsigned int old = 0u;
         (stamp_count > CLI_HISTORY_RETAIN_MAX) && (old < (stamp_count - CLI_HISTORY_RETAIN_MAX)); old += 1u)
    {
        if (cli_snapshot_path(folder, s_stamps[old], snapshot))
        {
            query_removed(snapshot);
        }
    }
    return 1;
}

int cli_history_save(const char *root, const char *path, const char *before, unsigned long long before_size,
                     const char *text, unsigned long long size, VerbaLinea *err_msg)
{
    cli_lock_take(&s_lock);
    const int saved = cli_snapshot_saved(root, path, before, before_size, text, size, err_msg);
    cli_lock_give(&s_lock);
    return saved;
}

void cli_history_list(const char *root, const char *path, VerbaLinea *out)
{
    char home[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    char snapshot[CLI_PATH_MAX_LEN];
    mmgr_line_ch(out, '[');
    cli_lock_take(&s_lock);
    const unsigned int count =
        (cli_home_folder(home) && cli_folder_of(home, root, path, folder)) ? cli_stamps(folder) : 0u;
    for (unsigned int at = count; at > 0u; at -= 1u)
    {
        unsigned long long size = 0ull;
        if (!cli_snapshot_path(folder, s_stamps[at - 1u], snapshot) || !cli_file_size(snapshot, &size))
        {
            size = 0ull;
        }
        mmgr_line_put(out, (at != count) ? ",{\"at\":" : "{\"at\":");
        mmgr_line_decimal(out, s_stamps[at - 1u], 1u);
        mmgr_line_put(out, ",\"size\":");
        mmgr_line_decimal(out, size, 1u);
        mmgr_line_ch(out, '}');
    }
    cli_lock_give(&s_lock);
    mmgr_line_ch(out, ']');
}

int cli_history_read(const char *root, const char *path, unsigned long long at, VerbaLinea *out, VerbaLinea *err_msg)
{
    char home[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    char snapshot[CLI_PATH_MAX_LEN];
    if (!cli_home_folder(home))
    {
        mmgr_line_put(err_msg, s_no_home);
        return 0;
    }
    unsigned long long file_size = 0ull;
    const unsigned long long room = mmgr_line_ok(out) ? (out->capacity - out->at) : 0ull;
    const int found = cli_folder_of(home, root, path, folder) && cli_snapshot_path(folder, at, snapshot) &&
                      cli_file_size(snapshot, &file_size) && (room > file_size);
    const unsigned long long size =
        found ? qry_member_file_read(snapshot, (unsigned char *)(out->out + out->at), room) : QRY_MEMBER_UNREAD;
    if (size == QRY_MEMBER_UNREAD)
    {
        mmgr_line_put(err_msg, "no snapshot of ");
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, " at ");
        mmgr_line_decimal(err_msg, at, 1u);
        return 0;
    }
    out->at += size;
    return 1;
}
