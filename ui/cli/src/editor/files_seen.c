// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_seen.c: what the tree lists off its trunk, git's where git can read it, every file it tracks or would track,
// or else every file walked from the top; and the tree's files laid flat for the length of one call, sorted, in the
// temporary tier
#include "../code/symbols.h"
#include "../command_line/cells.h"
#include "../machine/trust.h"
#include "../text/text.h"
#include "files_internal.h"

// the most folders still to look in on a walk
#define CLI_WALK_PENDING 4096u

// the folders a walk does not go into where git cannot determine: version control's own, and the one cargo writes the
// app's build to
static const char *const s_skipped[2] = {".git", "target"};

// A walk of the tree's folders: those still to look in, each a cell containing its path, the folder looked in, and
// where each file found goes
typedef struct
{
    char **pending;
    unsigned int pending_count;
    const char *root;
    const char *folder;
    void (*each)(void *context, const char *path, unsigned long long length);
    void *context;
} CliWalk;

// `path` copied into a cell of its own, put among the folders still to look in
static void walk_pending(CliWalk *walk, const char *path)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    char *const cell = (walk->pending_count < CLI_WALK_PENDING) ? cli_files_work_taken(length + 1ull) : NULL;
    if (cell != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = cell, .src = path, .bytes = (size_t)length + 1u});
        walk->pending[walk->pending_count] = cell;
        walk->pending_count += 1u;
    }
}

static void walk_each(void *context, const char *name, int is_folder)
{
    CliWalk *const walk = (CliWalk *)context;
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ull};
    mmgr_line_put(&line, walk->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, name);
    if (mmgr_line_finish(&line) == 0ull)
    {
        return;
    }
    if (is_folder)
    {
        const int skipped =
            (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = s_skipped[0], .cap = CLI_CELLS_POOL})) ||
            (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = s_skipped[1], .cap = CLI_CELLS_POOL}));
        if (!skipped)
        {
            walk_pending(walk, path);
        }
        return;
    }
    char relative[CLI_PATH_MAX_LEN];
    cli_relative(walk->root, path, relative);
    walk->each(walk->context, relative, cellul.len(&(CatenaFinitaCfg){.src = relative, .cap = CLI_CELLS_POOL}));
}

// the files under the folder `from` walked, each path from `root` handed to `each`
static void walked_from(const char *root, const char *from, void (*each)(void *context, const char *path, unsigned long long length),
                        void *context)
{
    CliWalk walk = {cli_files_work_taken(CLI_WALK_PENDING * sizeof(char *)), 0u, root, NULL, each, context};
    if (walk.pending == NULL)
    {
        return;
    }
    walk_pending(&walk, from);
    while (walk.pending_count > 0u)
    {
        walk.pending_count -= 1u;
        char *const folder = walk.pending[walk.pending_count];
        walk.folder = folder;
        query_folder_each(folder, walk_each, &walk);
        cli_files_cell_given(folder);
    }
    cli_files_cell_given(walk.pending);
}

void cli_files_walked(const char *root, void (*each)(void *context, const char *path, unsigned long long length),
                      void *context)
{
    walked_from(root, root, each, context);
}

