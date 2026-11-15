// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// history.c: a file's snapshots stored, listed and read back
#include "history.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../text/regexp.h"
#include "../text/text.h"

// the max snapshots a file retains
#define CLI_HISTORY_RETAIN_MAX 100u

static const char *const s_no_home = "impero has no folder of its own here";

// The stamps of a folder of snapshots as one call reads them: in a movable cell of the call grown as they are
// found, how many there are and the cell's capacity
typedef struct
{
    const CarcerMovableEntry *cell;
    unsigned int count;
    unsigned int capacity;
} HistoryStamps;

// `stamp` added at the end of `stamps`, the cell grown to twice its capacity where it is full: 1, or 0 where the
// cells refuse the growth
static int history_stamp_add(HistoryStamps *stamps, unsigned long long stamp)
{
    if (stamps->count == stamps->capacity)
    {
        const unsigned int capacity = (stamps->capacity != 0u) ? stamps->capacity * 2u : 16u;
        const CarcerMovableEntry *const grown =
            g_cli_guard->movable_buf_alloc((unsigned long long)capacity * sizeof(stamp), g_cli_editor_owner);
        if (grown == NULL)
        {
            return 0;
        }
        for (unsigned int at = 0u; at < stamps->count; at += 1u)
        {
            unsigned long long held = 0ULL;
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                .cell = stamps->cell, .offset = (embed_word)at * sizeof(held), .dst = &held, .bytes = sizeof(held)});
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
                .cell = grown, .offset = (embed_word)at * sizeof(held), .src = &held, .bytes = sizeof(held)});
        }
        if (stamps->cell != NULL)
        {
            (void)g_cli_guard->movable_buf_release(stamps->cell, g_cli_editor_owner);
        }
        stamps->cell = grown;
        stamps->capacity = capacity;
    }
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
        .cell = stamps->cell, .offset = (embed_word)stamps->count * sizeof(stamp), .src = &stamp, .bytes = sizeof(stamp)});
    stamps->count += 1u;
    return 1;
}

// the stamps' cell given back and the list emptied
static void history_stamps_release(HistoryStamps *stamps)
{
    if (stamps->cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(stamps->cell, g_cli_editor_owner);
    }
    *stamps = (HistoryStamps){NULL, 0u, 0u};
}

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
    HistoryStamps *stamps;
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
    unsigned int characters[CLI_PATH_MAX_LEN];
    const unsigned long long count =
        EMBED_CALL(cli_regexp_text.characters, CliRegexpTextCfg, .text = name,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}), .characters = characters,
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
        const unsigned int character = characters[at];
        if (EMBED_CALL(cli_regexp_character.property, CliRegexpLetterCfg, .property = CLI_REGEXP_CHARACTER,
                       .character = character) ||
            EMBED_CALL(cli_regexp_character.property, CliRegexpLetterCfg, .property = CLI_REGEXP_NUMBER,
                       .character = character) ||
            (character == '.') || (character == '_') || (character == '-'))
        {
            EMBED_CALL(cli_line_character_internal.utf8_append, CliLineLetterCtx, .line = &line, .character = character);
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

// one file of the folder read where it names a snapshot, `<number>.txt`: its stamp added to the stamps
static void cli_stamp_each(const CliEachNameIsFolderCfg *args)
{
    HistoryStamps *const stamps = (HistoryStamps *)args->context;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES});
    if (args->is_folder || (length <= 4u) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = args->name + length - 4u, .other = ".txt", .cap = CLI_POOL_BYTES})))
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
    (void)history_stamp_add(stamps, value);
}

// the stamps of a folder read into `stamps` in order, the oldest first: their count
EMBED_INLINE unsigned int cli_stamps(const HistoryCtx *args)
{
    HistoryStamps *const stamps = args->stamps;
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->folder,
               .each = cli_stamp_each, .context_bytess = stamps);
    for (unsigned int at = 1u; at < stamps->count; at += 1u)
    {
        unsigned long long stamp = 0ULL;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = stamps->cell, .offset = (embed_word)at * sizeof(stamp), .dst = &stamp, .bytes = sizeof(stamp)});
        unsigned int place = at;
        // the stamp before the place, moved up while it orders after the stamp
        unsigned long long before = 0ULL;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = stamps->cell,
                                                                .offset = (embed_word)((place > 0u) ? place - 1u : 0u) * sizeof(before),
                                                                .dst = &before,
                                                                .bytes = sizeof(before)});
        while ((place > 0u) && (before > stamp))
        {
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
                .cell = stamps->cell, .offset = (embed_word)place * sizeof(before), .src = &before, .bytes = sizeof(before)});
            place -= 1u;
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = stamps->cell,
                                                                    .offset = (embed_word)((place > 0u) ? place - 1u : 0u) * sizeof(before),
                                                                    .dst = &before,
                                                                    .bytes = sizeof(before)});
        }
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = stamps->cell, .offset = (embed_word)place * sizeof(stamp), .src = &stamp, .bytes = sizeof(stamp)});
    }
    return stamps->count;
}

EMBED_INLINE int cli_snapshot_write(const HistoryCtx *args);

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
    HistoryStamps stamps = {NULL, 0u, 0u};
    const int written = EMBED_CALL(cli_snapshot_write, HistoryCtx, .folder = folder, .stamps = &stamps,
                                   .before = args->before, .before_size = args->before_size, .text = args->text,
                                   .size = size, .err_msg = args->err_msg);
    history_stamps_release(&stamps);
    return written;
}

// the snapshot of `size` bytes of `text` written into the folder `folder` of the file's snapshots, the text before
// the edit first where it differs and no snapshot stands, none where the newest snapshot is the text already, and
// the snapshots past the retained count removed: 1, or 0 with why in `err_msg`
EMBED_INLINE int cli_snapshot_write(const HistoryCtx *args)
{
    const char *const folder = args->folder;
    const unsigned long long size = args->size;
    char snapshot[CLI_PATH_MAX_LEN];
    HistoryStamps *const stamps = args->stamps;
    unsigned int stamp_count = EMBED_CALL(cli_stamps, HistoryCtx, .folder = folder, .stamps = stamps);
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
        stamp_count += history_stamp_add(stamps, at) ? 1u : 0u;
        at += 1ULL;
    }
    if (stamp_count != 0u)
    {
        unsigned long long last = 0ULL;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = stamps->cell, .offset = (embed_word)(stamp_count - 1u) * sizeof(last), .dst = &last, .bytes = sizeof(last)});
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
    // the snapshot written counts among the stamps; the oldest past the retained count are removed
    stamp_count += 1u;
    for (unsigned int old = 0u;
         (stamp_count > CLI_HISTORY_RETAIN_MAX) && (old < (stamp_count - CLI_HISTORY_RETAIN_MAX)); old += 1u)
    {
        unsigned long long oldest = 0ULL;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = stamps->cell, .offset = (embed_word)old * sizeof(oldest), .dst = &oldest, .bytes = sizeof(oldest)});
        if (EMBED_CALL(cli_snapshot_path, HistoryCtx, .folder = folder, .at = oldest, .path_out = snapshot))
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
    HistoryStamps stamps = {NULL, 0u, 0u};
    const unsigned int count = EMBED_CALL(cli_stamps, HistoryCtx, .folder = folder, .stamps = &stamps);
    const unsigned int copied = (count < args->capacity) ? count : args->capacity;
    if (copied != 0u)
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = stamps.cell, .dst = args->stamps_out, .bytes = (embed_word)copied * sizeof(unsigned long long)});
    }
    history_stamps_release(&stamps);
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
