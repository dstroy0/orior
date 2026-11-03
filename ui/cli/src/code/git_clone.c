// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_clone.c: a folder made a repository, and a repository cloned into a folder of its own, each line of git's
// progress reported as it comes
#include "../command_line/callback_cfg.h"
#include "../machine/net.h"
#include "../text/text.h"
#include "../toolchains/toolchains.h"
#include "git_internal.h"

#define CLI_GIT_PROGRESS 4096u
// A line of git's progress being read, and the first line it marked fatal
typedef struct
{
    char line[CLI_GIT_PROGRESS];
    unsigned long long length;
    char fatal[CLI_GIT_PROGRESS];
    int has_fatal;
    void (*progress_line)(const CliEachLineEndedCfg *args);
    void *context;
} CliProgress;
// the most bytes of the PATH a clone runs with, and of a line of git's progress
#define CLI_GIT_RUN_PATH (1u << 15u)

int cli_git_clone_parent(const CliGitErrMsgFolderCfg *args)
{
#if defined(_WIN32)
    static const char s_home[] = "USERPROFILE";
#else
    static const char s_home[] = "HOME";
#endif
    return EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = s_home,
                      .value_out = args->folder, .capacity = CLI_PATH_MAX_LEN) ||
           EMBED_CALL(cli_path_capacity_internal.folder_current, CliPathCapacityCtx, .path_out = args->folder,
                      .capacity_bytes = CLI_PATH_MAX_LEN);
}

int cli_git_opened_parent(const CliGitErrMsgFolderCfg *args)
{
    char home[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = home))
    {
        return 0;
    }
    VerbaLinea line = {args->folder, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, home);
    mmgr_line_put(&line, "/repositories");
    return mmgr_line_finish(&line) != 0ULL;
}

int cli_git_init(const CliGitOutCfg *args)
{
    static const char *const s_init[1] = {"init"};
    char git[CLI_PATH_MAX_LEN];
    VerbaLinea line = {git, sizeof(git), 0ULL};
    mmgr_line_put(&line, args->folder_read);
    mmgr_line_put(&line, "/.git");
    if ((mmgr_line_finish(&line) != 0ULL) &&
        (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = git) ||
         EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = git)))
    {
        mmgr_line_put(args->err_msg, args->folder_read);
        mmgr_line_put(args->err_msg, " is a repository already");
        return 0;
    }
    return EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = args->folder_read,
                      .request = s_init, .count = 1u, .out = args->out, .err_msg = args->err_msg);
}

int cli_git_clone_folder(const CliGitErrMsgFolderCfg *args)
{
    const char *from = args->url;
    const char *to = args->url + cellul.len(&(CatenaFinitaCfg){.src = args->url, .cap = CLI_CELLS_POOL});
    EMBED_CALL(cli_git_from_internal.trim, CliGitFromCtx, .from = &from, .to = &to);
    to = from + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = from,
                           .at = (unsigned long long)(to - from), .set = "/\\");
    if (((to - from) >= 4) && (memor.cmp(&(MemoriaCfg){.src = to - 4, .other = ".git", .bytes = 4u}) == 0))
    {
        to -= 4;
    }
    const char *name = to;
    name = from + EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = from,
                             .at = (unsigned long long)(name - from), .set = "/\\:");
    const unsigned long long length = (unsigned long long)(to - name);
    if ((length == 0ULL) || ((length == 1ULL) && (name[0] == '.')) ||
        ((length == 2ULL) && (name[0] == '.') && (name[1] == '.')))
    {
        mmgr_line_put(args->err_msg, args->url);
        mmgr_line_put(args->err_msg, " names no repository");
        return 0;
    }
    VerbaLinea line = {args->folder, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->parent);
    mmgr_line_ch(&line, '/');
    mmgr_line_put_n(&line, name, length);
    if (mmgr_line_finish(&line) == 0ULL)
    {
        mmgr_line_put(args->err_msg, args->url);
        mmgr_line_put(args->err_msg, ": the folder's path is too long");
        return 0;
    }
    return 1;
}

// whether a folder contains anything, counted as its entries are handed
static void cli_folder_entry_count(const CliEachNameIsFolderCfg *args)
{
    void *context = args->context;
    *(int *)context = 1;
}

typedef struct
{
    const char *path;
    char *program;
    CliProgress *progress;
    int ended;
} GitCloneCtx;
// git found in the folders of the PATH `path` names, its full path into `program`: 1, or 0 where none contains it
EMBED_INLINE int cli_git_on(const GitCloneCtx *args)
{
    const char *path = args->path;
#if defined(_WIN32)
    static const char s_name[] = "/git.exe";
    const char between = ';';
#else
    static const char s_name[] = "/git";
    const char between = ':';
#endif
    const char *folder = path;
    for (const char *at = path;; at += 1)
    {
        if ((at[0] == between) || (at[0] == '\0'))
        {
            VerbaLinea line = {args->program, CLI_PATH_MAX_LEN, 0ULL};
            mmgr_line_put_n(&line, folder, (unsigned long long)(at - folder));
            mmgr_line_put(&line, s_name);
            if ((at > folder) && (mmgr_line_finish(&line) != 0ULL) &&
                EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = args->program))
            {
                return 1;
            }
            if (at[0] == '\0')
            {
                return 0;
            }
            folder = at + 1;
        }
    }
}

