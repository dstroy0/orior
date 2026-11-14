// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compile_db_look.c: a look for the tree's compilation database: the files it reads, the look cached for two seconds
// and cached while the files it read are, and the words clangd starts with for the database found
#include "../command_line/pool.h"
#include "../text/text.h"
#include "compile_db_internal.h"

// how long one look for the database is before the builds are looked at again, in milliseconds
#define CLI_COMPILE_LOOKED_FOR 2000ULL

typedef struct
{
    const char *root;
    char *folder;
    CliInputs *inputs;
    const char *path;
    CliInputs *read_state;
    const CliInputs *other;
} CompileDbLookCtx;
// the folder a .clangd at the tree's top names as its CompilationDatabase into `folder`: 1, or 0 where it names none
EMBED_INLINE int cli_clangd_names(const CompileDbLookCtx *args)
{
    const char *root = args->root;
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = ".clangd", .joined = path);
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                  .size_pointer = &size, .owner = g_cli_compile_owner);
    int named = 0;
    for (unsigned long long at = 0ULL; (bytes != NULL) && !named && (at < size);)
    {
        const unsigned long long end =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = (const char *)bytes, .at = at, .size = size).stop;
        const char *from = (const char *)bytes + at;
        const char *to = (const char *)bytes + end;
        at = end + 1ULL;
        while ((from < to) && EMBED_CALL(cli_compile_db_internal.compile_white, CliCompileDbCtx, .character = from[0]))
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
        from += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = from, .at = 0ULL,
                           .size = (unsigned long long)(to - from), .set = CLI_TEXT_WHITE "\"");
        to = from + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = from,
                               .at = (unsigned long long)(to - from), .set = CLI_TEXT_WHITE "\"");
        // the folder lies between its quotes, its end at or past its start
        const unsigned long long length = (unsigned long long)(to - from);
        if ((length > 0ULL) && (length < CLI_PATH_MAX_LEN))
        {
            memor.cpy(&(MemoriaCfg){.dst = args->folder, .src = from, .bytes = length});
            args->folder[length] = '\0';
            named = 1;
        }
    }
    g_cli_guard->persistent_buf_release((void *)(embed_word)bytes, (g_cli_compile_owner));
    return named;
}

// the file at `path` put among what a look reads where it is there: 1, or 0 where it is not or no capacity is given
EMBED_INLINE int cli_input_append(const CompileDbLookCtx *args)
{
    const char *path = args->path;
    if (args->inputs->count >= args->inputs->capacity)
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
    g_cli_guard->persistent_buf_release(args->inputs->file, g_cli_compile_owner);
    *args->inputs = (CliInputs){0, NULL, 0u, 0u};
    args->inputs->file = g_cli_guard->temporary_buf_alloc((unsigned long long)CLI_COMPILE_INPUTS * sizeof(CliStamped),
                                                          g_cli_compile_owner);
    args->inputs->capacity = (args->inputs->file != NULL) ? CLI_COMPILE_INPUTS : 0u;
    char own[3][CLI_PATH_MAX_LEN];
    unsigned int owner = 0u;
    char named[CLI_PATH_MAX_LEN];
    char joined[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_clangd_names, CompileDbLookCtx, .root = args->root, .folder = named))
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = named, .joined = joined);
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = joined, .relative = "compile_commands.json",
                   .joined = own[owner]);
        owner += 1u;
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = "compile_commands.json",
               .joined = own[owner]);
    owner += 1u;
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = "build/compile_commands.json",
               .joined = own[owner]);
    owner += 1u;
    for (unsigned int at = 0u; at < owner; at += 1u)
    {
        if (EMBED_CALL(cli_input_append, CompileDbLookCtx, .inputs = args->inputs, .path = own[at]))
        {
            args->inputs->own = 1;
            return;
        }
    }
    CliFoundFolders found;
    EMBED_CALL(cli_compile_db_found_top_internal.compile_found_alloc, CliCompileDbFoundTopCtx, .found = &found);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = "build/cmake", .joined = joined);
    EMBED_CALL(cli_compile_db_found_internal.compile_folders_by_name, CliCompileDbFoundCtx, .top = joined,
               .tail = s_reply, .count = 4u, .found = &found);
    for (unsigned int at = 0u; at < found.count; at += 1u)
    {
        char index[CLI_PATH_MAX_LEN];
        if (EMBED_CALL(cli_compile_db_reply_internal.compile_newest_index, CliCompileDbReplyCtx,
                       .reply = found.path[at], .index = index))
        {
            (void)EMBED_CALL(cli_input_append, CompileDbLookCtx, .inputs = args->inputs, .path = index);
        }
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = "build/meson", .joined = joined);
    EMBED_CALL(cli_compile_db_found_top_internal.compile_files_by_name, CliCompileDbFoundTopCtx, .top = joined,
               .name = "compile_commands.json", .found = &found);
    for (unsigned int at = 0u; at < found.count; at += 1u)
    {
        (void)EMBED_CALL(cli_input_append, CompileDbLookCtx, .inputs = args->inputs, .path = found.path[at]);
    }
    g_cli_guard->persistent_buf_release(found.path, g_cli_compile_owner);
    *(&found) = (CliFoundFolders){NULL, 0u, 0u};
    // sorted by path, then time
    for (unsigned int at = 1u; (args->inputs->file != NULL) && (at < args->inputs->count); at += 1u)
    {
        const CliStamped pivot = args->inputs->file[at];
        unsigned int place = at;
        while ((place > 0u) &&
               ((EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = args->inputs->file[place - 1u].path,
                            .other_pointer = pivot.path) > 0) ||
                ((cellul.eq(&(CatenaFinitaCfg){
                     .src = args->inputs->file[place - 1u].path, .other = pivot.path, .cap = CLI_POOL_BYTES})) &&
                 (args->inputs->file[place - 1u].time > pivot.time))))
        {
            args->inputs->file[place] = args->inputs->file[place - 1u];
            place -= 1u;
        }
        args->inputs->file[place] = pivot;
    }
}

