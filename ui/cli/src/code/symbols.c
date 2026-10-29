// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols.c: how well a query names a name, and the tree's declarations: read on a thread of their own, cached with
// each file's time, each file's declarations a cell of their own, and searched in symbols_find.c. The include folders
// and files the compilation database names outside the tree are joined to it as it is brought up to date, each
// mounted beside it, and their files are read as the tree's own, by their mounts' names
#include "../builds/compile_db.h"
#include "../command_line/cells.h"
#include "../editor/files.h"
#include "symbols_internal.h"
#include "symbols_tree.h"

// a file larger than this is not read for its declarations; how long the tree stands after it was last brought up to
// date before a search starts it again, in milliseconds; and the most bytes of the paths a reading goes through
#define CLI_SYMBOLS_LARGEST (1u << 20u)

#define CLI_FRESH_FOR 4000ull

#define CLI_SYMBOL_TEXT (1u << 24u)

// the bytes and the declarations a file's names' room is begun with, each doubling as they are read
#define CLI_SYMBOL_READ_FIRST 4096u
#define CLI_SYMBOLS_READ_FIRST 64u

// the most kinds of declaration of one file whose texts are read once and shared by its declarations
#define CLI_SYMBOLS_KINDS 16u

// The storage a reading goes through, a cell taken as it starts and given back as it ends, sized to the paths of the
// files it reads, one after another, at its end: one file's declarations as read, before they are stored, their names
// and kinds and the declarations each in a cell whose room doubles as they are read, and the place of each kind read
// among those texts; the languages it reads its files in, a cell of their own; and the folders and files mounted
// beside the tree as it started, each its name and its whole path, how many
typedef struct
{
    char *read_text;
    unsigned int text_room;
    CliSymbol *read_symbols;
    unsigned int symbol_room;
    unsigned int kind[CLI_SYMBOLS_KINDS];
    unsigned int kinds;
    unsigned char *file_bytes;
    unsigned long long file_room;
    CliSymbolsLanguages *languages;
    char mount_name[CLI_MOUNTS_MOST][CLI_NAME_MAX_LEN];
    char mount_path[CLI_MOUNTS_MOST][CLI_PATH_MAX_LEN];
    unsigned int mounts;
    char listed[];
} CliSymbolsReading;

// the bytes past a file read in the reading's cell, zero, so a walk's word past the text reads inside the cell
#define CLI_SYMBOLS_FILE_SLACK 64ull

// room for a file of `size` bytes in the reading's one file cell: the cell as it is where it fits them and the slack,
// else one taken anew of twice the cell's bytes or the file's and the slack, the cell before given back; NULL, so the
// file is passed over, where it is larger than the tree reads or no cell is given
static unsigned char *cli_file_roomed(void *context, unsigned long long size)
{
    CliSymbolsReading *const reading = context;
    if (size > CLI_SYMBOLS_LARGEST)
    {
        return NULL;
    }
    if ((size + CLI_SYMBOLS_FILE_SLACK) > reading->file_room)
    {
        g_cli_cells->persistent_buf_release(reading->file_bytes, NULL);
        unsigned long long room = (reading->file_room * 2ull > size + CLI_SYMBOLS_FILE_SLACK)
                                      ? (reading->file_room * 2ull)
                                      : (size + CLI_SYMBOLS_FILE_SLACK);
        room = (room < CLI_SYMBOL_READ_FIRST) ? CLI_SYMBOL_READ_FIRST : room;
        reading->file_bytes = g_cli_cells->persistent_buf_alloc(room, NULL);
        reading->file_room = (reading->file_bytes != NULL) ? room : 0ull;
    }
    return reading->file_bytes;
}

// `cell`, `used` bytes of it occupied, moved into a cell of `room` bytes: the larger, or NULL with `cell` retained
// where it is refused
static void *cli_read_moved(void *cell, unsigned long long used, unsigned long long room, const char *what)
{
    void *const larger = g_cli_cells->persistent_buf_alloc(room, NULL);
    if (larger != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = larger, .src = cell, .bytes = (size_t)used});
        g_cli_cells->persistent_buf_release(cell, NULL);
    }
    return larger;
}

