// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_checkers.c: the checkers the reader named, by their ids: each file the editor holds checked again with them
// as they are named and the tree where its check is on, a checker no longer named taking its findings with it; one
// file checked with each checker named for its language on a thread of the servers', with its text as the editor holds
// it where the checker reads its input and as the disk holds it where `disk` says the disk has it, a later run of the
// same checker over the same file standing in for an earlier one still going; and each checker that reads a tree whole
// run over the tree
#include "../command_line/cells.h"
#include "../plugins/plugins.h"
#include "../toolchains/checkers.h"
#include "servers_internal.h"

// the most runs counted
#define CLI_SERVERS_RUNS_KEPT 16384u

// A checker's run: the checker, its tree, the file, its key and its run, where what is said goes, whether the text
// follows, and its size
typedef struct
{
    char id[CLI_NAME_LONGEST];
    char root[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    char key[CLI_PATH_LONGEST];
    unsigned long long run;
    CliServersEmit emit;
    int has_text;
    unsigned long long size;
} CliServersCheckerJob;

static CliKeyed s_runs = {"checker runs", {NULL}, 0u, CLI_SERVERS_RUNS_KEPT};
static CliLock s_runs_lock = CLI_LOCK_INIT;

// the checkers named and the languages known, as a job is made, under its lock
static CliLock s_job_lock = CLI_LOCK_INIT;

// the key a run is counted under: the file's key and the checker's id
static void checkers_run_key(const char *key, const char *id, char *joined, unsigned long long room)
{
    CliLine line = {joined, room, 0ull};
    cli_line_text(&line, key);
    cli_line_character(&line, '\n');
    cli_line_text(&line, id);
    (void)cli_line_closed(&line);
}

unsigned long long cli_servers_run_counted(const char *key, const char *id)
{
    char joined[CLI_PATH_LONGEST + CLI_NAME_LONGEST + 2u];
    checkers_run_key(key, id, joined, sizeof(joined));
    cli_lock_take(&s_runs_lock);
    const char *beside = NULL;
    unsigned long long size = 0ull;
    const char *const kept = cli_keyed_get(&s_runs, joined, &beside, &size);
    unsigned long long run = 0ull;
    if ((kept != NULL) && (size == sizeof(run)))
    {
        memor.cpy(&(MemoriaCfg){.dst = &run, .src = kept, .bytes = sizeof(run)});
    }
    run += 1ull;
    (void)cli_keyed_put(&s_runs, joined, "", (const char *)&run, sizeof(run));
    cli_lock_give(&s_runs_lock);
    return run;
}

int cli_servers_run_last(const char *key, const char *id, unsigned long long run)
{
    char joined[CLI_PATH_LONGEST + CLI_NAME_LONGEST + 2u];
    checkers_run_key(key, id, joined, sizeof(joined));
    cli_lock_take(&s_runs_lock);
    const char *beside = NULL;
    unsigned long long size = 0ull;
    const char *const kept = cli_keyed_get(&s_runs, joined, &beside, &size);
    unsigned long long last = 0ull;
    if ((kept != NULL) && (size == sizeof(last)))
    {
        memor.cpy(&(MemoriaCfg){.dst = &last, .src = kept, .bytes = sizeof(last)});
    }
    cli_lock_give(&s_runs_lock);
    return last == run;
}

// one file checked by one checker, its findings written through pieces into a cell of their size and kept where its
// run is still the last
static void checkers_file_job(const void *job, unsigned long long size)
{
    const CliServersCheckerJob *const one = job;
    if (size < sizeof(*one) + (one->has_text ? one->size : 0ull))
    {
        return;
    }
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    CliLine found;
    cli_pieces_begun(&pieces, &found, piece, sizeof(piece));
    char quiet[1024];
    CliLine said = {quiet, sizeof(quiet), 0ull};
    const char *const text = one->has_text ? (const char *)job + sizeof(*one) : NULL;
    const int checked = cli_checkers_check_file(one->id, one->root, one->path, text, one->size, &found, &said);
    unsigned long long found_size = 0ull;
    char *const findings = cli_pieces_joined(&pieces, &found, "a checker's findings", &found_size);
    if (checked && (findings != NULL) && cli_servers_run_last(one->key, one->id, one->run))
    {
        cli_servers_keep_checked(one->id, findings, found_size, 0, &one->emit);
    }
    cli_cell_given(findings);
}

// the tree checked whole by one checker, its findings written through pieces into a cell of their size and kept
static void checkers_tree_job(const void *job, unsigned long long size)
{
    const CliServersCheckerJob *const one = job;
    if (size < sizeof(*one))
    {
        return;
    }
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    CliLine found;
    cli_pieces_begun(&pieces, &found, piece, sizeof(piece));
    char quiet[1024];
    CliLine said = {quiet, sizeof(quiet), 0ull};
    const int checked = cli_checkers_check_tree(one->id, one->root, &found, &said);
    unsigned long long found_size = 0ull;
    char *const findings = cli_pieces_joined(&pieces, &found, "a checker's findings", &found_size);
    if (checked && (findings != NULL))
    {
        cli_servers_keep_checked(one->id, findings, found_size, 0, &one->emit);
    }
    cli_cell_given(findings);
}

// a path copied into what holds CLI_PATH_LONGEST
static void checkers_path_copied(const char *path, char *into)
{
    CliLine line = {into, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, path);
    (void)cli_line_closed(&line);
}

void cli_servers_run_checkers(const char *root, const char *path, const char *text, unsigned long long size, int disk,
                              const CliServersEmit *emit)
{
    static char s_id[CLI_SERVERS_CHECKERS][CLI_NAME_LONGEST];
    static CliLanguages s_languages;
    cli_lock_take(&s_job_lock);
    cli_lock_take(&g_cli_servers.lock);
    const unsigned int ids = g_cli_servers.checkers;
    memor.cpy(&(MemoriaCfg){.dst = s_id, .src = g_cli_servers.checker, .bytes = sizeof(s_id)});
    cli_lock_give(&g_cli_servers.lock);
    char language[CLI_NAME_LONGEST];
    cli_plugins_languages(&s_languages);
    if ((ids == 0u) || !cli_plugins_language_for(&s_languages, path, language))
    {
        cli_lock_give(&s_job_lock);
        return;
    }
    char key[CLI_PATH_LONGEST];
    cli_servers_key_of(path, key);
    for (unsigned int at = 0u; at < ids; at += 1u)
    {
        if (!cli_checkers_checks(s_id[at], language))
        {
            continue;
        }
        const int input = cli_checkers_reads_input(s_id[at]);
        if (!disk && !input)
        {
            continue;
        }
        // the job made in a cell of its size, the text after it where the checker has it
        const int has_text = (text != NULL) && (size <= CLI_SERVERS_LARGEST);
        const unsigned long long text_size = has_text ? size : 0ull;
        char *const job = cli_cell_taken(sizeof(CliServersCheckerJob) + text_size, "a checker's run of a file");
        if (job == NULL)
        {
            continue;
        }
        CliServersCheckerJob *const one = (CliServersCheckerJob *)(void *)job;
        memor.set(&(MemoriaCfg){.dst = one, .val = 0, .bytes = sizeof(*one)});
        memor.cpy(&(MemoriaCfg){.dst = one->id, .src = s_id[at], .bytes = sizeof(one->id)});
        checkers_path_copied(root, one->root);
        checkers_path_copied(path, one->path);
        memor.cpy(&(MemoriaCfg){.dst = one->key, .src = key, .bytes = sizeof(key)});
        one->run = cli_servers_run_counted(key, s_id[at]);
        one->emit = *emit;
        one->has_text = has_text;
        one->size = text_size;
        if (has_text)
        {
            // a size under CLI_SERVERS_LARGEST fits a size_t
            memor.cpy(&(MemoriaCfg){.dst = job + sizeof(*one), .src = text, .bytes = (size_t)text_size});
        }
        (void)cli_servers_later_handed(checkers_file_job, job, sizeof(*one) + text_size);
    }
    cli_lock_give(&s_job_lock);
}

void cli_servers_check_tree_with_checkers(const char *root, const CliServersEmit *emit)
{
    static char s_id[CLI_SERVERS_CHECKERS][CLI_NAME_LONGEST];
    cli_lock_take(&s_job_lock);
    cli_lock_take(&g_cli_servers.lock);
    const unsigned int ids = g_cli_servers.checkers;
    memor.cpy(&(MemoriaCfg){.dst = s_id, .src = g_cli_servers.checker, .bytes = sizeof(s_id)});
    cli_lock_give(&g_cli_servers.lock);
    for (unsigned int at = 0u; at < ids; at += 1u)
    {
        if (!cli_checkers_reads_tree(s_id[at]))
        {
            continue;
        }
        CliServersCheckerJob one;
        memor.set(&(MemoriaCfg){.dst = &one, .val = 0, .bytes = sizeof(one)});
        memor.cpy(&(MemoriaCfg){.dst = one.id, .src = s_id[at], .bytes = sizeof(one.id)});
        checkers_path_copied(root, one.root);
        one.emit = *emit;
        (void)cli_servers_later(checkers_tree_job, &one, sizeof(one));
    }
    cli_lock_give(&s_job_lock);
}
