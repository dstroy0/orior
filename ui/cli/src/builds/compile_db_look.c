// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compile_db_look.c: a look for the tree's compilation database: the files it reads, the look cached for two seconds
// and cached while the files it read stand, and the words clangd starts with for the database found
#include "../command_line/cells.h"
#include "../text/text.h"
#include "compile_db_internal.h"

// how long one look for the database stands before the builds are looked at again, in milliseconds
#define CLI_COMPILE_LOOKED_FOR 2000ull

// the folder a .clangd at the tree's top names as its CompilationDatabase into `folder`: 1, or 0 where it names none
static int cli_clangd_names(const char *root, char *folder)
{
    char path[CLI_PATH_MAX_LEN];
    cli_full(root, ".clangd", path);
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_taken(path, &size, &g_cli_compile_cells);
    int named = 0;
    for (unsigned long long at = 0ull; (bytes != NULL) && !named && (at < size);)
    {
        const unsigned long long end = cli_text_line_at((const char *)bytes, at, size).stop;
        const char *from = (const char *)bytes + at;
        const char *to = (const char *)bytes + end;
        at = end + 1ull;
        while ((from < to) && cli_compile_white(from[0]))
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
        from += cli_text_past_any(from, 0ull, (unsigned long long)(to - from), CLI_TEXT_WHITE "\"");
        to = from + cli_text_back_past(from, (unsigned long long)(to - from), CLI_TEXT_WHITE "\"");
        // the folder lies between its quotes, its end at or past its start
        const unsigned long long length = (unsigned long long)(to - from);
        if ((length > 0ull) && (length < CLI_PATH_MAX_LEN))
        {
            memor.cpy(&(MemoriaCfg){.dst = folder, .src = from, .bytes = length});
            folder[length] = '\0';
            named = 1;
        }
    }
    cli_file_given(bytes, &g_cli_compile_cells);
    return named;
}

// the file at `path` put among what a look reads where it is there: 1, or 0 where it is not or no room is given
static int cli_input_put(CliInputs *inputs, const char *path)
{
    if (inputs->count >= inputs->room)
    {
        return 0;
    }
    const int put = cli_compile_stamped(path, &inputs->file[inputs->count]);
    inputs->count += put ? 1u : 0u;
    return put;
}

void cli_compile_inputs(const char *root, CliInputs *inputs)
{
    static const char *const s_reply[4] = {".cmake", "api", "v1", "reply"};
    cli_compile_inputs_given(inputs);
    inputs->file = g_cli_cells->temporary_buf_alloc((unsigned long long)CLI_COMPILE_INPUTS * sizeof(CliStamped),
                                                    &g_cli_compile_cells);
    inputs->room = (inputs->file != NULL) ? CLI_COMPILE_INPUTS : 0u;
    char own[3][CLI_PATH_MAX_LEN];
    unsigned int owns = 0u;
    char named[CLI_PATH_MAX_LEN];
    char joined[CLI_PATH_MAX_LEN];
    if (cli_clangd_names(root, named))
    {
        cli_full(root, named, joined);
        cli_full(joined, "compile_commands.json", own[owns]);
        owns += 1u;
    }
    cli_full(root, "compile_commands.json", own[owns]);
    owns += 1u;
    cli_full(root, "build/compile_commands.json", own[owns]);
    owns += 1u;
    for (unsigned int at = 0u; at < owns; at += 1u)
    {
        if (cli_input_put(inputs, own[at]))
        {
            inputs->own = 1;
            return;
        }
    }
    CliFoundFolders found;
    cli_compile_found_taken(&found);
    cli_full(root, "build/cmake", joined);
    cli_compile_folders_named(joined, s_reply, 4u, &found);
    for (unsigned int at = 0u; at < found.count; at += 1u)
    {
        char index[CLI_PATH_MAX_LEN];
        if (cli_compile_newest_index(found.path[at], index))
        {
            (void)cli_input_put(inputs, index);
        }
    }
    cli_full(root, "build/meson", joined);
    cli_compile_files_named(joined, "compile_commands.json", &found);
    for (unsigned int at = 0u; at < found.count; at += 1u)
    {
        (void)cli_input_put(inputs, found.path[at]);
    }
    cli_compile_found_given(&found);
    // sorted by path, then time
    for (unsigned int at = 1u; at < inputs->count; at += 1u)
    {
        const CliStamped pivot = inputs->file[at];
        unsigned int place = at;
        while ((place > 0u) &&
               ((cli_text_order(inputs->file[place - 1u].path, pivot.path) > 0) ||
                ((cellul.eq(&(CatenaFinitaCfg){
                     .src = inputs->file[place - 1u].path, .other = pivot.path, .cap = CLI_CELLS_POOL})) &&
                 (inputs->file[place - 1u].time > pivot.time))))
        {
            inputs->file[place] = inputs->file[place - 1u];
            place -= 1u;
        }
        inputs->file[place] = pivot;
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

// the files a look read kept for the next look in a cell of their count, the one kept before and the reading given back
static void cli_inputs_kept(CliInputs *kept, CliInputs *reading)
{
    cli_compile_inputs_given(kept);
    const unsigned long long bytes = (unsigned long long)reading->count * sizeof(CliStamped);
    CliStamped *const file = (bytes != 0ull) ? g_cli_cells->persistent_buf_alloc(bytes, &g_cli_compile_cells) : NULL;
    if (file != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = file, .src = reading->file, .bytes = bytes});
    }
    const unsigned int count = (file != NULL) ? reading->count : 0u;
    *kept = (CliInputs){reading->own, file, count, count};
    cli_compile_inputs_given(reading);
}