static CliSymbolsReading *s_listing;

// The tree's state: its folder, whether a reading goes and when the last ended, how far it has gone, whether it is
// requested to stop, which reading of the tree it is, and whether its thread runs
char g_cli_symbols_root[CLI_PATH_MAX_LEN];
static int s_rooted;
int g_cli_symbols_reading;
static int s_read;
static unsigned long long s_read_at;
static unsigned long long s_done;
static unsigned long long s_total;
static int s_stop;
static unsigned long long s_generation;
static int s_running;
CliLock g_cli_symbols_lock = CLI_LOCK_INIT;
static CliWake s_wake = CLI_WAKE_INIT;

// how many files a reading goes through, and the thread reading them
static unsigned int s_listed_count;
static CliThread s_thread;

// how many declarations of a file were read and the bytes of their names and kinds, before they are stored
static unsigned int s_read_count;
static unsigned int s_read_used;

// none: a kind not read before in the file
#define CLI_SYMBOLS_KIND_NEW 0xffffffffu

// A declaration read: its name, and its kind's text where the file's declarations have not read it before, after the
// name with its NUL
static void cli_read_each(void *context, const char *name, unsigned long long length, const char *kind,
                          unsigned int line)
{
    CliSymbolsReading *const reading = (CliSymbolsReading *)context;
    unsigned int kind_at = CLI_SYMBOLS_KIND_NEW;
    for (unsigned int at = 0u; (kind_at == CLI_SYMBOLS_KIND_NEW) && (at < reading->kinds); at += 1u)
    {
        kind_at = cellul.eq(&(CatenaFinitaCfg){
                      .src = reading->read_text + reading->kind[at], .other = kind, .cap = CLI_CELLS_POOL})
                      ? reading->kind[at]
                      : CLI_SYMBOLS_KIND_NEW;
    }
    const unsigned long long kind_length =
        (kind_at == CLI_SYMBOLS_KIND_NEW) ? cellul.len(&(CatenaFinitaCfg){.src = kind, .cap = CLI_CELLS_POOL}) + 1ull
                                          : 0ull;
    if ((s_read_count >= (CLI_SYMBOLS_LARGEST / 4u)) || ((s_read_used + length + kind_length) > CLI_SYMBOLS_LARGEST))
    {
        return;
    }
    if (s_read_count == reading->symbol_room)
    {
        const unsigned int room = (reading->symbol_room == 0u) ? CLI_SYMBOLS_READ_FIRST : (reading->symbol_room * 2u);
        CliSymbol *const symbols =
            cli_read_moved(reading->read_symbols, (unsigned long long)s_read_count * sizeof(CliSymbol),
                           (unsigned long long)room * sizeof(CliSymbol), "a file's declarations as read");
        if (symbols == NULL)
        {
            return;
        }
        reading->read_symbols = symbols;
        reading->symbol_room = room;
    }
    unsigned long long room = (reading->text_room == 0u) ? CLI_SYMBOL_READ_FIRST : reading->text_room;
    while (room < (s_read_used + length + kind_length))
    {
        room *= 2ull;
    }
    if (room != reading->text_room)
    {
        char *const text = cli_read_moved(reading->read_text, s_read_used, room, "a file's names as read");
        if (text == NULL)
        {
            return;
        }
        reading->read_text = text;
        // a room no more than twice a file's largest size, which an unsigned int counts
        reading->text_room = (unsigned int)room;
    }
    memor.cpy(&(MemoriaCfg){.dst = reading->read_text + s_read_used, .src = name, .bytes = length});
    if (kind_at == CLI_SYMBOLS_KIND_NEW)
    {
        // a name and a kind are below a file's largest size, which an unsigned int counts
        kind_at = s_read_used + (unsigned int)length;
        memor.cpy(&(MemoriaCfg){.dst = reading->read_text + kind_at, .src = kind, .bytes = (size_t)kind_length});
        reading->kind[reading->kinds] = kind_at;
        reading->kinds += (reading->kinds < (CLI_SYMBOLS_KINDS - 1u)) ? 1u : 0u;
    }
    const CliSymbol symbol = {s_read_used, (unsigned int)length, kind_at, line};
    reading->read_symbols[s_read_count] = symbol;
    s_read_count += 1u;
    s_read_used += (unsigned int)(length + kind_length);
}

