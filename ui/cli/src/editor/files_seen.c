// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_seen.c: what the tree lists off its trunk, git's where git can read it, every file it tracks or would track,
// or else every file walked from the top; and the tree's files laid flat for the length of one call, sorted, in the
// temporary tier
#include "../code/symbols.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../machine/trust.h"
#include "../text/text.h"
#include "files_internal.h"

// A walk of the tree's folders: those still to look in, each a cell containing its path, the folder looked in, and
// where each file found goes
typedef struct
{
    char **pending;
    unsigned int pending_count;
    const char *root;
    const char *folder;
    void (*each)(const CliEachPathLengthCfg *args);
    void *context;
} CliWalk;
// the most folders still to look in on a walk
#define CLI_WALK_PENDING 4096u

// the folders a walk does not go into where git cannot determine: version control's own, and the one cargo writes the
// app's build to
static const char *const s_skipped[2] = {".git", "target"};

typedef struct
{
    CliWalk *walk;
    const char *path;
    const char *root;
    const char *from;
    void (*each)(const CliEachPathLengthCfg *args);
    void *context;
    const char *folder;
    int *whole;
    unsigned long long length;
    unsigned long long *ending;
} FilesSeenCtx;
// `path` copied into a cell of its own, put among the folders still to look in
EMBED_INLINE void walk_pending(const FilesSeenCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL});
    char *const cell = (args->walk->pending_count < CLI_WALK_PENDING)
                           ? EMBED_CALL(cli_files_bytes_internal.work_alloc, CliFilesBytesCtx, .bytes = length + 1ULL)
                           : NULL;
    if (cell != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = cell, .src = args->path, .bytes = (size_t)length + 1u});
        args->walk->pending[args->walk->pending_count] = cell;
        args->walk->pending_count += 1u;
    }
}

static void walk_each(const CliEachNameIsFolderCfg *args)
{
    CliWalk *const walk = (CliWalk *)args->context;
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ULL};
    mmgr_line_put(&line, walk->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->name);
    if (mmgr_line_finish(&line) == 0ULL)
    {
        return;
    }
    if (args->is_folder)
    {
        const int skipped =
            (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = s_skipped[0], .cap = CLI_CELLS_POOL})) ||
            (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = s_skipped[1], .cap = CLI_CELLS_POOL}));
        if (!skipped)
        {
            EMBED_CALL(walk_pending, FilesSeenCtx, .walk = walk, .path = path);
        }
        return;
    }
    char relative[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = walk->root, .path = path,
               .relative_out = relative);
    EMBED_CALL(walk->each, CliEachPathLengthCfg, .context = walk->context, .path = relative,
               .length = cellul.len(&(CatenaFinitaCfg){.src = relative, .cap = CLI_CELLS_POOL}));
}

// the files under the folder `from` walked, each path from `root` handed to `each`
EMBED_INLINE void walked_from(const FilesSeenCtx *args)
{
    CliWalk walk = {
        EMBED_CALL(cli_files_bytes_internal.work_alloc, CliFilesBytesCtx, .bytes = CLI_WALK_PENDING * sizeof(char *)),
        0u,
        args->root,
        NULL,
        args->each,
        args->context};
    if (walk.pending == NULL)
    {
        return;
    }
    EMBED_CALL(walk_pending, FilesSeenCtx, .walk = &walk, .path = args->from);
    while (walk.pending_count > 0u)
    {
        walk.pending_count -= 1u;
        char *const folder = walk.pending[walk.pending_count];
        walk.folder = folder;
        EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = folder, .each = walk_each,
                   .context_bytess = &walk);
        EMBED_CALL(cli_files_internal.cell_free, CliFilesCtx, .cell = folder);
    }
    EMBED_CALL(cli_files_internal.cell_free, CliFilesCtx, .cell = walk.pending);
}

void cli_files_walk(const CliFilesRootEachContextCfg *args)
{
    const char *root = args->root;
    void (*each)(const CliEachPathLengthCfg *args) = args->each;
    void *context = args->context;
    EMBED_CALL(walked_from, FilesSeenCtx, .root = root, .from = root, .each = each, .context = context);
}

