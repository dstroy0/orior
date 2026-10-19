// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// history.c: a file's snapshots kept, listed and read back
#include "history.h"
#include "../text/regexp.h"
#include "../text/text.h"

// the most snapshots a file keeps, and the most a folder of them is read as holding
#define CLI_HISTORY_KEPT 100u

#define CLI_HISTORY_STAMPS 4096u

// the most bytes of the newest snapshot read to compare a save with it
#define CLI_HISTORY_BYTES (1u << 24u)

static const char *const s_no_home = "orior has no folder of its own here";

// the stamps of a folder of snapshots and the newest's text, held under the lock while one is kept or listed
static unsigned long long s_stamps[CLI_HISTORY_STAMPS];
static unsigned char s_kept[CLI_HISTORY_BYTES];
static CliLock s_lock = CLI_LOCK_INIT;

// FNV-1a over the bytes, which names a folder the same on every run, sixteen hex digits
static void cli_hash_put(CliLine *line, const char *text)
{
    unsigned long long value = 0xcbf29ce484222325ull;
    for (const char *at = text; at[0] != '\0'; at += 1)
    {
        value = (value ^ (unsigned char)at[0]) * 0x100000001b3ull;
    }
    cli_line_hex(line, value, 16u);
}

// the folder of a file's snapshots under orior's folder `home` into `folder`, which holds CLI_PATH_LONGEST: 1, or 0
// where it does not fit
static int cli_folder_of(const char *home, const char *root, const char *path, char *folder)
{
    const char *const slash =
        cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '/');
    const char *const name = (slash != NULL) ? (slash + 1) : path;
    unsigned int letters[CLI_PATH_LONGEST];
    const unsigned long long count = cli_regexp_letters(
        name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), letters, CLI_PATH_LONGEST);
    CliLine line = {folder, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, home);
    cli_line_text(&line, "/history/");
    cli_hash_put(&line, root);
    cli_line_character(&line, '/');
    cli_hash_put(&line, path);
    cli_line_character(&line, '-');
    for (unsigned long long at = 0ull; at < count; at += 1ull)
    {
        const unsigned int letter = letters[at];
        if (cli_regexp_property(CLI_REGEXP_LETTER, letter) || cli_regexp_property(CLI_REGEXP_NUMBER, letter) ||
            (letter == '.') || (letter == '_') || (letter == '-'))
        {
            cli_utf8_put(&line, letter);
        }
    }
    const int fits = cli_line_fits(&line);
    cli_line_closed(&line);
    return fits;
}

// the path of the snapshot at `at` in `folder` into `path`, which holds CLI_PATH_LONGEST
static int cli_snapshot_path(const char *folder, unsigned long long at, char *path)
{
    CliLine line = {path, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, folder);
    cli_line_character(&line, '/');
    cli_line_decimal(&line, at, 1u);
    cli_line_text(&line, ".txt");
    const int fits = cli_line_fits(&line);
    cli_line_closed(&line);
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
        const unsigned long long held = s_stamps[at];
        unsigned int place = at;
        while ((place > 0u) && (s_stamps[place - 1u] > held))
        {
            s_stamps[place] = s_stamps[place - 1u];
            place -= 1u;
        }
        s_stamps[place] = held;
    }
    return stamps.count;
}