// the declarations read put as the file at `path`, written at `written`, in a cell of its own: 1, or 0 where the
// table fits no more files or the cells cannot meet it. Called under the lock
static int cli_symbol_file_put(const CliSymbolsReading *reading, const char *path, unsigned long long written)
{
    const unsigned long long path_length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1ull;
    unsigned int file = cli_symbols_file_of(path);
    if ((file == CLI_SYMBOL_FILES) && !cli_symbols_room_made())
    {
        return 0;
    }
    const unsigned long long symbols_size = s_read_count * sizeof(CliSymbol);
    char *const cell = g_cli_cells->persistent_buf_alloc(symbols_size + path_length + s_read_used, NULL);
    if (cell == NULL)
    {
        return 0;
    }
    if (file == CLI_SYMBOL_FILES)
    {
        file = g_cli_symbol_file_count;
        g_cli_symbol_file_count += 1u;
        memor.cpy(&(MemoriaCfg){.dst = cell + symbols_size, .src = path, .bytes = path_length});
        g_cli_symbol_files[file].cell = cell;
        // a file's declarations, path and names are below what an unsigned int counts
        g_cli_symbol_files[file].path = (unsigned int)symbols_size;
        cli_symbols_bucket_put(path, file);
    }
    else
    {
        memor.cpy(&(MemoriaCfg){.dst = cell + symbols_size, .src = path, .bytes = path_length});
        g_cli_cells->persistent_buf_release(g_cli_symbol_files[file].cell, NULL);
        g_cli_symbol_files[file].cell = cell;
        g_cli_symbol_files[file].path = (unsigned int)symbols_size;
    }
    CliSymbolFile *const branch = &g_cli_symbol_files[file];
    branch->used = 1;
    branch->written = written;
    branch->count = s_read_count;
    branch->seen = 1;
    // the names and kinds read, after the path, each declaration's places among them moved with them
    CliSymbol *const symbols = (CliSymbol *)(void *)cell;
    const unsigned int texts = (unsigned int)(symbols_size + path_length);
    memor.cpy(&(MemoriaCfg){.dst = cell + texts, .src = reading->read_text, .bytes = s_read_used});
    for (unsigned int at = 0u; at < s_read_count; at += 1u)
    {
        CliSymbol symbol = reading->read_symbols[at];
        symbol.name += texts;
        symbol.kind += texts;
        symbols[at] = symbol;
    }
    return 1;
}

// the file at `path` of the tree, a path from its top or under a mount's name, read where its time moved since it was
// last read, its declarations stored; called under the lock, given up while the file is stamped and read: 1, or 0
// where the reading was asked to stop or the tree it reads was let go
static int cli_reading_one(CliSymbolsReading *reading, const char *root, const char *path,
                           unsigned long long generation)
{
    if (s_stop || (s_generation != generation))
    {
        return 0;
    }
    s_done += 1ull;
    cli_lock_give(&g_cli_symbols_lock);
    char full[CLI_PATH_MAX_LEN];
    cli_full(root, path, full);
    unsigned long long size = 0ull;
    unsigned long long written = 0ull;
    const int stamped = cli_file_stamp(full, &size, &written) && (size <= CLI_SYMBOLS_LARGEST);
    cli_lock_take(&g_cli_symbols_lock);
    const unsigned int file = stamped ? cli_symbols_file_of(path) : CLI_SYMBOL_FILES;
    if (file != CLI_SYMBOL_FILES)
    {
        g_cli_symbol_files[file].seen = 1;
        if (g_cli_symbol_files[file].written == written)
        {
            return 1;
        }
    }
    if (!stamped)
    {
        return 1;
    }
    cli_lock_give(&g_cli_symbols_lock);
    // the file read into the reading's one cell, reused from file to file, so a reading takes a cell a growth and not
    // one a file
    unsigned long long mapped_size = 0ull;
    const int mapped =
        cli_file_read_whole(full, cli_file_roomed, reading, &mapped_size) && (reading->file_bytes != NULL);
    const unsigned long long read = mapped ? mapped_size : QRY_MEMBER_UNREAD;
    s_read_count = 0u;
    s_read_used = 0u;
    reading->kinds = 0u;
    if (read != QRY_MEMBER_UNREAD)
    {
        memor.set(&(MemoriaCfg){.dst = reading->file_bytes + read, .val = 0, .bytes = CLI_SYMBOLS_FILE_SLACK});
        // the file's bytes are read as its text
        cli_symbols_declared_in(reading->languages, path, (const char *)reading->file_bytes, read, cli_read_each,
                                reading);
    }
    cli_lock_take(&g_cli_symbols_lock);
    if (s_generation != generation)
    {
        return 0;
    }
    if (read != QRY_MEMBER_UNREAD)
    {
        cli_symbol_file_put(reading, path, written);
    }
    return 1;
}