// git's listing of the tree at `root`, or of the folder `folder` under it where one is named, each path handed to
// `each`: 1 where git read it, its whole answer into `*whole`, and for the whole tree where it lists a file. A tree in
// a folder its repository ignores is one git lists nothing of
static int git_list(const FilesSeenCtx *args)
{
    const char *folder = args->folder;
    int *whole = args->whole;
    const char *words[12] = {"git"};
    unsigned int count =
        1u + EMBED_CALL(cli_trust_tree_words.git_words, CliTrustTreeWordsCfg, .tree = args->root, .words = words + 1);
    static const char *const s_args[5] = {"ls-files", "--cached", "--others", "--exclude-standard", "-z"};
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        words[count] = s_args[at];
        count += 1u;
    }
    char spec[CLI_PATH_MAX_LEN + 16u];
    if (folder != NULL)
    {
        VerbaLinea line = {spec, sizeof(spec), 0ULL};
        mmgr_line_put(&line, ":(literal)");
        mmgr_line_put(&line, folder);
        mmgr_line_ch(&line, '/');
        (void)mmgr_line_finish(&line);
        words[count] = "--";
        words[count + 1u] = spec;
        count += 2u;
    }
    words[count] = NULL;
    CliRunResult *const run_result = cli_run_result_alloc();
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ULL};
    if (!EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = args->root, .settings = NULL,
                    .count = 0u, .run_result = run_result, .err_msg = &unstarted) ||
        (run_result->code != 0LL))
    {
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        return 0;
    }
    *whole = run_result->whole;
    unsigned long long list_entries = 0ULL;
    const char *const end = run_result->out + run_result->out_size;
    for (const char *path = run_result->out; path < end;)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
        if (length != 0ULL)
        {
            EMBED_CALL(args->each, CliEachPathLengthCfg, .context = args->context, .path = path, .length = length);
            list_entries += 1ULL;
        }
        path += length + 1ULL;
    }
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    return (folder != NULL) || (list_entries != 0ULL);
}

int cli_files_on_list(const CliFilesRootEachContextCfg *args)
{
    const char *root = args->root;
    void (*each)(const CliEachPathLengthCfg *args) = args->each;
    void *context = args->context;
    int *git = args->git_pointer;
    int whole = 1;
    *git = EMBED_CALL(git_list, FilesSeenCtx, .root = root, .folder = NULL, .each = each, .context = context,
                      .whole = &whole);
    if (!*git)
    {
        whole = 1;
        EMBED_CALL(cli_files_root_each_context.walk, CliFilesRootEachContextCfg, .root = root, .each = each,
                   .context = context);
    }
    return whole;
}

int cli_files_folder_list(const CliFilesRootEachCfg *args)
{
    const char *root = args->root;
    const char *folder = args->folder;
    void (*each)(const CliEachPathLengthCfg *args) = args->each;
    void *context = args->context;
    int whole = 1;
    if (args->git)
    {
        return EMBED_CALL(git_list, FilesSeenCtx, .root = root, .folder = folder, .each = each, .context = context,
                          .whole = &whole) &&
               whole;
    }
    char from[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = folder, .joined = from);
    EMBED_CALL(walked_from, FilesSeenCtx, .root = root, .from = from, .each = each, .context = context);
    return 1;
}

int cli_files_path_skip(const CliFilesGitCfg *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL});
    for (unsigned long long from = 0ULL; from < length;)
    {
        const unsigned long long end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                  .text = args->path, .at = from, .size = length, .set = "/");
        const unsigned long long size = end - from;
        for (unsigned int at = args->git ? 1u : 2u; at > 0u; at -= 1u)
        {
            const char *const skipped = s_skipped[at - 1u];
            if ((cellul.len(&(CatenaFinitaCfg){.src = skipped, .cap = CLI_NAME_MAX_LEN}) == size) &&
                EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->path + from,
                           .other_pointer = skipped, .size = size))
            {
                return 1;
            }
        }
        from = end + 1ULL;
    }
    return 0;
}

