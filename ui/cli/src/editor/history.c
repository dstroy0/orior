// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// history.c: a file's snapshots stored, listed and read back
#include "history.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../text/regexp.h"
#include "../text/text.h"

// the most snapshots a file retains, and the most a folder of them is read as containing
#define CLI_HISTORY_RETAIN_MAX 100u

#define CLI_HISTORY_STAMPS 4096u

static const char *const s_no_home = "impero has no folder of its own here";

// the stamps of a folder of snapshots, guarded by the lock while one is stored or listed
static unsigned long long s_stamps[CLI_HISTORY_STAMPS];

typedef struct
{
    const char *snapshot;
    const char *text;
    unsigned long long size;
    VerbaLinea *line;
    const char *home;
    const char *root;
    const char *path;
    char *folder_out;
    const char *folder;
    unsigned long long at;
    char *path_out;
    const char *before;
    unsigned long long before_size;
    VerbaLinea *err_msg;
} HistoryCtx;
// whether the snapshot at `snapshot` contains `size` bytes of `text`, the snapshot mapped where it lies and let go
EMBED_INLINE int cli_snapshot_same(const HistoryCtx *args)
{
    unsigned long long size = args->size;
    unsigned long long read = 0ULL;
    const unsigned char *const snapshot_bytes =
        EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = args->snapshot, .size_pointer = &read,
                   .owner = g_cli_editor_owner);
    const int same =
        (snapshot_bytes != NULL) && (read == size) &&
        (memor.cmp(&(MemoriaCfg){.src = snapshot_bytes, .other = args->text, .bytes = (embed_word)size}) == 0);
    g_cli_guard->persistent_buf_release((void *)(embed_word)snapshot_bytes, (g_cli_editor_owner));
    return same;
}

// FNV-1a over the bytes, which names a folder the same on every run, sixteen hex digits
EMBED_INLINE void cli_hash_append(const HistoryCtx *args)
{
    unsigned long long value = 0xcbf29ce484222325ULL;
    for (const char *at = args->text; at[0] != '\0'; at += 1)
    {
        value = (value ^ (unsigned char)at[0]) * 0x100000001b3ULL;
    }
    mmgr_line_hex(args->line, value, 16u);
}

// the folder of a file's snapshots under orior's folder `home` into `folder`, sized for CLI_PATH_MAX_LEN: 1, or 0
// where it does not fit
EMBED_INLINE int cli_folder_get(const HistoryCtx *args)
{
    const char *const slash = EMBED_CALL(
        cli_text_byte.last, CliTextByteCfg, .text = args->path,
        .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}), .byte = '/');
    const char *const name = (slash != NULL) ? (slash + 1) : args->path;
    unsigned int letters[CLI_PATH_MAX_LEN];
    const unsigned long long count =
        EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = name,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}), .letters = letters,
                   .capacity_u64 = CLI_PATH_MAX_LEN);
    VerbaLinea line = {args->folder_out, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->home);
    mmgr_line_put(&line, "/history/");
    EMBED_CALL(cli_hash_append, HistoryCtx, .line = &line, .text = args->root);
    mmgr_line_ch(&line, '/');
    EMBED_CALL(cli_hash_append, HistoryCtx, .line = &line, .text = args->path);
    mmgr_line_ch(&line, '-');
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        const unsigned int letter = letters[at];
        if (EMBED_CALL(cli_regexp_letter.property, CliRegexpLetterCfg, .property = CLI_REGEXP_LETTER,
                       .letter = letter) ||
            EMBED_CALL(cli_regexp_letter.property, CliRegexpLetterCfg, .property = CLI_REGEXP_NUMBER,
                       .letter = letter) ||
            (letter == '.') || (letter == '_') || (letter == '-'))
        {
            EMBED_CALL(cli_line_letter_internal.utf8_append, CliLineLetterCtx, .line = &line, .letter = letter);
        }
    }
    const int fits = mmgr_line_ok(&line);
    mmgr_line_finish(&line);
    return fits;
}

// the path of the snapshot at `at` in `folder` into `path`, sized for CLI_PATH_MAX_LEN
EMBED_INLINE int cli_snapshot_path(const HistoryCtx *args)
{
    VerbaLinea line = {args->path_out, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_decimal(&line, args->at, 1u);
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

static void cli_stamp_each(const CliEachNameIsFolderCfg *args)
{
    CliStamps *const stamps = (CliStamps *)args->context;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES});
    if (args->is_folder || (length <= 4u) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = args->name + length - 4u, .other = ".txt", .cap = CLI_POOL_BYTES})) ||
        (stamps->count >= CLI_HISTORY_STAMPS))
    {
        return;
    }
    unsigned long long value = 0ULL;
    for (unsigned long long at = 0ULL; at < (length - 4u); at += 1ULL)
    {
        const char digit = args->name[at];
        if ((digit < '0') || (digit > '9') ||
            (value > ((0xffffffffffffffffULL - (unsigned long long)(digit - '0')) / 10ULL)))
        {
            return;
        }
        value = (value * 10ULL) + (unsigned long long)(digit - '0');
    }
    s_stamps[stamps->count] = value;
    stamps->count += 1u;
}

