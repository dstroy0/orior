// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compile_db.c: what a look for the tree's compilation database reads with: a file and when it changed, the folders and
// files of a name below a build, a CMake reply's newest index, a command line's words, and a JSON file read
#include "../text/text.h"
#include "compile_db_internal.h"

// the most folders a search below a build holds to look in
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

CliCellsTally g_cli_compile_cells = CLI_CELLS_TALLY("cli_compile_db");

void *cli_compile_room(void *list, unsigned int count, unsigned int *room, unsigned long long each, unsigned int most,
                       const char *what)
{
    if (count < *room)
    {
        return list;
    }
    const unsigned int doubled = (*room == 0u) ? CLI_COMPILE_FIRST : (*room * 2u);
    const unsigned int grown = (doubled < most) ? doubled : most;
    void *const larger =
        (count < most) ? cli_cell_taken_by(&g_cli_compile_cells, (unsigned long long)grown * each, what) : NULL;
    if (larger == NULL)
    {
        return NULL;
    }
    memor.cpy(&(MemoriaCfg){.dst = larger, .src = list, .bytes = (size_t)(count * each)});
    cli_cell_given_by(&g_cli_compile_cells, list);
    *room = grown;
    return larger;
}

void cli_compile_inputs_given(CliInputs *inputs)
{
    cli_cell_given_by(&g_cli_compile_cells, inputs->file);
    *inputs = (CliInputs){0, NULL, 0u, 0u};
}

void cli_compile_found_given(CliFoundFolders *found)
{
    cli_cell_given_by(&g_cli_compile_cells, found->path);
    *found = (CliFoundFolders){NULL, 0u, 0u};
}

// A folder still to look in below a build, and its depth
typedef struct
{
    char path[CLI_PATH_LONGEST];
    unsigned int depth;
} CliBelowFolder;

// A search below a build: the folders still to look in, in a cell whose room doubles as they are put, and the folder
// looked in and its depth
typedef struct
{
    CliBelowFolder *pending;
    unsigned int count;
    unsigned int room;
    const char *folder;
    unsigned int folder_depth;
} CliBelow;

static void cli_below_each(void *context, const char *name, int is_folder)
{
    CliBelow *const below = context;
    CliBelowFolder *const pending =
        (is_folder && (name[0] != '.'))
            ? cli_compile_room(below->pending, below->count, &below->room, sizeof(CliBelowFolder), CLI_COMPILE_PENDING,
                               "the folders a search below a build has still to look in")
            : NULL;
    if (pending != NULL)
    {
        below->pending = pending;
        cli_full(below->folder, name, below->pending[below->count].path);
        below->pending[below->count].depth = below->folder_depth + 1u;
        below->count += 1u;
    }
}

// the folders below `top` walked, each handed to `each` with its depth: whether to look below it
static void cli_walked_below(const char *top, int (*each)(void *context, const char *folder, unsigned int depth),
                             void *context)
{
    CliBelow below = {NULL, 0u, 0u, NULL, 0u};
    below.pending = cli_compile_room(NULL, 0u, &below.room, sizeof(CliBelowFolder), CLI_COMPILE_PENDING,
                                     "the folders a search below a build has still to look in");
    if (below.pending == NULL)
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = below.pending[0].path,
                            .src = top,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = top, .cap = CLI_CELLS_POOL}) + 1u});
    below.pending[0].depth = 0u;
    below.count = 1u;
    while (below.count > 0u)
    {
        below.count -= 1u;
        char folder[CLI_PATH_LONGEST];
        memor.cpy(&(MemoriaCfg){.dst = folder, .src = below.pending[below.count].path, .bytes = sizeof(folder)});
        const unsigned int depth = below.pending[below.count].depth;
        if (!each(context, folder, depth) || (depth >= CLI_COMPILE_DEPTH))
        {
            continue;
        }
        below.folder = folder;
        below.folder_depth = depth;
        query_folder_each(folder, cli_below_each, &below);
    }
    cli_cell_given_by(&g_cli_compile_cells, below.pending);
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
    char (*const paths)[CLI_PATH_LONGEST] =
        cli_compile_room(found->path, found->count, &found->room, CLI_PATH_LONGEST, CLI_COMPILE_FOUND,
                         "the folders a search below a build found");
    if (paths != NULL)
    {
        found->path = paths;
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

int cli_compile_white(char letter)
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
        else if (cli_compile_white(letter) && !quoted)
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
    const unsigned char *const bytes = cli_file_held(path, &size);
    if (bytes == NULL)
    {
        return CLI_JSON_NONE;
    }
    char why[CLI_NAME_LONGEST];
    CliLine unread = {why, sizeof(why), 0ull};
    const unsigned int root = cli_json_read(json, (const char *)bytes, size, tokens, tokens_most, &unread);
    cli_file_let_go(bytes, size);
    return root;
}