void cli_files_ignored_read(const CliFilesRootPathsCfg *args)
{
    unsigned long long bytes = 0ULL;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        bytes += cellul.len(&(CatenaFinitaCfg){.src = args->paths[at], .cap = CLI_CELLS_POOL}) + 1ULL;
    }
    char *const input = EMBED_CALL(cli_files_bytes_internal.work_alloc, CliFilesBytesCtx, .bytes = bytes + 1ULL);
    if (input == NULL)
    {
        return;
    }
    unsigned long long used = 0ULL;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const unsigned long long length =
            cellul.len(&(CatenaFinitaCfg){.src = args->paths[at], .cap = CLI_CELLS_POOL}) + 1ULL;
        memor.cpy(&(MemoriaCfg){.dst = input + used, .src = args->paths[at], .bytes = (size_t)length});
        used += length;
    }
    const char *words[8] = {"git"};
    unsigned int word =
        1u + EMBED_CALL(cli_trust_tree_words.git_words, CliTrustTreeWordsCfg, .tree = args->root, .words = words + 1);
    words[word] = "check-ignore";
    words[word + 1u] = "--stdin";
    words[word + 2u] = "-z";
    words[word + 3u] = NULL;
    CliRunResult *const run_result = cli_run_result_alloc();
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ULL};
    if (EMBED_CALL(cli_words_internal.run_result_feed, CliWordsCtx, .words = words, .folder = args->root,
                   .settings = NULL, .count = 0u, .input = input, .length = used, .run_result = run_result,
                   .err_msg = &unstarted) &&
        (run_result->code == 0LL))
    {
        // git answers in the order it was asked, the paths it ignores alone
        const char *answer = run_result->out;
        const char *const end = run_result->out + run_result->out_size;
        for (unsigned int at = 0u; (at < args->count) && (answer < end); at += 1u)
        {
            if (cellul.eq(&(CatenaFinitaCfg){.src = answer, .other = args->paths[at], .cap = CLI_CELLS_POOL}))
            {
                args->ignored[at] = 1u;
                answer += cellul.len(&(CatenaFinitaCfg){.src = answer, .cap = CLI_CELLS_POOL}) + 1u;
            }
        }
    }
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    EMBED_CALL(cli_files_internal.cell_free, CliFilesCtx, .cell = input);
}