// the line read so far reported, trimmed, with whether it is finished, and let go
EMBED_INLINE void cli_progress_emit(const GitCloneCtx *args)
{
    const char *from = args->progress->line;
    const char *to = args->progress->line + args->progress->length;
    EMBED_CALL(cli_git_from_internal.trim, CliGitFromCtx, .from = &from, .to = &to);
    args->progress->length = 0ULL;
    if (from == to)
    {
        return;
    }
    char text[CLI_GIT_PROGRESS];
    VerbaLinea line = {text, sizeof(text), 0ULL};
    mmgr_line_put_n(&line, from, (unsigned long long)(to - from));
    mmgr_line_finish(&line);
    EMBED_CALL(args->progress->progress_line, CliEachLineEndedCfg, .context = args->progress->context, .line = text,
               .ended = args->ended);
    if (!args->progress->has_fatal &&
        (cellul.starts(&(CatenaFinitaCfg){.src = text, .other = "fatal: ", .cap = CLI_CELLS_POOL})))
    {
        memor.cpy(&(MemoriaCfg){.dst = args->progress->fatal,
                                .src = text + 7,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = text + 7, .cap = CLI_CELLS_POOL}) + 1u});
        args->progress->has_fatal = 1;
    }
}

int cli_git_clone(const CliGitErrMsgCfg *args)
{
    void (*progress_line)(const CliEachLineEndedCfg *args) = args->progress_line;
    void *context = args->context;
    VerbaLinea *err_msg = args->err_msg;
    static char s_run_path[CLI_GIT_RUN_PATH];
    static CliProgress s_progress;
    if (!EMBED_CALL(cli_net_address.reach, CliNetAddressCfg, .address = args->url, .purpose = "git clone",
                    .err_msg = err_msg))
    {
        return 0;
    }
    int has_entries = 0;
    if (EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->target,
                   .each = cli_folder_entry_count, .context_bytess = &has_entries) &&
        has_entries)
    {
        mmgr_line_put(err_msg, args->target);
        mmgr_line_put(err_msg, " is there already and contains files");
        return 0;
    }
    EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = s_run_path,
               .size = sizeof(s_run_path));
    char program[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_toolchains_id_path.chosen_program, CliToolchainsIdPathCfg, .id = "git", .path = program) &&
        !EMBED_CALL(cli_git_on, GitCloneCtx, .path = s_run_path, .program = program))
    {
        mmgr_line_put(err_msg,
                      "git is not installed, or not on the PATH: File, Toolchains, or orior_cli file toolchains "
                      "install git, opens its install page");
        return 0;
    }
    const char *const words[7] = {program, "clone", "--progress", "--", args->url, args->target, NULL};
    // git prompts for no password at a terminal no one can type into; a credential helper still may
    const CliSetting settings[2] = {{"PATH", s_run_path}, {"GIT_TERMINAL_PROMPT", "0"}};
    CliSpawned spawned;
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ULL};
    if (!EMBED_CALL(cli_words_err_msg_internal.spawn_as, CliWordsErrMsgCtx, .priority = CLI_PRIORITY_GUEST,
                    .spawned = &spawned, .words = words, .folder = NULL, .settings = settings, .count = 2u,
                    .detached = 1, .err_msg = &unstarted))
    {
        mmgr_line_put(err_msg, "git did not start: ");
        mmgr_line_put_n(err_msg, why, unstarted.at);
        return 0;
    }
    CliProgress *const progress = &s_progress;
    progress->length = 0ULL;
    progress->has_fatal = 0;
    progress->progress_line = progress_line;
    progress->context = context;
    for (;;)
    {
        char chunk[4096];
        const unsigned long long read =
            EMBED_CALL(cli_capacity_pipe_handle_internal.spawn_read, CliCapacityPipeHandleCtx,
                       .pipe_handle = spawned.err, .piece = chunk, .capacity = sizeof(chunk));
        if (read == 0ULL)
        {
            break;
        }
        for (unsigned long long at = 0ULL; at < read; at += 1ULL)
        {
            if ((chunk[at] == '\n') || (chunk[at] == '\r'))
            {
                EMBED_CALL(cli_progress_emit, GitCloneCtx, .progress = progress, .ended = chunk[at] == '\n');
            }
            else if (progress->length < (CLI_GIT_PROGRESS - 1u))
            {
                progress->line[progress->length] = chunk[at];
                progress->length += 1ULL;
            }
        }
    }
    EMBED_CALL(cli_progress_emit, GitCloneCtx, .progress = progress, .ended = 1);
    // what git writes to its output is read and let go, so it never waits on a full pipe
    for (;;)
    {
        char chunk[4096];
        if (EMBED_CALL(cli_capacity_pipe_handle_internal.spawn_read, CliCapacityPipeHandleCtx,
                       .pipe_handle = spawned.out, .piece = chunk, .capacity = sizeof(chunk)) == 0ULL)
        {
            break;
        }
    }
    const long long code = EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &spawned);
    EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &spawned);
    if (code != 0LL)
    {
        if (progress->has_fatal)
        {
            mmgr_line_put(err_msg, progress->fatal);
        }
        else
        {
            mmgr_line_put(err_msg, "git clone ");
            mmgr_line_put(err_msg, args->url);
            mmgr_line_put(err_msg, " failed");
        }
    }
    return code == 0LL;
}
