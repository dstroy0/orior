// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compile_db_look.c: a look for the tree's compilation database: the files it reads, the look cached for two seconds
// and cached while the files it read stand, and the words clangd starts with for the database found
#include "../command_line/cells.h"
#include "../text/text.h"
#include "compile_db_internal.h"

// how long one look for the database stands before the builds are looked at again, in milliseconds
#define CLI_COMPILE_LOOKED_FOR 2000ull

typedef struct
{
    const char *root;
    char *folder;
    CliInputs *inputs;
    const char *path;
    CliInputs *saved;
    CliInputs *reading;
    const CliInputs *one;
    const CliInputs *other;
} CompileDbLookCtx;
// the folder a .clangd at the tree's top names as its CompilationDatabase into `folder`: 1, or 0 where it names none
EMBED_INLINE int cli_clangd_names(const CompileDbLookCtx *args)
{
    const char *root = args->root;
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = ".clangd", .joined = path);
    unsigned long long size = 0ull;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = path,
                                                  .size_pointer = &size, .owner = &g_cli_compile_cells);
    int named = 0;
    for (unsigned long long at = 0ull; (bytes != NULL) && !named && (at < size);)
    {
        const unsigned long long end =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = (const char *)bytes, .at = at, .size = size).stop;
        const char *from = (const char *)bytes + at;
        const char *to = (const char *)bytes + end;
        at = end + 1ull;
        while ((from < to) && EMBED_CALL(cli_compile_db_internal.compile_white, CliCompileDbCtx, .letter = from[0]))
        {
            from += 1;
        }
        if (((to - from) < 20) ||
            (memor.cmp(&(MemoriaCfg){.src = from, .other = "CompilationDatabase:", .bytes = 20u}) != 0))
        {
            continue;
        }
        from += 20;
        const char *const hash =
            memor.chr(&(MemoriaCfg){.src = from, .val = '#', .bytes = (unsigned long long)(to - from)});
        to = (hash != NULL) ? hash : to;
        from += EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = from, .at = 0ull,
                           .size = (unsigned long long)(to - from), .set = CLI_TEXT_WHITE "\"");
        to = from + EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg, .text = from,
                               .at = (unsigned long long)(to - from), .set = CLI_TEXT_WHITE "\"");
        // the folder lies between its quotes, its end at or past its start
        const unsigned long long length = (unsigned long long)(to - from);
        if ((length > 0ull) && (length < CLI_PATH_MAX_LEN))
        {
            memor.cpy(&(MemoriaCfg){.dst = args->folder, .src = from, .bytes = length});
            args->folder[length] = '\0';
            named = 1;
        }
    }
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_compile_cells);
    return named;
}

// the file at `path` put among what a look reads where it is there: 1, or 0 where it is not or no room is given
EMBED_INLINE int cli_input_put(const CompileDbLookCtx *args)
{
    const char *path = args->path;
    if (args->inputs->count >= args->inputs->room)
    {
        return 0;
    }
    const int put = EMBED_CALL(cli_compile_db_path_stamped_internal.compile_stamp, CliCompileDbPathStampedCtx,
                               .path = path, .stamped = &args->inputs->file[args->inputs->count]);
    args->inputs->count += put ? 1u : 0u;
    return put;
}

