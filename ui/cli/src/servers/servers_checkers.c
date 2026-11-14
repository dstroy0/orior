// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_checkers.c: the checkers the reader named, by their ids: each file the editor has open checked again with
// them as they are named and the tree where its check is on, a checker no longer named taking its findings with it; one
// file checked with each checker named for its language on a thread of the servers', with its text as the editor has
// it where the checker reads its input and as the disk stores it where `disk` specifies the disk has it, a later run of
// the same checker over the same file standing in for an earlier one still going; and each checker that reads a tree
// whole run over the tree
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../plugins/plugins.h"
#include "../toolchains/checkers.h"
#include "servers_internal.h"

// the most runs counted
#define CLI_SERVERS_RUNS_MAX 16384u

// A checker's run: the checker, its tree, the file, its key and its run, where what is reported goes, whether the text
// follows, and its size
typedef struct
{
    char id[CLI_NAME_MAX_LEN];
    char root[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char key[CLI_PATH_MAX_LEN];
    unsigned long long run;
    CliServersSink sink;
    int has_text;
    unsigned long long size;
} CliServersCheckerJob;

static CliKeyed s_runs = {"checker runs", {NULL}, 0u, CLI_SERVERS_RUNS_MAX};

// the checkers named and the languages known, as a job is made, under its lock

typedef struct
{
    const char *key;
    const char *id;
    char *joined;
    unsigned long long capacity;
    const char *path;
    char *into;
} ServersCheckersCtx;
// the key a run is counted under: the file's key and the checker's id
EMBED_INLINE void checkers_run_key(const ServersCheckersCtx *args)
{
    VerbaLinea line = {args->joined, args->capacity, 0ULL};
    mmgr_line_put(&line, args->key);
    mmgr_line_ch(&line, '\n');
    mmgr_line_put(&line, args->id);
    (void)mmgr_line_finish(&line);
}

unsigned long long cli_servers_run_count(const CliServersKeyCtx *args)
{
    char joined[CLI_PATH_MAX_LEN + CLI_NAME_MAX_LEN + 2u];
    EMBED_CALL(checkers_run_key, ServersCheckersCtx, .key = args->key, .id = args->id, .joined = joined,
               .capacity = sizeof(joined));
    const unsigned int stored =
        EMBED_CALL(cli_keyed_key_adjacent.find, CliKeyedKeyAdjacentCfg, .keyed = &s_runs, .key = joined);
    unsigned long long run = 0ULL;
    if (stored != CLI_KEYED_NONE)
    {
        const CliKeyedEntry one = EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = &s_runs, .at = stored);
        if (one.text_size == sizeof(run))
        {
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                .cell = one.cell, .offset = (embed_word)one.text_at, .dst = &run, .bytes = sizeof(run)});
        }
    }
    run += 1ULL;
    (void)EMBED_CALL(cli_keyed_key_adjacent.put, CliKeyedKeyAdjacentCfg, .keyed_out = &s_runs, .key = joined,
                     .adjacent_value = "", .text = (const char *)&run, .size_value = sizeof(run));
    return run;
}

int cli_servers_run_last(const CliServersKeyCtx *args)
{
    char joined[CLI_PATH_MAX_LEN + CLI_NAME_MAX_LEN + 2u];
    EMBED_CALL(checkers_run_key, ServersCheckersCtx, .key = args->key, .id = args->id, .joined = joined,
               .capacity = sizeof(joined));
    const unsigned int stored =
        EMBED_CALL(cli_keyed_key_adjacent.find, CliKeyedKeyAdjacentCfg, .keyed = &s_runs, .key = joined);
    unsigned long long last = 0ULL;
    if (stored != CLI_KEYED_NONE)
    {
        const CliKeyedEntry one = EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = &s_runs, .at = stored);
        if (one.text_size == sizeof(last))
        {
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                .cell = one.cell, .offset = (embed_word)one.text_at, .dst = &last, .bytes = sizeof(last)});
        }
    }
    return last == args->run_u64;
}

// one file checked by one checker, its findings written on a line grown to them and stored where
// its run is still the last
static void checkers_file_job(const CliServersJobCfg *args)
{
    unsigned long long size = args->size;
    const CliServersCheckerJob *const one = args->job;
    if (size < sizeof(*one) + (one->has_text ? one->size : 0ULL))
    {
        return;
    }
    CliGrownLine found;
    cli_grown_begin(&found, CLI_SERVERS_ANSWER, g_cli_servers_owner);
    char quiet[1024];
    VerbaLinea err_msg = {quiet, sizeof(quiet), 0ULL};
    const char *const text = one->has_text ? (const char *)args->job + sizeof(*one) : NULL;
    const int checked =
        EMBED_CALL(cli_checkers_out.check_file, CliCheckersOutCfg, .id = one->id, .root = one->root, .path = one->path,
                   .text = text, .length = one->size, .out = &found.line, .err_msg = &err_msg);
    if (checked && mmgr_line_ok(&found.line) &&
        EMBED_CALL(cli_servers_key_internal.run_last, CliServersKeyCtx, .key = one->key, .id = one->id,
                   .run_u64 = one->run))
    {
        EMBED_CALL(cli_servers_size_internal.findings_store, CliServersSizeCtx, .id = one->id, .found = found.cell,
                   .size = cli_grown_size(&found), .always = 0, .sink = &one->sink);
    }
    cli_grown_end(&found);
}