// A mount's files being read: the reading, the tree's top, the mount's name, which reading of the tree it is, and
// whether it was stopped
typedef struct
{
    CliSymbolsReading *reading;
    const char *root;
    const char *name;
    unsigned long long generation;
    int stopped;
} CliMountReading;

// One file of a mounted folder, its path from the mount's top, read as the tree's own under the mount's name where
// its language is one read here
static void cli_reading_mounted(void *context, const char *path, unsigned long long length)
{
    CliMountReading *const mount = (CliMountReading *)context;
    char named[CLI_PATH_MAX_LEN];
    VerbaLinea line = {named, sizeof(named), 0ull};
    mmgr_line_ch(&line, '@');
    mmgr_line_put(&line, mount->name);
    mmgr_line_ch(&line, '/');
    mmgr_line_put_n(&line, path, length);
    if (mount->stopped || (mmgr_line_finish(&line) == 0ull) || !cli_symbols_reads_in(mount->reading->languages, named))
    {
        return;
    }
    cli_lock_take(&g_cli_symbols_lock);
    s_total += 1ull;
    mount->stopped = !cli_reading_one(mount->reading, mount->root, named, mount->generation);
    cli_lock_give(&g_cli_symbols_lock);
}

// The reading: each file listed, then each file of each folder mounted beside the tree and each file mounted, the
// stamp of its file read, and read again where its time moved; past the last, the files it did not see let go where it
// was not stopped
static void cli_reading(void *context)
{
    CliSymbolsReading *const reading = (CliSymbolsReading *)context;
    cli_lock_take(&g_cli_symbols_lock);
    const unsigned long long generation = s_generation;
    char root[CLI_PATH_MAX_LEN];
    memor.cpy(
        &(MemoriaCfg){.dst = root,
                      .src = g_cli_symbols_root,
                      .bytes = cellul.len(&(CatenaFinitaCfg){.src = g_cli_symbols_root, .cap = CLI_CELLS_POOL}) + 1u});
    s_done = 0ull;
    s_total = s_listed_count;
    for (unsigned int at = 0u; at < g_cli_symbol_file_count; at += 1u)
    {
        g_cli_symbol_files[at].seen = 0;
    }
    int stopped = 0;
    unsigned long long offset = 0ull;
    for (unsigned int each = 0u; each < s_listed_count; each += 1u)
    {
        char path[CLI_PATH_MAX_LEN];
        memor.cpy(&(MemoriaCfg){
            .dst = path,
            .src = reading->listed + offset,
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = reading->listed + offset, .cap = CLI_CELLS_POOL}) + 1u});
        offset += cellul.len(&(CatenaFinitaCfg){.src = reading->listed + offset, .cap = CLI_CELLS_POOL}) + 1ull;
        if (!cli_reading_one(reading, root, path, generation))
        {
            stopped = 1;
            break;
        }
    }
    for (unsigned int at = 0u; !stopped && (at < reading->mounts); at += 1u)
    {
        CliMountReading mount = {reading, root, reading->mount_name[at], generation, 0};
        cli_lock_give(&g_cli_symbols_lock);
        if (query_is_file(reading->mount_path[at]))
        {
            char named[CLI_PATH_MAX_LEN];
            VerbaLinea line = {named, sizeof(named), 0ull};
            mmgr_line_ch(&line, '@');
            mmgr_line_put(&line, mount.name);
            cli_lock_take(&g_cli_symbols_lock);
            mount.stopped = (mmgr_line_finish(&line) != 0ull) && cli_symbols_reads_in(reading->languages, named) &&
                            !cli_reading_one(reading, root, named, generation);
            cli_lock_give(&g_cli_symbols_lock);
        }
        else
        {
            cli_files_walked(reading->mount_path[at], cli_reading_mounted, &mount);
        }
        cli_lock_take(&g_cli_symbols_lock);
        stopped = mount.stopped || s_stop || (s_generation != generation);
    }
    if (s_generation == generation)
    {
        // A reading stopped retains every file it has read, and those it had not come to
        for (unsigned int at = 0u; !stopped && (at < g_cli_symbol_file_count); at += 1u)
        {
            if (g_cli_symbol_files[at].used && !g_cli_symbol_files[at].seen)
            {
                cli_symbols_let_go(at);
            }
        }
        g_cli_symbols_reading = 0;
        s_read = 1;
        s_read_at = cli_clock_milliseconds();
    }
    g_cli_cells->persistent_buf_release(reading->read_text, NULL);
    g_cli_cells->persistent_buf_release(reading->read_symbols, NULL);
    g_cli_cells->persistent_buf_release(reading->file_bytes, NULL);
    cli_symbols_languages_given(reading->languages);
    g_cli_cells->persistent_buf_release(reading, NULL);
    s_listing = NULL;
    s_running = 0;
    cli_wake_all(&s_wake);
    cli_lock_give(&g_cli_symbols_lock);
}