void cli_compile_inputs(const CliCompileDbInputsCtx *args)
{
    static const char *const s_reply[4] = {".cmake", "api", "v1", "reply"};
    cli_compile_db_inputs_internal.compile_inputs_free(args);
    args->inputs->file = g_cli_cells->temporary_buf_alloc((unsigned long long)CLI_COMPILE_INPUTS * sizeof(CliStamped),
                                                          &g_cli_compile_cells);
    args->inputs->room = (args->inputs->file != NULL) ? CLI_COMPILE_INPUTS : 0u;
    char own[3][CLI_PATH_MAX_LEN];
    unsigned int owns = 0u;
    char named[CLI_PATH_MAX_LEN];
    char joined[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_clangd_names, CompileDbLookCtx, .root = args->root, .folder = named))
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = named, .joined = joined);
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = joined, .relative = "compile_commands.json",
                   .joined = own[owns]);
        owns += 1u;
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = "compile_commands.json",
               .joined = own[owns]);
    owns += 1u;
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = "build/compile_commands.json",
               .joined = own[owns]);
    owns += 1u;
    for (unsigned int at = 0u; at < owns; at += 1u)
    {
        if (EMBED_CALL(cli_input_put, CompileDbLookCtx, .inputs = args->inputs, .path = own[at]))
        {
            args->inputs->own = 1;
            return;
        }
    }
    CliFoundFolders found;
    EMBED_CALL(cli_compile_db_found_top_internal.compile_found_take, CliCompileDbFoundTopCtx, .found = &found);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = "build/cmake", .joined = joined);
    EMBED_CALL(cli_compile_db_found_internal.compile_folders_by_name, CliCompileDbFoundCtx, .top = joined,
               .tail = s_reply, .count = 4u, .found = &found);
    for (unsigned int at = 0u; at < found.count; at += 1u)
    {
        char index[CLI_PATH_MAX_LEN];
        if (EMBED_CALL(cli_compile_db_reply_internal.compile_newest_index, CliCompileDbReplyCtx,
                       .reply = found.path[at], .index = index))
        {
            (void)EMBED_CALL(cli_input_put, CompileDbLookCtx, .inputs = args->inputs, .path = index);
        }
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = "build/meson", .joined = joined);
    EMBED_CALL(cli_compile_db_found_top_internal.compile_files_by_name, CliCompileDbFoundTopCtx, .top = joined,
               .name = "compile_commands.json", .found = &found);
    for (unsigned int at = 0u; at < found.count; at += 1u)
    {
        (void)EMBED_CALL(cli_input_put, CompileDbLookCtx, .inputs = args->inputs, .path = found.path[at]);
    }
    EMBED_CALL(cli_compile_db_found_top_internal.compile_found_free, CliCompileDbFoundTopCtx, .found = &found);
    // sorted by path, then time
    for (unsigned int at = 1u; at < args->inputs->count; at += 1u)
    {
        const CliStamped pivot = args->inputs->file[at];
        unsigned int place = at;
        while ((place > 0u) &&
               ((EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = args->inputs->file[place - 1u].path,
                            .other_pointer = pivot.path) > 0) ||
                ((cellul.eq(&(CatenaFinitaCfg){
                     .src = args->inputs->file[place - 1u].path, .other = pivot.path, .cap = CLI_CELLS_POOL})) &&
                 (args->inputs->file[place - 1u].time > pivot.time))))
        {
            args->inputs->file[place] = args->inputs->file[place - 1u];
            place -= 1u;
        }
        args->inputs->file[place] = pivot;
    }
}

// The last look: its tree, when, the files it read with their times, and what it found
typedef struct
{
    int is_cached;
    char root[CLI_PATH_MAX_LEN];
    unsigned long long at;
    CliInputs read;
    CliDatabase database;
} CliLookCache;

static CliLookCache s_cache;
static CliLock s_cache_lock = CLI_LOCK_INIT;

// the files a look read saved for the next look in a cell of their count, the one saved before and the reading given
// back
EMBED_INLINE void cli_inputs_save(const CompileDbLookCtx *args)
{
    CliInputs *saved = args->saved;
    EMBED_CALL(cli_compile_db_inputs_internal.compile_inputs_free, CliCompileDbInputsCtx, .inputs = saved);
    const unsigned long long bytes = (unsigned long long)args->reading->count * sizeof(CliStamped);
    CliStamped *const file = (bytes != 0ull) ? g_cli_cells->persistent_buf_alloc(bytes, &g_cli_compile_cells) : NULL;
    if (file != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = file, .src = args->reading->file, .bytes = bytes});
    }
    const unsigned int count = (file != NULL) ? args->reading->count : 0u;
    *saved = (CliInputs){args->reading->own, file, count, count};
    EMBED_CALL(cli_compile_db_inputs_internal.compile_inputs_free, CliCompileDbInputsCtx, .inputs = args->reading);
}

