// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compile_db_look.c: a look for the tree's compilation database: the files it reads, the look kept for two seconds and
// kept while the files it read stand, and the words clangd starts with for the database found
#include "../text/text.h"
#include "compile_db_internal.h"

// how long one look for the database stands before the builds are looked at again, in milliseconds
#define CLI_COMPILE_LOOKED_FOR 2000ull

// the folder a .clangd at the tree's top names as its CompilationDatabase into `folder`: 1, or 0 where it names none
static int cli_clangd_names(const char *root, char *folder)
{
    char path[CLI_PATH_LONGEST];
    cli_full(root, ".clangd", path);
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_held(path, &size);
    int named = 0;
    for (unsigned long long at = 0ull; (bytes != NULL) && !named && (at < size);)
    {
        const unsigned char *const newline =
            memor.chr(&(MemoriaCfg){.src = bytes + at, .val = '\n', .bytes = size - at});
        // a newline found lies in the bytes, at or past `at`
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - bytes) : size;
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
        while ((from < to) && (cli_compile_white(from[0]) || (from[0] == '"')))
        {
            from += 1;
        }
        while ((to > from) && (cli_compile_white(to[-1]) || (to[-1] == '"')))
        {
            to -= 1;
        }
        // the folder lies between its quotes, its end at or past its start
        const unsigned long long length = (unsigned long long)(to - from);
        if ((length > 0ull) && (length < CLI_PATH_LONGEST))
        {
            memor.cpy(&(MemoriaCfg){.dst = folder, .src = from, .bytes = length});
            folder[length] = '\0';
            named = 1;
        }
    }
    if (bytes != NULL)
    {
        cli_file_let_go(bytes, size);
    }
    return named;
}

// the file at `path` put among what a look reads where it is there: 1, or 0 where it is not or no room is given
static int cli_input_put(CliInputs *inputs, const char *path)
{
    CliStamped *const files = cli_compile_room(inputs->file, inputs->count, &inputs->room, sizeof(CliStamped),
                                               CLI_COMPILE_INPUTS, "the files a look reads");
    if (files == NULL)
    {
        return 0;
    }
    inputs->file = files;
    const int put = cli_compile_stamped(path, &inputs->file[inputs->count]);
    inputs->count += put ? 1u : 0u;
    return put;
}

void cli_compile_inputs(const char *root, CliInputs *inputs)
{
    static const char *const s_reply[4] = {".cmake", "api", "v1", "reply"};
    CliFoundFolders found = {NULL, 0u, 0u};
    inputs->own = 0;
    inputs->count = 0u;
    char own[3][CLI_PATH_LONGEST];
    unsigned int owns = 0u;
    char named[CLI_PATH_LONGEST];
    char joined[CLI_PATH_LONGEST];
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
    cli_full(root, "build/cmake", joined);
    cli_compile_folders_named(joined, s_reply, 4u, &found);
    for (unsigned int at = 0u; at < found.count; at += 1u)
    {
        char index[CLI_PATH_LONGEST];
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
        const CliStamped held = inputs->file[at];
        unsigned int place = at;
        while ((place > 0u) &&
               ((cli_text_order(inputs->file[place - 1u].path, held.path) > 0) ||
                ((cellul.eq(&(CatenaFinitaCfg){
                     .src = inputs->file[place - 1u].path, .other = held.path, .cap = CLI_CELLS_POOL})) &&
                 (inputs->file[place - 1u].time > held.time))))
        {
            inputs->file[place] = inputs->file[place - 1u];
            place -= 1u;
        }
        inputs->file[place] = held;
    }
}

// The last look: its tree, when, the files it read with their times, and what it found
typedef struct
{
    int kept;
    char root[CLI_PATH_LONGEST];
    unsigned long long at;
    CliInputs read;
    CliDatabase database;
} CliKeptLook;

static CliKeptLook s_kept;
static CliLock s_kept_lock = CLI_LOCK_INIT;

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
    cli_lock_take(&s_kept_lock);
    const int same =
        s_kept.kept && (cellul.eq(&(CatenaFinitaCfg){.src = s_kept.root, .other = root, .cap = CLI_CELLS_POOL}));
    if (same && ((cli_clock_milliseconds() - s_kept.at) < CLI_COMPILE_LOOKED_FOR))
    {
        *database = s_kept.database;
        cli_lock_give(&s_kept_lock);
        return 0;
    }
    CliInputs reading = {0, NULL, 0u, 0u};
    cli_compile_inputs(root, &reading);
    if (same && cli_same_inputs(&s_kept.read, &reading))
    {
        cli_compile_inputs_given(&reading);
        s_kept.at = cli_clock_milliseconds();
        *database = s_kept.database;
        cli_lock_give(&s_kept_lock);
        return 0;
    }
    cli_compile_gather(root, &reading, &s_kept.database);
    // the builds read changed since the last look at this tree: clangd is to read the database again
    s_kept.kept = 1;
    memor.cpy(&(MemoriaCfg){.dst = s_kept.root,
                            .src = root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    s_kept.at = cli_clock_milliseconds();
    // the files this look read are kept in place of the last look's, whose cell is given back
    cli_compile_inputs_given(&s_kept.read);
    s_kept.read = reading;
    *database = s_kept.database;
    cli_lock_give(&s_kept_lock);
    return same;
}

int cli_compile_db_is_cl(const char *compiler)
{
    const char *name = compiler;
    for (const char *at = compiler; at[0] != '\0'; at += 1)
    {
        name = ((at[0] == '/') || (at[0] == '\\')) ? (at + 1) : name;
    }
    const char *const dot =
        cli_text_last(name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), '.');
    const unsigned long long stem = ((dot != NULL) && (dot != name))
                                        ? (unsigned long long)(dot - name)
                                        : cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    return (stem == 2ull) && ((name[0] | 0x20) == 'c') && ((name[1] | 0x20) == 'l');
}

void cli_compile_db_words(const CliDatabase *database, CliWords *words)
{
    char word[CLI_PATH_LONGEST * 4u];
    if (database->has_folder)
    {
        CliLine line = {word, sizeof(word), 0ull};
        cli_line_text(&line, "--compile-commands-dir=");
        cli_line_text(&line, database->folder);
        cli_line_closed(&line);
        cli_words_put(words, word);
    }
    CliLine line = {word, sizeof(word), 0ull};
    cli_line_text(&line, "--query-driver=");
    unsigned int drivers = 0u;
    for (unsigned int at = 0u; at < database->compilers; at += 1u)
    {
        if (!cli_compile_db_is_cl(database->compiler[at]))
        {
            cli_line_text(&line, (drivers != 0u) ? "," : "");
            cli_line_text(&line, database->compiler[at]);
            drivers += 1u;
        }
    }
    cli_line_closed(&line);
    if (drivers != 0u)
    {
        cli_words_put(words, word);
    }
}