// git's listing of the tree at `root`, or of the folder `folder` under it where one is named, each path handed to
// `each`: 1 where git read it, its whole answer into `*whole`, and for the whole tree where it lists a file. A tree in
// a folder its repository ignores is one git lists nothing of
static int git_listed(const char *root, const char *folder, void (*each)(void *context, const char *path, unsigned long long length),
                      void *context, int *whole)
{
    const char *words[12] = {"git"};
    unsigned int count = 1u + cli_trust_git_words(root, words + 1);
    static const char *const s_args[5] = {"ls-files", "--cached", "--others", "--exclude-standard", "-z"};
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        words[count] = s_args[at];
        count += 1u;
    }
    char spec[CLI_PATH_MAX_LEN + 16u];
    if (folder != NULL)
    {
        VerbaLinea line = {spec, sizeof(spec), 0ull};
        mmgr_line_put(&line, ":(literal)");
        mmgr_line_put(&line, folder);
        mmgr_line_ch(&line, '/');
        (void)mmgr_line_finish(&line);
        words[count] = "--";
        words[count + 1u] = spec;
        count += 2u;
    }
    words[count] = NULL;
    CliRan *const ran = cli_ran_taken();
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ull};
    if (!cli_ran(words, root, NULL, 0u, ran, &unstarted) || (ran->code != 0ll))
    {
        cli_ran_given(ran);
        return 0;
    }
    *whole = ran->whole;
    unsigned long long listed = 0ull;
    const char *const end = ran->out + ran->out_size;
    for (const char *path = ran->out; path < end;)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
        if (length != 0ull)
        {
            each(context, path, length);
            listed += 1ull;
        }
        path += length + 1ull;
    }
    cli_ran_given(ran);
    return (folder != NULL) || (listed != 0ull);
}

int cli_files_listed(const char *root, void (*each)(void *context, const char *path, unsigned long long length),
                     void *context, int *git)
{
    int whole = 1;
    *git = git_listed(root, NULL, each, context, &whole);
    if (!*git)
    {
        whole = 1;
        cli_files_walked(root, each, context);
    }
    return whole;
}

int cli_files_folder_listed(const char *root, const char *folder, int git,
                            void (*each)(void *context, const char *path, unsigned long long length), void *context)
{
    int whole = 1;
    if (git)
    {
        return git_listed(root, folder, each, context, &whole) && whole;
    }
    char from[CLI_PATH_MAX_LEN];
    cli_full(root, folder, from);
    walked_from(root, from, each, context);
    return 1;
}

int cli_files_path_skipped(const char *path, int git)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    for (unsigned long long from = 0ull; from < length;)
    {
        const unsigned long long end = cli_text_to_any(path, from, length, "/");
        const unsigned long long size = end - from;
        for (unsigned int at = git ? 1u : 2u; at > 0u; at -= 1u)
        {
            const char *const skipped = s_skipped[at - 1u];
            if ((cellul.len(&(CatenaFinitaCfg){.src = skipped, .cap = CLI_NAME_MAX_LEN}) == size) &&
                cli_text_same(path + from, skipped, size))
            {
                return 1;
            }
        }
        from = end + 1ull;
    }
    return 0;
}

void cli_files_ignored_read(const char *root, const char *const *paths, unsigned int count, unsigned char *ignored)
{
    unsigned long long bytes = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        bytes += cellul.len(&(CatenaFinitaCfg){.src = paths[at], .cap = CLI_CELLS_POOL}) + 1ull;
    }
    char *const input = cli_files_work_taken(bytes + 1ull);
    if (input == NULL)
    {
        return;
    }
    unsigned long long used = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = paths[at], .cap = CLI_CELLS_POOL}) + 1ull;
        memor.cpy(&(MemoriaCfg){.dst = input + used, .src = paths[at], .bytes = (size_t)length});
        used += length;
    }
    const char *words[8] = {"git"};
    unsigned int word = 1u + cli_trust_git_words(root, words + 1);
    words[word] = "check-ignore";
    words[word + 1u] = "--stdin";
    words[word + 2u] = "-z";
    words[word + 3u] = NULL;
    CliRan *const ran = cli_ran_taken();
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ull};
    if (cli_ran_fed(words, root, NULL, 0u, input, used, ran, &unstarted) && (ran->code == 0ll))
    {
        // git answers in the order it was asked, the paths it ignores alone
        const char *answer = ran->out;
        const char *const end = ran->out + ran->out_size;
        for (unsigned int at = 0u; (at < count) && (answer < end); at += 1u)
        {
            if (cellul.eq(&(CatenaFinitaCfg){.src = answer, .other = paths[at], .cap = CLI_CELLS_POOL}))
            {
                ignored[at] = 1u;
                answer += cellul.len(&(CatenaFinitaCfg){.src = answer, .cap = CLI_CELLS_POOL}) + 1u;
            }
        }
    }
    cli_ran_given(ran);
    cli_files_cell_given(input);
}