static int cli_kept_held(const char *root, const char *path, const char *before, unsigned long long before_size,
                         const char *text, unsigned long long size, CliLine *said)
{
    char home[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    char snapshot[CLI_PATH_LONGEST];
    if (!cli_home_folder(home))
    {
        cli_line_text(said, s_no_home);
        return 0;
    }
    if (!cli_folder_of(home, root, path, folder) || !query_folder_made(folder))
    {
        cli_line_text(said, folder);
        cli_line_text(said, ": it could not be made");
        return 0;
    }
    unsigned int held = cli_stamps(folder);
    unsigned long long at = cli_wall_milliseconds();
    const int before_differs =
        (before != NULL) &&
        ((before_size != size) || (memor.cmp(&(MemoriaCfg){.src = before, .other = text, .bytes = size}) != 0));
    if ((held == 0u) && before_differs)
    {
        if (!cli_snapshot_path(folder, at, snapshot) || !query_file_written(snapshot, before, before_size))
        {
            cli_line_text(said, snapshot);
            cli_line_text(said, ": it could not be written");
            return 0;
        }
        s_stamps[held] = at;
        held += 1u;
        at += 1ull;
    }
    if (held != 0u)
    {
        const unsigned long long last = s_stamps[held - 1u];
        if (cli_snapshot_path(folder, last, snapshot))
        {
            const unsigned long long read = qry_member_file_read(snapshot, s_kept, sizeof(s_kept));
            if ((read == size) && (memor.cmp(&(MemoriaCfg){.src = s_kept, .other = text, .bytes = size}) == 0))
            {
                return 1;
            }
        }
        at = (at > (last + 1ull)) ? at : (last + 1ull);
    }
    if (!cli_snapshot_path(folder, at, snapshot) || !query_file_written(snapshot, text, size))
    {
        cli_line_text(said, snapshot);
        cli_line_text(said, ": it could not be written");
        return 0;
    }
    held += (held < CLI_HISTORY_STAMPS) ? 1u : 0u;
    for (unsigned int old = 0u; (held > CLI_HISTORY_KEPT) && (old < (held - CLI_HISTORY_KEPT)); old += 1u)
    {
        if (cli_snapshot_path(folder, s_stamps[old], snapshot))
        {
            query_removed(snapshot);
        }
    }
    return 1;
}

int cli_history_keep(const char *root, const char *path, const char *before, unsigned long long before_size,
                     const char *text, unsigned long long size, CliLine *said)
{
    cli_lock_take(&s_lock);
    const int kept = cli_kept_held(root, path, before, before_size, text, size, said);
    cli_lock_give(&s_lock);
    return kept;
}

void cli_history_list(const char *root, const char *path, CliLine *out)
{
    char home[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    char snapshot[CLI_PATH_LONGEST];
    cli_line_character(out, '[');
    cli_lock_take(&s_lock);
    const unsigned int held =
        (cli_home_folder(home) && cli_folder_of(home, root, path, folder)) ? cli_stamps(folder) : 0u;
    for (unsigned int at = held; at > 0u; at -= 1u)
    {
        unsigned long long size = 0ull;
        if (!cli_snapshot_path(folder, s_stamps[at - 1u], snapshot) || !cli_file_size(snapshot, &size))
        {
            size = 0ull;
        }
        cli_line_text(out, (at != held) ? ",{\"at\":" : "{\"at\":");
        cli_line_decimal(out, s_stamps[at - 1u], 1u);
        cli_line_text(out, ",\"size\":");
        cli_line_decimal(out, size, 1u);
        cli_line_character(out, '}');
    }
    cli_lock_give(&s_lock);
    cli_line_character(out, ']');
}

int cli_history_read(const char *root, const char *path, unsigned long long at, CliLine *out, CliLine *said)
{
    char home[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    char snapshot[CLI_PATH_LONGEST];
    if (!cli_home_folder(home))
    {
        cli_line_text(said, s_no_home);
        return 0;
    }
    const unsigned long long room = cli_line_fits(out) ? (out->capacity - out->at) : 0ull;
    const unsigned long long size = (cli_folder_of(home, root, path, folder) && cli_snapshot_path(folder, at, snapshot))
                                        ? qry_member_file_read(snapshot, (unsigned char *)(out->out + out->at), room)
                                        : QRY_MEMBER_UNREAD;
    if (size == QRY_MEMBER_UNREAD)
    {
        cli_line_text(said, "no snapshot of ");
        cli_line_text(said, path);
        cli_line_text(said, " at ");
        cli_line_decimal(said, at, 1u);
        return 0;
    }
    out->at += size;
    return 1;
}
