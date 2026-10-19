// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compile_db.c: a look for the tree's compilation database, kept for two seconds and kept while the files it read
// stand, and the words clangd starts with for it
#include "../text/text.h"
#include "compile_db_internal.h"

// how long one look for the database stands before the builds are looked at again, in milliseconds; and the most
// folders a search below a build holds to look in
#define CLI_COMPILE_LOOKED_FOR 2000ull
#define CLI_COMPILE_PENDING 4096u

int cli_compile_stamped(const char *path, CliStamped *stamped)
{
    unsigned long long size = 0ull;
    if (!query_is_file(path) || !cli_file_stamp(path, &size, &stamped->time))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = stamped->path,
                            .src = path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    return 1;
}

// A search below a build: the folders still to look in with their depths, the folder looked in and its depth
typedef struct
{
    char pending[CLI_COMPILE_PENDING][CLI_PATH_LONGEST];
    unsigned int depth[CLI_COMPILE_PENDING];
    unsigned int count;
    const char *folder;
    unsigned int folder_depth;
} CliBelow;

static CliBelow s_below;
static CliLock s_below_lock = CLI_LOCK_INIT;

static void cli_below_each(void *context, const char *name, int is_folder)
{
    (void)context;
    if (is_folder && (name[0] != '.') && (s_below.count < CLI_COMPILE_PENDING))
    {
        cli_full(s_below.folder, name, s_below.pending[s_below.count]);
        s_below.depth[s_below.count] = s_below.folder_depth + 1u;
        s_below.count += 1u;
    }
}

// the folders below `top` walked, each handed to `each` with its depth: whether to look below it
static void cli_walked_below(const char *top, int (*each)(void *context, const char *folder, unsigned int depth),
                             void *context)
{
    cli_lock_take(&s_below_lock);
    memor.cpy(&(MemoriaCfg){.dst = s_below.pending[0],
                            .src = top,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = top, .cap = CLI_CELLS_POOL}) + 1u});
    s_below.depth[0] = 0u;
    s_below.count = 1u;
    while (s_below.count > 0u)
    {
        s_below.count -= 1u;
        char folder[CLI_PATH_LONGEST];
        memor.cpy(&(MemoriaCfg){
            .dst = folder,
            .src = s_below.pending[s_below.count],
            .bytes =
                cellul.len(&(CatenaFinitaCfg){.src = s_below.pending[s_below.count], .cap = CLI_CELLS_POOL}) + 1u});
        const unsigned int depth = s_below.depth[s_below.count];
        if (!each(context, folder, depth) || (depth >= CLI_COMPILE_DEPTH))
        {
            continue;
        }
        s_below.folder = folder;
        s_below.folder_depth = depth;
        query_folder_each(folder, cli_below_each, NULL);
    }
    cli_lock_give(&s_below_lock);
}

// A search for folders ending in a tail, or for files of a name
typedef struct
{
    const char *const *tail;
    unsigned int count;
    const char *name;
    CliFoundFolders *found;
} CliNamedSearch;

static void cli_found_put(CliFoundFolders *found, const char *path)
{
    if (found->count < CLI_COMPILE_FOUND)
    {
        memor.cpy(&(MemoriaCfg){.dst = found->path[found->count],
                                .src = path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
        found->count += 1u;
    }
}

static int cli_tail_each(void *context, const char *folder, unsigned int depth)
{
    (void)depth;
    const CliNamedSearch *const search = (const CliNamedSearch *)context;
    char below[CLI_PATH_LONGEST];
    char next[CLI_PATH_LONGEST];
    memor.cpy(&(MemoriaCfg){.dst = below,
                            .src = folder,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1u});
    for (unsigned int at = 0u; at < search->count; at += 1u)
    {
        cli_full(below, search->tail[at], next);
        memor.cpy(&(MemoriaCfg){.dst = below,
                                .src = next,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = next, .cap = CLI_CELLS_POOL}) + 1u});
    }
    if (query_is_folder(below))
    {
        cli_found_put(search->found, below);
        return 0;
    }
    return 1;
}

void cli_compile_folders_named(const char *top, const char *const *tail, unsigned int count, CliFoundFolders *found)
{
    found->count = 0u;
    CliNamedSearch search = {tail, count, NULL, found};
    cli_walked_below(top, cli_tail_each, &search);
}

static int cli_name_each(void *context, const char *folder, unsigned int depth)
{
    (void)depth;
    const CliNamedSearch *const search = (const CliNamedSearch *)context;
    char file[CLI_PATH_LONGEST];
    cli_full(folder, search->name, file);
    if (query_is_file(file))
    {
        cli_found_put(search->found, file);
    }
    return 1;
}

void cli_compile_files_named(const char *top, const char *name, CliFoundFolders *found)
{
    found->count = 0u;
    CliNamedSearch search = {NULL, 0u, name, found};
    cli_walked_below(top, cli_name_each, &search);
}

// The newest index file of a reply folder as its entries are read
typedef struct
{
    const char *reply;
    char *index;
    int found;
} CliNewest;