// whether two looks read the same files at the same times
EMBED_INLINE int cli_same_inputs(const CompileDbLookCtx *args)
{
    const CliInputs *other = args->other;
    if (args->one->count != other->count)
    {
        return 0;
    }
    for (unsigned int at = 0u; at < args->one->count; at += 1u)
    {
        if ((args->one->file[at].time != other->file[at].time) ||
            (!cellul.eq(&(CatenaFinitaCfg){
                .src = args->one->file[at].path, .other = other->file[at].path, .cap = CLI_CELLS_POOL})))
        {
            return 0;
        }
    }
    return 1;
}

int cli_compile_db_refresh(const CliCompileDbRootDatabaseCfg *args)
{
    CliDatabase *database = args->database;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_cache_lock);
    const int same = s_cache.is_cached &&
                     (cellul.eq(&(CatenaFinitaCfg){.src = s_cache.root, .other = args->root, .cap = CLI_CELLS_POOL}));
    if (same && ((cli_clock_milliseconds() - s_cache.at) < CLI_COMPILE_LOOKED_FOR))
    {
        *database = s_cache.database;
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_cache_lock);
        return 0;
    }
    CliInputs reading = {0, NULL, 0u, 0u};
    EMBED_CALL(cli_compile_db_inputs_internal.compile_inputs, CliCompileDbInputsCtx, .root = args->root,
               .inputs = &reading);
    if (same && EMBED_CALL(cli_same_inputs, CompileDbLookCtx, .one = &s_cache.read, .other = &reading))
    {
        EMBED_CALL(cli_compile_db_inputs_internal.compile_inputs_free, CliCompileDbInputsCtx, .inputs = &reading);
        s_cache.at = cli_clock_milliseconds();
        *database = s_cache.database;
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_cache_lock);
        return 0;
    }
    EMBED_CALL(cli_compile_db_root_internal.compile_gather, CliCompileDbRootCtx, .root = args->root,
               .inputs_read = &reading, .database = &s_cache.database);
    // the builds read changed since the last look at this tree: clangd is to read the database again
    s_cache.is_cached = 1;
    memor.cpy(&(MemoriaCfg){.dst = s_cache.root,
                            .src = args->root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_CELLS_POOL}) + 1u});
    s_cache.at = cli_clock_milliseconds();
    // the files this look read are stored in place of the last look's
    EMBED_CALL(cli_inputs_save, CompileDbLookCtx, .saved = &s_cache.read, .reading = &reading);
    *database = s_cache.database;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_cache_lock);
    return same;
}

int cli_compile_db_is_cl(const CliCompileDbCfg *args)
{
    const char *name = args->compiler;
    const unsigned long long name_past =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->compiler,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->compiler, .cap = CLI_CELLS_POOL}), .set = "/\\");
    name = (name_past != 0ull) ? (args->compiler + name_past) : name;
    const char *const dot =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = name,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), .byte = '.');
    const unsigned long long stem = ((dot != NULL) && (dot != name))
                                        ? (unsigned long long)(dot - name)
                                        : cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    return (stem == 2ull) && ((name[0] | 0x20) == 'c') && ((name[1] | 0x20) == 'l');
}

void cli_compile_db_words(const CliCompileDbDatabaseCfg *args)
{
    char word[CLI_PATH_MAX_LEN * 4u];
    if (args->database_read->has_folder)
    {
        VerbaLinea line = {word, sizeof(word), 0ull};
        mmgr_line_put(&line, "--compile-commands-dir=");
        mmgr_line_put(&line, args->database_read->folder);
        mmgr_line_finish(&line);
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word = word);
    }
    VerbaLinea line = {word, sizeof(word), 0ull};
    mmgr_line_put(&line, "--query-driver=");
    unsigned int drivers = 0u;
    for (unsigned int at = 0u; at < args->database_read->compilers; at += 1u)
    {
        if (!EMBED_CALL(cli_compile_db_table.is_cl, CliCompileDbCfg, .compiler = args->database_read->compiler[at]))
        {
            mmgr_line_put(&line, (drivers != 0u) ? "," : "");
            mmgr_line_put(&line, args->database_read->compiler[at]);
            drivers += 1u;
        }
    }
    mmgr_line_finish(&line);
    if (drivers != 0u)
    {
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word = word);
    }
}