void cli_symbols_forget(void)
{
    cli_lock_take(&g_cli_symbols_lock);
    s_generation += 1ull;
    s_rooted = 0;
    g_cli_symbols_reading = 0;
    s_read = 0;
    s_stop = 0;
    cli_symbols_tree_cleared();
    cli_lock_give(&g_cli_symbols_lock);
}

// One include folder or file the compilation database names, joined to the tree at the top `context` is: mounted
// beside it where it lies outside it
static void cli_symbols_include_joined(void *context, const char *path, unsigned int kind)
{
    (void)kind;
    const char *const root = (const char *)context;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL});
    int inside = 1;
    for (unsigned long long at = 0ull; inside && (at < length); at += 1ull)
    {
        const char one = (root[at] == '\\') ? '/' : root[at];
        const char other = (path[at] == '\\') ? '/' : path[at];
        inside = one == other;
    }
    inside = inside && ((path[length] == '/') || (path[length] == '\\') || (path[length] == '\0'));
    char name[CLI_NAME_MAX_LEN];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (!inside)
    {
        (void)cli_mounted(path, name, &err_msg);
    }
}

void cli_symbols_refresh(const char *root, const char *const *paths, unsigned int count)
{
    cli_lock_take(&g_cli_symbols_lock);
    if (!s_rooted || (!cellul.eq(&(CatenaFinitaCfg){.src = g_cli_symbols_root, .other = root, .cap = CLI_CELLS_POOL})))
    {
        s_generation += 1ull;
        memor.cpy(&(MemoriaCfg){.dst = g_cli_symbols_root,
                                .src = root,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
        s_rooted = 1;
        g_cli_symbols_reading = 0;
        s_read = 0;
        s_stop = 0;
        cli_symbols_tree_cleared();
    }
    if (g_cli_symbols_reading || (s_read && ((cli_clock_milliseconds() - s_read_at) < CLI_FRESH_FOR)))
    {
        cli_lock_give(&g_cli_symbols_lock);
        return;
    }
    g_cli_symbols_reading = 1;
    s_stop = 0;
    // the list the last reading went through is let go only once it has ended
    while (s_running)
    {
        cli_wake_wait(&s_wake, &g_cli_symbols_lock);
    }
    // the languages the files are read in, then the paths read, their bytes counted first to size the reading's cell
    CliSymbolsLanguages *const languages = cli_symbols_languages_read();
    unsigned long long listed = 0ull;
    for (unsigned int at = 0u; (languages != NULL) && (at < count); at += 1u)
    {
        const unsigned long long length =
            cellul.len(&(CatenaFinitaCfg){.src = paths[at], .cap = CLI_CELLS_POOL}) + 1ull;
        listed += (cli_symbols_reads_in(languages, paths[at]) && ((listed + length) <= CLI_SYMBOL_TEXT) &&
                   (length < CLI_PATH_MAX_LEN))
                      ? length
                      : 0ull;
    }
    s_listing = (languages != NULL) ? g_cli_cells->persistent_buf_alloc(sizeof(*s_listing) + listed, NULL) : NULL;
    if (s_listing == NULL)
    {
        cli_symbols_languages_given(languages);
        g_cli_symbols_reading = 0;
        cli_lock_give(&g_cli_symbols_lock);
        return;
    }
    s_listing->languages = languages;
    // the database's include folders and files joined to the tree, then every mount beside it read with it
    cli_compile_db_includes_all(root, cli_symbols_include_joined, (void *)root);
    const char *mount_names[CLI_MOUNTS_MOST];
    const char *mount_paths[CLI_MOUNTS_MOST];
    s_listing->mounts = cli_mounts(mount_names, mount_paths, CLI_MOUNTS_MOST);
    for (unsigned int at = 0u; at < s_listing->mounts; at += 1u)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_listing->mount_name[at], .src = mount_names[at], .bytes = CLI_NAME_MAX_LEN});
        memor.cpy(&(MemoriaCfg){.dst = s_listing->mount_path[at], .src = mount_paths[at], .bytes = CLI_PATH_MAX_LEN});
    }
    s_listing->read_text = NULL;
    s_listing->text_room = 0u;
    s_listing->read_symbols = NULL;
    s_listing->symbol_room = 0u;
    s_listing->kinds = 0u;
    s_listing->file_bytes = NULL;
    s_listing->file_room = 0ull;
    unsigned long long offset = 0ull;
    s_listed_count = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned long long length =
            cellul.len(&(CatenaFinitaCfg){.src = paths[at], .cap = CLI_CELLS_POOL}) + 1ull;
        if (cli_symbols_reads_in(languages, paths[at]) && ((offset + length) <= listed) && (length < CLI_PATH_MAX_LEN))
        {
            memor.cpy(&(MemoriaCfg){.dst = s_listing->listed + offset, .src = paths[at], .bytes = length});
            offset += length;
            s_listed_count += 1u;
        }
    }
    s_thread.run = cli_reading;
    s_thread.context = s_listing;
    s_running = cli_thread_started(&s_thread);
    if (!s_running)
    {
        cli_symbols_languages_given(s_listing->languages);
        g_cli_cells->persistent_buf_release(s_listing, NULL);
        s_listing = NULL;
    }
    g_cli_symbols_reading = s_running;
    cli_lock_give(&g_cli_symbols_lock);
}

int cli_symbols_progress(unsigned long long *done, unsigned long long *total)
{
    cli_lock_take(&g_cli_symbols_lock);
    const int going = g_cli_symbols_reading && !s_read;
    *done = s_done;
    *total = s_total;
    cli_lock_give(&g_cli_symbols_lock);
    return going;
}

void cli_symbols_stop(void)
{
    cli_lock_take(&g_cli_symbols_lock);
    s_stop = 1;
    cli_lock_give(&g_cli_symbols_lock);
}

int cli_symbols_unread(void)
{
    cli_lock_take(&g_cli_symbols_lock);
    const int unread = g_cli_symbols_reading && !s_read;
    cli_lock_give(&g_cli_symbols_lock);
    return unread;
}