const CliFiles *cli_files_view(const char *root)
{
    cli_tree_listed(root);
    unsigned int count = 0u;
    unsigned long long bytes = 0ull;
    cli_tree_files_counted(&count, &bytes);
    CliFiles *const files = cli_files_work_taken(sizeof(*files) + ((unsigned long long)count * sizeof(unsigned int)) + bytes);
    if (files == NULL)
    {
        return NULL;
    }
    // the list's places and text lie in its cell past it, which the cell's alignment and the list's size keep aligned
    unsigned int *const start = (unsigned int *)(void *)(files + 1);
    *files = (CliFiles){start, 0u, count, (char *)(start + count), 0ull, bytes};
    char path[CLI_PATH_MAX_LEN];
    unsigned int at = 0u;
    while (cli_tree_next_file(&at, path))
    {
        (void)cli_files_put(files, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    }
    (void)cli_files_sorted(files);
    return files;
}

void cli_files_let(const CliFiles *files)
{
    // the cell is given back as it was taken; const is the reader's view of it
    cli_files_cell_given((void *)(uintptr_t)files);
}

// where the name of the file at `path`, `length` bytes, starts in it, and the bytes of its ending from its last dot
// into `*ending`, 0 where no dot stands past the name's first letter
static unsigned long long seen_name(const char *path, unsigned long long length, unsigned long long *ending)
{
    unsigned long long name = 0ull;
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        name = ((path[at] == '/') || (path[at] == '\\')) ? (at + 1ull) : name;
    }
    *ending = 0ull;
    for (unsigned long long at = name + 1ull; at < length; at += 1ull)
    {
        *ending = (path[at] == '.') ? (length - at) : *ending;
    }
    return name;
}

const CliFiles *cli_files_kinds_view(const char *root)
{
    const CliFiles *const files = cli_files_view(root);
    if (files == NULL)
    {
        return NULL;
    }
    unsigned long long count = 0ull;
    unsigned long long bytes = 0ull;
    for (unsigned int at = 0u; at < files->count; at += 1u)
    {
        const char *const path = cli_files_at(files, at);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
        unsigned long long ending = 0ull;
        const unsigned long long name = seen_name(path, length, &ending);
        count += (ending != 0ull) ? 2ull : 1ull;
        bytes += (length - name) + 1ull + ((ending != 0ull) ? (ending + 1ull) : 0ull);
    }
    CliFiles *const kinds = cli_files_work_taken(sizeof(*kinds) + (count * sizeof(unsigned int)) + bytes);
    if (kinds == NULL)
    {
        cli_files_let(files);
        return NULL;
    }
    unsigned int *const start = (unsigned int *)(void *)(kinds + 1);
    // two for each file, each file below what an unsigned int counts over two
    *kinds = (CliFiles){start, 0u, (unsigned int)count, (char *)(start + count), 0ull, bytes};
    for (unsigned int at = 0u; at < files->count; at += 1u)
    {
        const char *const path = cli_files_at(files, at);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
        unsigned long long ending = 0ull;
        const unsigned long long name = seen_name(path, length, &ending);
        (void)cli_files_put(kinds, path + name, length - name);
        if (ending != 0ull)
        {
            (void)cli_files_put(kinds, path + length - ending, ending);
        }
    }
    cli_files_let(files);
    (void)cli_files_sorted(kinds);
    return kinds;
}

// the tree listed behind the window, as a tree opens, for the first list or search
static void seen_warmed(void *context)
{
    cli_tree_listed((const char *)context);
}

void cli_files_warm(const char *root)
{
    static char s_root[CLI_PATH_MAX_LEN];
    static CliThread s_warmer;
    static CliLock s_warm_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_warm_lock);
    memor.cpy(&(MemoriaCfg){
        .dst = s_root, .src = root, .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    s_warmer = (CliThread){seen_warmed, s_root, CLI_PRIORITY_IDLE};
    cli_thread_started(&s_warmer);
    cli_lock_give(&s_warm_lock);
}