// The last look: its tree, when, the files it read with their times in a movable cell, a CliStamped a file, how many,
// and what it found
typedef struct
{
    int is_cached;
    char root[CLI_PATH_MAX_LEN];
    unsigned long long at;
    const CarcerMovableEntry *files;
    unsigned int count;
    CliDatabase database;
} CliLookCache;

static CliLookCache s_cache;

// the files a look read saved for the next look in a movable cell of their count, the ones saved before and the
// reading given back
EMBED_INLINE void cli_inputs_save(const CompileDbLookCtx *args)
{
    if (s_cache.files != NULL)
    {
        (void)g_cli_guard->movable_buf_release(s_cache.files, g_cli_compile_owner);
    }
    s_cache.files = NULL;
    s_cache.count = 0u;
    const unsigned long long bytes = (unsigned long long)args->read_state->count * sizeof(CliStamped);
    s_cache.files = (bytes != 0ULL) ? g_cli_guard->movable_buf_alloc(bytes, g_cli_compile_owner) : NULL;
    if (s_cache.files != NULL)
    {
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = s_cache.files, .offset = 0ULL, .src = args->read_state->file, .bytes = bytes});
        s_cache.count = args->read_state->count;
    }
    g_cli_guard->persistent_buf_release(args->read_state->file, g_cli_compile_owner);
    *args->read_state = (CliInputs){0, NULL, 0u, 0u};
}

// whether the look read the same files at the same times as the last look saved
EMBED_INLINE int cli_same_inputs(const CompileDbLookCtx *args)
{
    const CliInputs *other = args->other;
    if (s_cache.count != other->count)
    {
        return 0;
    }
    for (unsigned int at = 0u; at < s_cache.count; at += 1u)
    {
        CliStamped saved;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_cache.files,
                                                                .offset = (unsigned long long)at * sizeof(saved),
                                                                .dst = &saved,
                                                                .bytes = sizeof(saved)});
        if ((saved.time != other->file[at].time) ||
            (!cellul.eq(&(CatenaFinitaCfg){.src = saved.path, .other = other->file[at].path, .cap = CLI_POOL_BYTES})))
        {
            return 0;
        }
    }
    return 1;
}

int cli_compile_db_refresh(const CliCompileDbRootDatabaseCfg *args)
{
    CliDatabase *database = args->database;
    const int same = s_cache.is_cached &&
                     (cellul.eq(&(CatenaFinitaCfg){.src = s_cache.root, .other = args->root, .cap = CLI_POOL_BYTES}));
    if (same && ((cli_clock_milliseconds() - s_cache.at) < CLI_COMPILE_LOOKED_FOR))
    {
        *database = s_cache.database;
        return 0;
    }
    CliInputs read_state = {0, NULL, 0u, 0u};
    EMBED_CALL(cli_compile_db_inputs_internal.compile_inputs, CliCompileDbInputsCtx, .root = args->root,
               .inputs = &read_state);
    if (same && EMBED_CALL(cli_same_inputs, CompileDbLookCtx, .other = &read_state))
    {
        g_cli_guard->persistent_buf_release(read_state.file, g_cli_compile_owner);
        *(&read_state) = (CliInputs){0, NULL, 0u, 0u};
        s_cache.at = cli_clock_milliseconds();
        *database = s_cache.database;
        return 0;
    }
    EMBED_CALL(cli_compile_db_root_internal.compile_gather, CliCompileDbRootCtx, .root = args->root,
               .inputs_read = &read_state, .database = &s_cache.database);
    // the builds read changed since the last look at this tree: clangd is to read the database again
    s_cache.is_cached = 1;
    memor.cpy(&(MemoriaCfg){.dst = s_cache.root,
                            .src = args->root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_POOL_BYTES}) + 1u});
    s_cache.at = cli_clock_milliseconds();
    // the files this look read are stored in place of the last look's
    EMBED_CALL(cli_inputs_save, CompileDbLookCtx, .read_state = &read_state);
    *database = s_cache.database;
    return same;
}

int cli_compile_db_is_cl(const CliCompileDbCfg *args)
{
    const char *name = args->compiler;
    const unsigned long long name_end =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->compiler,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->compiler, .cap = CLI_POOL_BYTES}), .set = "/\\");
    name = (name_end != 0ULL) ? (args->compiler + name_end) : name;
    const char *const dot =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = name,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}), .byte = '.');
    const unsigned long long stem = ((dot != NULL) && (dot != name))
                                        ? (unsigned long long)(dot - name)
                                        : cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES});
    return (stem == 2ULL) && ((name[0] | 0x20) == 'c') && ((name[1] | 0x20) == 'l');
}

void cli_compile_db_words(const CliCompileDbDatabaseCfg *args)
{
    char word_text[CLI_PATH_MAX_LEN * 4u];
    if (args->database_read->has_folder)
    {
        VerbaLinea line = {word_text, sizeof(word_text), 0ULL};
        mmgr_line_put(&line, "--compile-commands-dir=");
        mmgr_line_put(&line, args->database_read->folder);
        mmgr_line_finish(&line);
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word_text = word_text);
    }
    VerbaLinea line = {word_text, sizeof(word_text), 0ULL};
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
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word_text = word_text);
    }
}