const CliFiles *cli_files_view(const CliFilesRootPathsCfg *args)
{
    const char *root = args->root;
    EMBED_CALL(cli_symbols_context.tree_list, CliSymbolsContextCfg, .root = root);
    unsigned int count = 0u;
    unsigned long long bytes = 0ULL;
    EMBED_CALL(cli_symbols_count.tree_files_count, CliSymbolsCountCfg, .count_pointer = &count, .bytes = &bytes);
    CliFiles *const files =
        EMBED_CALL(cli_files_bytes_internal.work_alloc, CliFilesBytesCtx,
                   .bytes = sizeof(*files) + ((unsigned long long)count * sizeof(unsigned int)) + bytes);
    if (files == NULL)
    {
        return NULL;
    }
    // the list's places and text lie in its cell past it, which the cell's alignment and the list's size save aligned
    unsigned int *const start = (unsigned int *)(void *)(files + 1);
    *files = (CliFiles){start, 0u, count, (char *)(start + count), 0ULL, bytes};
    char path[CLI_PATH_MAX_LEN];
    unsigned int at = 0u;
    while (EMBED_CALL(cli_symbols_at.tree_next_file, CliSymbolsAtCfg, .at = &at, .path_out = path))
    {
        (void)EMBED_CALL(cli_files_path_length_internal.put, CliFilesPathLengthCtx, .files = files, .path = path,
                         .length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    }
    (void)EMBED_CALL(cli_files_bytes_internal.sort, CliFilesBytesCtx, .files = files);
    return files;
}

void cli_files_let(const CliFilesCfg *args)
{
    // the cell is given back as it was taken; const is the reader's view of it
    EMBED_CALL(cli_files_internal.cell_free, CliFilesCtx, .cell = (void *)(uintptr_t)args->files);
}

// where the name of the file at `path`, `length` bytes, starts in it, and the bytes of its ending from its last dot
// into `*ending`, 0 where no dot stands past the name's first letter
EMBED_INLINE unsigned long long seen_name(const FilesSeenCtx *args)
{
    unsigned long long *ending = args->ending;
    unsigned long long name = 0ULL;
    for (unsigned long long at = 0ULL; at < args->length; at += 1ULL)
    {
        name = ((args->path[at] == '/') || (args->path[at] == '\\')) ? (at + 1ULL) : name;
    }
    *ending = 0ULL;
    for (unsigned long long at = name + 1ULL; at < args->length; at += 1ULL)
    {
        *ending = (args->path[at] == '.') ? (args->length - at) : *ending;
    }
    return name;
}

const CliFiles *cli_files_kinds_view(const CliFilesRootPathsCfg *args)
{
    const CliFiles *const files = cli_files_root_paths.view(args);
    if (files == NULL)
    {
        return NULL;
    }
    unsigned long long count = 0ULL;
    unsigned long long bytes = 0ULL;
    for (unsigned int at = 0u; at < files->count; at += 1u)
    {
        const char *const path = EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = at);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
        unsigned long long ending = 0ULL;
        const unsigned long long name =
            EMBED_CALL(seen_name, FilesSeenCtx, .path = path, .length = length, .ending = &ending);
        count += (ending != 0ULL) ? 2ULL : 1ULL;
        bytes += (length - name) + 1ULL + ((ending != 0ULL) ? (ending + 1ULL) : 0ULL);
    }
    CliFiles *const kinds = EMBED_CALL(cli_files_bytes_internal.work_alloc, CliFilesBytesCtx,
                                       .bytes = sizeof(*kinds) + (count * sizeof(unsigned int)) + bytes);
    if (kinds == NULL)
    {
        EMBED_CALL(cli_files.let, CliFilesCfg, .files = files);
        return NULL;
    }
    unsigned int *const start = (unsigned int *)(void *)(kinds + 1);
    // two for each file, each file below what an unsigned int counts over two
    *kinds = (CliFiles){start, 0u, (unsigned int)count, (char *)(start + count), 0ULL, bytes};
    for (unsigned int at = 0u; at < files->count; at += 1u)
    {
        const char *const path = EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = at);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
        unsigned long long ending = 0ULL;
        const unsigned long long name =
            EMBED_CALL(seen_name, FilesSeenCtx, .path = path, .length = length, .ending = &ending);
        (void)EMBED_CALL(cli_files_path_length_internal.put, CliFilesPathLengthCtx, .files = kinds, .path = path + name,
                         .length = length - name);
        if (ending != 0ULL)
        {
            (void)EMBED_CALL(cli_files_path_length_internal.put, CliFilesPathLengthCtx, .files = kinds,
                             .path = path + length - ending, .length = ending);
        }
    }
    EMBED_CALL(cli_files.let, CliFilesCfg, .files = files);
    (void)EMBED_CALL(cli_files_bytes_internal.sort, CliFilesBytesCtx, .files = kinds);
    return kinds;
}

// the tree listed behind the window, as a tree opens, for the first list or search
static void seen_warm(void *context)
{
    EMBED_CALL(cli_symbols_context.tree_list, CliSymbolsContextCfg, .root = (const char *)context);
}

void cli_files_warm(const CliFilesRootPathsCfg *args)
{
    static char s_root[CLI_PATH_MAX_LEN];
    static CliThread s_warmer;
    memor.cpy(&(MemoriaCfg){.dst = s_root,
                            .src = args->root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_CELLS_POOL}) + 1u});
    s_warmer = (CliThread){seen_warm, s_root, CLI_PRIORITY_IDLE};
    EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_warmer);
}