// the stamps of a folder in order, the oldest first: their count
EMBED_INLINE unsigned int cli_stamps(const HistoryCtx *args)
{
    CliStamps stamps = {0u};
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->folder,
               .each = cli_stamp_each, .context_bytess = &stamps);
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

EMBED_INLINE int cli_snapshot_save(const HistoryCtx *args)
{
    const char *path = args->path;
    unsigned long long size = args->size;
    char home[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    char snapshot[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = home))
    {
        mmgr_line_put(args->err_msg, s_no_home);
        return 0;
    }
    if (!EMBED_CALL(cli_folder_get, HistoryCtx, .home = home, .root = args->root, .path = path, .folder_out = folder) ||
        !EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder))
    {
        mmgr_line_put(args->err_msg, folder);
        mmgr_line_put(args->err_msg, ": it could not be made");
        return 0;
    }
    unsigned int stamp_count = EMBED_CALL(cli_stamps, HistoryCtx, .folder = folder);
    unsigned long long at = cli_wall_milliseconds();
    const int before_differs =
        (args->before != NULL) &&
        ((args->before_size != size) ||
         (memor.cmp(&(MemoriaCfg){.src = args->before, .other = args->text, .bytes = size}) != 0));
    if ((stamp_count == 0u) && before_differs)
    {
        if (!EMBED_CALL(cli_snapshot_path, HistoryCtx, .folder = folder, .at = at, .path_out = snapshot) ||
            !EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = snapshot,
                        .bytes = args->before, .size = args->before_size))
        {
            mmgr_line_put(args->err_msg, snapshot);
            mmgr_line_put(args->err_msg, ": it could not be written");
            return 0;
        }
        s_stamps[stamp_count] = at;
        stamp_count += 1u;
        at += 1ULL;
    }
    if (stamp_count != 0u)
    {
        const unsigned long long last = s_stamps[stamp_count - 1u];
        if (EMBED_CALL(cli_snapshot_path, HistoryCtx, .folder = folder, .at = last, .path_out = snapshot) &&
            EMBED_CALL(cli_snapshot_same, HistoryCtx, .snapshot = snapshot, .text = args->text, .size = size))
        {
            return 1;
        }
        at = (at > (last + 1ULL)) ? at : (last + 1ULL);
    }
    if (!EMBED_CALL(cli_snapshot_path, HistoryCtx, .folder = folder, .at = at, .path_out = snapshot) ||
        !EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = snapshot,
                    .bytes = args->text, .size = size))
    {
        mmgr_line_put(args->err_msg, snapshot);
        mmgr_line_put(args->err_msg, ": it could not be written");
        return 0;
    }
    stamp_count += (stamp_count < CLI_HISTORY_STAMPS) ? 1u : 0u;
    for (unsigned int old = 0u;
         (stamp_count > CLI_HISTORY_RETAIN_MAX) && (old < (stamp_count - CLI_HISTORY_RETAIN_MAX)); old += 1u)
    {
        if (EMBED_CALL(cli_snapshot_path, HistoryCtx, .folder = folder, .at = s_stamps[old], .path_out = snapshot))
        {
            EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = snapshot);
        }
    }
    return 1;
}

int cli_history_save(const CliHistoryBeforeCfg *args)
{
    const int saved =
        EMBED_CALL(cli_snapshot_save, HistoryCtx, .root = args->root, .path = args->path, .before = args->before,
                   .before_size = args->before_size, .text = args->text, .size = args->size, .err_msg = args->err_msg);
    return saved;
}

unsigned int cli_history_stamps(const CliHistoryStampsCfg *args)
{
    char home[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = home) ||
        !EMBED_CALL(cli_folder_get, HistoryCtx, .home = home, .root = args->root, .path = args->path,
                    .folder_out = folder))
    {
        return 0u;
    }
    const unsigned int count = EMBED_CALL(cli_stamps, HistoryCtx, .folder = folder);
    const unsigned int copied = (count < args->capacity) ? count : args->capacity;
    memor.cpy(&(MemoriaCfg){.dst = args->stamps_out, .src = s_stamps, .bytes = (embed_word)copied * sizeof(*s_stamps)});
    return copied;
}

int cli_history_read(const CliHistoryCfg *args)
{
    unsigned long long at = args->at;
    char home[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    char snapshot[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = home))
    {
        mmgr_line_put(args->err_msg, s_no_home);
        return 0;
    }
    unsigned long long file_size = 0ULL;
    const unsigned long long capacity = mmgr_line_ok(args->out) ? (args->out->capacity - args->out->at) : 0ULL;
    const int found =
        EMBED_CALL(cli_folder_get, HistoryCtx, .home = home, .root = args->root, .path = args->path,
                   .folder_out = folder) &&
        EMBED_CALL(cli_snapshot_path, HistoryCtx, .folder = folder, .at = at, .path_out = snapshot) &&
        EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = snapshot, .size_pointer = &file_size) &&
        (capacity > file_size);
    const unsigned long long size =
        found ? qry_member_file_read(snapshot, (unsigned char *)(args->out->out + args->out->at), capacity)
              : QRY_MEMBER_UNREAD;
    if (size == QRY_MEMBER_UNREAD)
    {
        mmgr_line_put(args->err_msg, "no snapshot of ");
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, " at ");
        mmgr_line_decimal(args->err_msg, at, 1u);
        return 0;
    }
    args->out->at += size;
    return 1;
}