// the tree checked whole by one checker, its findings written on a line grown to them and stored
static void checkers_tree_job(const CliServersJobCfg *args)
{
    unsigned long long size = args->size;
    const CliServersCheckerJob *const one = args->job;
    if (size < sizeof(*one))
    {
        return;
    }
    CliGrownLine found;
    cli_grown_begin(&found, CLI_SERVERS_FINDINGS, g_cli_servers_owner);
    char quiet[1024];
    VerbaLinea err_msg = {quiet, sizeof(quiet), 0ULL};
    const int checked = EMBED_CALL(cli_checkers_out.check_tree, CliCheckersOutCfg, .id = one->id, .root = one->root,
                                   .out = &found.line, .err_msg = &err_msg);
    if (checked && mmgr_line_ok(&found.line))
    {
        EMBED_CALL(cli_servers_size_internal.findings_store, CliServersSizeCtx, .id = one->id, .found = found.cell,
                   .size = cli_grown_size(&found), .always = 0, .sink = &one->sink);
    }
    cli_grown_end(&found);
}

// a path copied into what is sized for CLI_PATH_MAX_LEN
EMBED_INLINE void checkers_path_copy(const ServersCheckersCtx *args)
{
    VerbaLinea line = {args->into, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->path);
    (void)mmgr_line_finish(&line);
}

void cli_servers_run_checkers(const CliServersPathSizeRootSinkCtx *args)
{
    const char *path = args->path;
    unsigned long long size = args->size;
    const CliServersSink *sink = args->sink;
    static char s_id[CLI_SERVERS_CHECKERS][CLI_NAME_MAX_LEN];
    const unsigned int ids = g_cli_servers.checkers;
    memor.cpy(&(MemoriaCfg){.dst = s_id, .src = g_cli_servers.checker, .bytes = sizeof(s_id)});
    char language[CLI_NAME_MAX_LEN];
    CliLanguages languages;
    EMBED_CALL(cli_plugins_table.languages_for_call, CliPluginsCfg, .languages = &languages);
    const int known = (ids != 0u) && EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg,
                                                .languages_read = &languages, .path = path, .id_out = language);
    g_cli_guard->persistent_buf_release(languages.pair, g_cli_plugins_owner);
    *(&languages) = (CliLanguages){NULL, 0u, 0u};
    if (!known)
    {
        return;
    }
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_servers_path_key_internal.key_get, CliServersPathKeyCtx, .path = path, .key_out = key);
    for (unsigned int at = 0u; at < ids; at += 1u)
    {
        if (!EMBED_CALL(cli_checkers.checks, CliCheckersCfg, .id = s_id[at], .language = language))
        {
            continue;
        }
        const int input = EMBED_CALL(cli_checkers.reads_input, CliCheckersCfg, .id = s_id[at]);
        if (!args->disk && !input)
        {
            continue;
        }
        // the job made in a cell of this call's own, the text after it where the checker has it, then copied into
        // the cell the servers' thread takes it from
        const int has_text = (args->text != NULL) && (size <= CLI_SERVERS_LARGEST);
        const unsigned long long text_size = has_text ? size : 0ULL;
        char *const job =
            g_cli_guard->temporary_buf_alloc(sizeof(CliServersCheckerJob) + text_size, g_cli_servers_owner);
        if (job == NULL)
        {
            continue;
        }
        CliServersCheckerJob *const one = (CliServersCheckerJob *)(void *)job;
        memor.set(&(MemoriaCfg){.dst = one, .val = 0, .bytes = sizeof(*one)});
        memor.cpy(&(MemoriaCfg){.dst = one->id, .src = s_id[at], .bytes = sizeof(one->id)});
        EMBED_CALL(checkers_path_copy, ServersCheckersCtx, .path = args->root, .into = one->root);
        EMBED_CALL(checkers_path_copy, ServersCheckersCtx, .path = path, .into = one->path);
        memor.cpy(&(MemoriaCfg){.dst = one->key, .src = key, .bytes = sizeof(key)});
        one->run = EMBED_CALL(cli_servers_key_internal.run_count, CliServersKeyCtx, .key = key, .id = s_id[at]);
        one->sink = *sink;
        one->has_text = has_text;
        one->size = text_size;
        if (has_text)
        {
            // a size under CLI_SERVERS_LARGEST fits a embed_word
            memor.cpy(&(MemoriaCfg){.dst = job + sizeof(*one), .src = args->text, .bytes = (embed_word)text_size});
        }
        (void)EMBED_CALL(cli_servers_run_internal.later, CliServersRunCtx, .run = checkers_file_job, .job = job,
                         .size = sizeof(*one) + text_size);
        g_cli_guard->persistent_buf_release(job, g_cli_servers_owner);
    }
}

void cli_servers_check_tree_with_checkers(const CliServersFilesCtx *args)
{
    const CliServersSink *sink = args->sink;
    static char s_id[CLI_SERVERS_CHECKERS][CLI_NAME_MAX_LEN];
    const unsigned int ids = g_cli_servers.checkers;
    memor.cpy(&(MemoriaCfg){.dst = s_id, .src = g_cli_servers.checker, .bytes = sizeof(s_id)});
    for (unsigned int at = 0u; at < ids; at += 1u)
    {
        if (!EMBED_CALL(cli_checkers.reads_tree, CliCheckersCfg, .id = s_id[at]))
        {
            continue;
        }
        CliServersCheckerJob one;
        memor.set(&(MemoriaCfg){.dst = &one, .val = 0, .bytes = sizeof(one)});
        memor.cpy(&(MemoriaCfg){.dst = one.id, .src = s_id[at], .bytes = sizeof(one.id)});
        EMBED_CALL(checkers_path_copy, ServersCheckersCtx, .path = args->root, .into = one.root);
        one.sink = *sink;
        (void)EMBED_CALL(cli_servers_run_internal.later, CliServersRunCtx, .run = checkers_tree_job, .job = &one,
                         .size = sizeof(one));
    }
}
