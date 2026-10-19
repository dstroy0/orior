// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_clone.c: a folder made a repository, and a repository cloned into a folder of its own, each line of git's
// progress told as it comes
#include "../machine/net.h"
#include "../toolchains/toolchains.h"
#include "git_internal.h"

// the most bytes of the PATH a clone runs with, and of a line of git's progress
#define CLI_GIT_RUN_PATH (1u << 15u)

#define CLI_GIT_PROGRESS 4096u

int cli_git_clone_parent(char *folder)
{
#if defined(_WIN32)
    static const char s_home[] = "USERPROFILE";
#else
    static const char s_home[] = "HOME";
#endif
    return query_environment_read(s_home, folder, CLI_PATH_LONGEST) || cli_folder_current(folder, CLI_PATH_LONGEST);
}

int cli_git_opened_parent(char *folder)
{
    char home[CLI_PATH_LONGEST];
    if (!cli_home_folder(home))
    {
        return 0;
    }
    CliLine line = {folder, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, home);
    cli_line_text(&line, "/repositories");
    return cli_line_closed(&line) != 0ull;
}

int cli_git_init(const char *folder, CliLine *out, CliLine *said)
{
    static const char *const s_init[1] = {"init"};
    char git[CLI_PATH_LONGEST];
    CliLine line = {git, sizeof(git), 0ull};
    cli_line_text(&line, folder);
    cli_line_text(&line, "/.git");
    if ((cli_line_closed(&line) != 0ull) && (query_is_file(git) || query_is_folder(git)))
    {
        cli_line_text(said, folder);
        cli_line_text(said, " is a repository already");
        return 0;
    }
    return cli_git_run(folder, s_init, 1u, out, said);
}

int cli_git_clone_folder(const char *url, const char *parent, char *folder, CliLine *said)
{
    const char *from = url;
    const char *to = url + cellul.len(&(CatenaFinitaCfg){.src = url, .cap = CLI_CELLS_POOL});
    cli_git_trimmed(&from, &to);
    while ((to > from) && ((to[-1] == '/') || (to[-1] == '\\')))
    {
        to -= 1;
    }
    if (((to - from) >= 4) && (memor.cmp(&(MemoriaCfg){.src = to - 4, .other = ".git", .bytes = 4u}) == 0))
    {
        to -= 4;
    }
    const char *name = to;
    while ((name > from) && (name[-1] != '/') && (name[-1] != '\\') && (name[-1] != ':'))
    {
        name -= 1;
    }
    const unsigned long long length = (unsigned long long)(to - name);
    if ((length == 0ull) || ((length == 1ull) && (name[0] == '.')) ||
        ((length == 2ull) && (name[0] == '.') && (name[1] == '.')))
    {
        cli_line_text(said, url);
        cli_line_text(said, " names no repository");
        return 0;
    }
    CliLine line = {folder, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, parent);
    cli_line_character(&line, '/');
    cli_line_counted(&line, name, length);
    if (cli_line_closed(&line) == 0ull)
    {
        cli_line_text(said, url);
        cli_line_text(said, ": the folder's path is too long");
        return 0;
    }
    return 1;
}

// whether a folder holds anything, counted as its entries are handed
static void cli_holds_each(void *context, const char *name, int is_folder)
{
    (void)name;
    (void)is_folder;
    *(int *)context = 1;
}