// whether two looks read the same files at the same times
static int cli_same_inputs(const CliInputs *one, const CliInputs *other)
{
    if (one->count != other->count)
    {
        return 0;
    }
    for (unsigned int at = 0u; at < one->count; at += 1u)
    {
        if ((one->file[at].time != other->file[at].time) ||
            (!cellul.eq(
                &(CatenaFinitaCfg){.src = one->file[at].path, .other = other->file[at].path, .cap = CLI_CELLS_POOL})))
        {
            return 0;
        }
    }
    return 1;
}

int cli_compile_db_refresh(const char *root, CliDatabase *database)
{
    cli_lock_take(&s_cache_lock);
    const int same =
        s_cache.is_cached && (cellul.eq(&(CatenaFinitaCfg){.src = s_cache.root, .other = root, .cap = CLI_CELLS_POOL}));
    if (same && ((cli_clock_milliseconds() - s_cache.at) < CLI_COMPILE_LOOKED_FOR))
    {
        *database = s_cache.database;
        cli_lock_give(&s_cache_lock);
        return 0;
    }
    CliInputs reading = {0, NULL, 0u, 0u};
    cli_compile_inputs(root, &reading);
    if (same && cli_same_inputs(&s_cache.read, &reading))
    {
        cli_compile_inputs_given(&reading);
        s_cache.at = cli_clock_milliseconds();
        *database = s_cache.database;
        cli_lock_give(&s_cache_lock);
        return 0;
    }
    cli_compile_gather(root, &reading, &s_cache.database);
    // the builds read changed since the last look at this tree: clangd is to read the database again
    s_cache.is_cached = 1;
    memor.cpy(&(MemoriaCfg){.dst = s_cache.root,
                            .src = root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    s_cache.at = cli_clock_milliseconds();
    // the files this look read are stored in place of the last look's
    cli_inputs_kept(&s_cache.read, &reading);
    *database = s_cache.database;
    cli_lock_give(&s_cache_lock);
    return same;
}

int cli_compile_db_is_cl(const char *compiler)
{
    const char *name = compiler;
    const unsigned long long name_past =
        cli_text_back_any(compiler, cellul.len(&(CatenaFinitaCfg){.src = compiler, .cap = CLI_CELLS_POOL}), "/\\");
    name = (name_past != 0ull) ? (compiler + name_past) : name;
    const char *const dot =
        cli_text_last(name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), '.');
    const unsigned long long stem = ((dot != NULL) && (dot != name))
                                        ? (unsigned long long)(dot - name)
                                        : cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    return (stem == 2ull) && ((name[0] | 0x20) == 'c') && ((name[1] | 0x20) == 'l');
}

void cli_compile_db_words(const CliDatabase *database, CliWords *words)
{
    char word[CLI_PATH_MAX_LEN * 4u];
    if (database->has_folder)
    {
        VerbaLinea line = {word, sizeof(word), 0ull};
        mmgr_line_put(&line, "--compile-commands-dir=");
        mmgr_line_put(&line, database->folder);
        mmgr_line_finish(&line);
        cli_words_put(words, word);
    }
    VerbaLinea line = {word, sizeof(word), 0ull};
    mmgr_line_put(&line, "--query-driver=");
    unsigned int drivers = 0u;
    for (unsigned int at = 0u; at < database->compilers; at += 1u)
    {
        if (!cli_compile_db_is_cl(database->compiler[at]))
        {
            mmgr_line_put(&line, (drivers != 0u) ? "," : "");
            mmgr_line_put(&line, database->compiler[at]);
            drivers += 1u;
        }
    }
    mmgr_line_finish(&line);
    if (drivers != 0u)
    {
        cli_words_put(words, word);
    }
}