static void cli_index_each(void *context, const char *name, int is_folder)
{
    CliNewest *const newest = (CliNewest *)context;
    char path[CLI_PATH_LONGEST];
    cli_full(newest->reply, name, path);
    if (is_folder || (!cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "index-", .cap = CLI_CELLS_POOL})) ||
        !query_is_file(path))
    {
        return;
    }
    if (!newest->found || (cli_text_order(path, newest->index) > 0))
    {
        memor.cpy(&(MemoriaCfg){.dst = newest->index,
                                .src = path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
        newest->found = 1;
    }
}

int cli_compile_newest_index(const char *reply, char *index)
{
    CliNewest newest = {reply, index, 0};
    query_folder_each(reply, cli_index_each, &newest);
    return newest.found;
}

// whether a letter is whitespace as a command line is read
static int cli_white(char letter)
{
    return (letter == ' ') || ((letter >= '\t') && (letter <= '\r'));
}

void cli_compile_words_each(const char *text, unsigned long long length,
                            void (*each)(void *context, const char *word, unsigned long long size), void *context)
{
    static char s_word[1u << 16u];
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    unsigned long long size = 0ull;
    int quoted = 0;
    int any = 0;
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        const char letter = text[at];
        if (letter == '"')
        {
            quoted = !quoted;
            any = 1;
        }
        else if (cli_white(letter) && !quoted)
        {
            if (any)
            {
                each(context, s_word, size);
                size = 0ull;
                any = 0;
            }
        }
        else
        {
            if (size < (sizeof(s_word) - 1ull))
            {
                s_word[size] = letter;
                size += 1ull;
            }
            any = 1;
        }
    }
    if (any)
    {
        each(context, s_word, size);
    }
    cli_lock_give(&s_lock);
}

// The first word of a command line as the words are read
typedef struct
{
    char *first;
    int found;
} CliFirst;

static void cli_first_each(void *context, const char *word, unsigned long long size)
{
    CliFirst *const first = (CliFirst *)context;
    if (!first->found && (size < CLI_PATH_LONGEST))
    {
        memor.cpy(&(MemoriaCfg){.dst = first->first, .src = word, .bytes = size});
        first->first[size] = '\0';
        first->found = 1;
    }
}

int cli_compile_first_word(const char *text, unsigned long long length, char *first)
{
    CliFirst found = {first, 0};
    cli_compile_words_each(text, length, cli_first_each, &found);
    return found.found;
}

unsigned int cli_compile_json_read(CliJson *json, const char *path, CfgJsonToken *tokens, unsigned int tokens_most)
{
    unsigned long long size = 0ull;
    const unsigned char *const bytes = qry_member_file_mapped(path, &size);
    if (bytes == NULL)
    {
        return CLI_JSON_NONE;
    }
    char why[CLI_NAME_LONGEST];
    CliLine unread = {why, sizeof(why), 0ull};
    const unsigned int root = cli_json_read(json, (const char *)bytes, size, tokens, tokens_most, &unread);
    qry_member_file_unmapped(bytes, size);
    return root;
}

// the folder a .clangd at the tree's top names as its CompilationDatabase into `folder`: 1, or 0 where it names none
static int cli_clangd_names(const char *root, char *folder)
{
    char path[CLI_PATH_LONGEST];
    cli_full(root, ".clangd", path);
    unsigned long long size = 0ull;
    const unsigned char *const bytes = qry_member_file_mapped(path, &size);
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
        while ((from < to) && cli_white(from[0]))
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
        while ((from < to) && (cli_white(from[0]) || (from[0] == '"')))
        {
            from += 1;
        }
        while ((to > from) && (cli_white(to[-1]) || (to[-1] == '"')))
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
        qry_member_file_unmapped(bytes, size);
    }
    return named;
}

void cli_compile_inputs(const char *root, CliInputs *inputs)
{
    static CliFoundFolders s_found;
    static const char *const s_reply[4] = {".cmake", "api", "v1", "reply"};
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
        if (cli_compile_stamped(own[at], &inputs->file[0]))
        {
            inputs->own = 1;
            inputs->count = 1u;
            return;
        }
    }
    cli_full(root, "build/cmake", joined);
    cli_compile_folders_named(joined, s_reply, 4u, &s_found);
    for (unsigned int at = 0u; (at < s_found.count) && (inputs->count < CLI_COMPILE_INPUTS); at += 1u)
    {
        char index[CLI_PATH_LONGEST];
        if (cli_compile_newest_index(s_found.path[at], index) &&
            cli_compile_stamped(index, &inputs->file[inputs->count]))
        {
            inputs->count += 1u;
        }
    }
    cli_full(root, "build/meson", joined);
    cli_compile_files_named(joined, "compile_commands.json", &s_found);
    for (unsigned int at = 0u; (at < s_found.count) && (inputs->count < CLI_COMPILE_INPUTS); at += 1u)
    {
        inputs->count += cli_compile_stamped(s_found.path[at], &inputs->file[inputs->count]) ? 1u : 0u;
    }
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
    CliInputs reading;
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
    cli_compile_inputs(root, &s_kept.reading);
    if (same && cli_same_inputs(&s_kept.read, &s_kept.reading))
    {
        s_kept.at = cli_clock_milliseconds();
        *database = s_kept.database;
        cli_lock_give(&s_kept_lock);
        return 0;
    }
    cli_compile_gather(root, &s_kept.reading, &s_kept.database);
    // the builds read changed since the last look at this tree: clangd is to read the database again
    s_kept.kept = 1;
    memor.cpy(&(MemoriaCfg){.dst = s_kept.root,
                            .src = root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    s_kept.at = cli_clock_milliseconds();
    s_kept.read = s_kept.reading;
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