// git found in the folders of the PATH `path` names, its full path into `program`: 1, or 0 where none holds it
static int cli_git_on(const char *path, char *program)
{
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
            CliLine line = {program, CLI_PATH_LONGEST, 0ull};
            cli_line_counted(&line, folder, (unsigned long long)(at - folder));
            cli_line_text(&line, s_name);
            if ((at > folder) && (cli_line_closed(&line) != 0ull) && query_is_file(program))
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

// A line of git's progress being read, and the first line it marked fatal
typedef struct
{
    char line[CLI_GIT_PROGRESS];
    unsigned long long length;
    char fatal[CLI_GIT_PROGRESS];
    int has_fatal;
    void (*said_line)(void *context, const char *line, int ended);
    void *context;
} CliProgress;

// the line read so far told, trimmed, with whether it is finished, and let go
static void cli_told(CliProgress *progress, int ended)
{
    const char *from = progress->line;
    const char *to = progress->line + progress->length;
    cli_git_trimmed(&from, &to);
    progress->length = 0ull;
    if (from == to)
    {
        return;
    }
    char text[CLI_GIT_PROGRESS];
    CliLine line = {text, sizeof(text), 0ull};
    cli_line_counted(&line, from, (unsigned long long)(to - from));
    cli_line_closed(&line);
    progress->said_line(progress->context, text, ended);
    if (!progress->has_fatal &&
        (cellul.starts(&(CatenaFinitaCfg){.src = text, .other = "fatal: ", .cap = CLI_CELLS_POOL})))
    {
        memor.cpy(&(MemoriaCfg){.dst = progress->fatal,
                                .src = text + 7,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = text + 7, .cap = CLI_CELLS_POOL}) + 1u});
        progress->has_fatal = 1;
    }
}

int cli_git_clone(const char *url, const char *target, void (*said_line)(void *context, const char *line, int ended),
                  void *context, CliLine *said)
{
    static char s_run_path[CLI_GIT_RUN_PATH];
    static CliProgress s_progress;
    static CliLock s_lock = CLI_LOCK_INIT;
    if (!cli_net_reach(url, "git clone", said))
    {
        return 0;
    }
    int holds = 0;
    if (query_folder_each(target, cli_holds_each, &holds) && holds)
    {
        cli_line_text(said, target);
        cli_line_text(said, " is there already and holds files");
        return 0;
    }
    cli_lock_take(&s_lock);
    cli_toolchains_run_path(s_run_path, sizeof(s_run_path));
    char program[CLI_PATH_LONGEST];
    if (!cli_toolchains_chosen_program("git", program) && !cli_git_on(s_run_path, program))
    {
        cli_lock_give(&s_lock);
        cli_line_text(said, "git is not installed, or not on the PATH: File, Toolchains, or orior file toolchains "
                            "install git, opens its install page");
        return 0;
    }
    const char *const words[7] = {program, "clone", "--progress", "--", url, target, NULL};
    // git asks for no password at a terminal no one can type into; a credential helper still may
    const CliSetting settings[2] = {{"PATH", s_run_path}, {"GIT_TERMINAL_PROMPT", "0"}};
    CliSpawned spawned;
    char why[CLI_PATH_LONGEST];
    CliLine unstarted = {why, sizeof(why), 0ull};
    if (!cli_spawn(&spawned, words, NULL, settings, 2u, 1, &unstarted))
    {
        cli_lock_give(&s_lock);
        cli_line_text(said, "git did not start: ");
        cli_line_counted(said, why, unstarted.at);
        return 0;
    }
    CliProgress *const progress = &s_progress;
    progress->length = 0ull;
    progress->has_fatal = 0;
    progress->said_line = said_line;
    progress->context = context;
    for (;;)
    {
        char chunk[4096];
        const unsigned long long read = cli_spawn_read(spawned.err, chunk, sizeof(chunk));
        if (read == 0ull)
        {
            break;
        }
        for (unsigned long long at = 0ull; at < read; at += 1ull)
        {
            if ((chunk[at] == '\n') || (chunk[at] == '\r'))
            {
                cli_told(progress, chunk[at] == '\n');
            }
            else if (progress->length < (CLI_GIT_PROGRESS - 1u))
            {
                progress->line[progress->length] = chunk[at];
                progress->length += 1ull;
            }
        }
    }
    cli_told(progress, 1);
    // what git writes to its output is read and let go, so it never waits on a full pipe
    for (;;)
    {
        char chunk[4096];
        if (cli_spawn_read(spawned.out, chunk, sizeof(chunk)) == 0ull)
        {
            break;
        }
    }
    const long long code = cli_spawn_wait(&spawned);
    cli_spawn_closed(&spawned);
    if (code != 0ll)
    {
        if (progress->has_fatal)
        {
            cli_line_text(said, progress->fatal);
        }
        else
        {
            cli_line_text(said, "git clone ");
            cli_line_text(said, url);
            cli_line_text(said, " failed");
        }
    }
    cli_lock_give(&s_lock);
    return code == 0ll;
}
