// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// catalog_jobs.c: the driver's jobs, the runners, the page builders, the stage scripts and their pipelines, the test
// suites, and the whole catalog read in the order the engine's own steps run
#include "../text/text.h"
#include "catalog_internal.h"
#include "cells.h"

// A reading's lists, a cell taken with the lock and given back with it: the runs, parts and paths listed, the
// subjects, the folders within them and their files, and the folders of each depth collected, each list in a cell of
// its own sized to what it holds
typedef struct
{
    CliPaths runs;
    CliEntries parts;
    CliPaths paths;
    CliEntries subjects;
    CliEntries inner;
    CliEntries files;
    CliEntries dirs[17];
} CliCatalogJobsRoom;

static CliCatalogJobsRoom *s_jobs;
static CliLock s_lock = CLI_LOCK_INIT;

// the arguments every run of the driver ends with
static void cli_set(CliCatalog *catalog)
{
    cli_catalog_arg(catalog, CLI_ARG_VALUE, "set", NULL, NULL, NULL, 0);
    cli_catalog_arg(catalog, CLI_ARG_WORDS, "samples", NULL, NULL, NULL, 0);
}

// a choice param of the runs listed
static void cli_choice(CliCatalog *catalog, const char *key, char (*list)[CLI_PATH_LONGEST], unsigned int count,
                       int required)
{
    cli_catalog_param(catalog, key, "choice", required);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_catalog_choice(catalog, list[at]);
    }
}

// The cell tracker's driver as ingest, as a run of its parts, and as a render of what it found
static void cli_driver(const char *root, CliCatalog *catalog)
{
    char source[CLI_PATH_LONGEST];
    char file[CLI_PATH_LONGEST];
    char tracker[CLI_PATH_LONGEST];
    char base[CLI_PATH_LONGEST];
    char cfg[CLI_PATH_LONGEST];
    cli_catalog_join(root, "examples/cell_tracking/src/track_driver/track_driver.cu", source);
    char *const text = cli_catalog_script_taken(source);
    if (text == NULL)
    {
        return;
    }
    (void)cli_catalog_names_room(&s_jobs->parts, cli_catalog_driver_parts(text, NULL, 0u));
    const unsigned int parts =
        (s_jobs->parts.name != NULL) ? cli_catalog_driver_parts(text, s_jobs->parts.name, s_jobs->parts.room) : 0u;
    cli_catalog_text_given(text);
    cli_relative(root, source, file);
    char *const taken = cli_catalog_about_taken(source);
    const char *const about = (taken != NULL) ? taken : "";
    cli_catalog_join(root, "examples/cell_tracking", tracker);
    cli_catalog_join(tracker, "base.cfg", base);
    cli_catalog_join(tracker, "cfg", cfg);
    unsigned int runs = 0u;
    if (cli_catalog_paths_room(&s_jobs->runs, 1u))
    {
        cli_relative(root, base, s_jobs->runs.path[0]);
        runs = cli_catalog_paths(root, cfg, cli_catalog_is_cfg, &s_jobs->runs, 1u);
    }

    cli_catalog_job(catalog, "ingest/track_driver", "ingest", "track_driver --ingest", file, about, "");
    cli_catalog_param(catalog, "source", "dir", 1);
    cli_catalog_param(catalog, "axes", "word", 0);
    cli_catalog_param(catalog, "channel", "word", 0);
    cli_catalog_param(catalog, "set", "save-dir", 1);
    cli_catalog_param(catalog, "samples", "words", 0);
    cli_catalog_step(catalog, CLI_PROGRAM_BUILT, "track_driver", NULL, 0);
    cli_catalog_arg(catalog, CLI_ARG_LIT, "--ingest", NULL, NULL, NULL, 0);
    cli_catalog_arg(catalog, CLI_ARG_FLAG, "--source", "source", NULL, NULL, 0);
    cli_catalog_arg(catalog, CLI_ARG_FLAG, "--axes", "axes", NULL, NULL, 0);
    cli_catalog_arg(catalog, CLI_ARG_FLAG, "--channel", "channel", NULL, NULL, 0);
    cli_set(catalog);

    cli_catalog_job(catalog, "run/track_driver", "run", "track_driver --run", file, about, "");
    cli_choice(catalog, "cfg", s_jobs->runs.path, runs, 0);
    cli_catalog_param(catalog, "cfg-out", "save", 0);
    cli_catalog_param(catalog, "run", "many", 0);
    for (unsigned int at = 0u; at < parts; at += 1u)
    {
        cli_catalog_choice(catalog, s_jobs->parts.name[at]);
    }
    cli_catalog_words(catalog);
    cli_catalog_param(catalog, "set", "dir", 0);
    cli_catalog_param(catalog, "samples", "words", 0);
    cli_catalog_step(catalog, CLI_PROGRAM_BUILT, "track_driver", NULL, 0);
    cli_catalog_arg(catalog, CLI_ARG_FLAG, "--cfg", "cfg", NULL, NULL, 0);
    cli_catalog_arg(catalog, CLI_ARG_FLAG, "--cfg-out", "cfg-out", NULL, NULL, 0);
    cli_catalog_arg(catalog, CLI_ARG_EACH, "--run", "run", NULL, NULL, 0);
    cli_catalog_arg(catalog, CLI_ARG_WORDS, "arguments", NULL, NULL, NULL, 0);
    cli_set(catalog);

    cli_catalog_job(catalog, "render/track_driver", "render", "track_driver --vis", file, about, "");
    cli_choice(catalog, "cfg", s_jobs->runs.path, runs, 0);
    cli_catalog_param(catalog, "vis", "save", 0);
    cli_catalog_param(catalog, "export", "save", 0);
    cli_catalog_words(catalog);
    cli_catalog_param(catalog, "set", "dir", 0);
    cli_catalog_param(catalog, "samples", "words", 0);
    cli_catalog_step(catalog, CLI_PROGRAM_BUILT, "track_driver", NULL, 0);
    cli_catalog_arg(catalog, CLI_ARG_FLAG, "--cfg", "cfg", NULL, NULL, 0);
    cli_catalog_arg(catalog, CLI_ARG_FLAG, "--vis", "vis", NULL, NULL, 0);
    cli_catalog_arg(catalog, CLI_ARG_FLAG, "--export", "export", NULL, NULL, 0);
    cli_catalog_arg(catalog, CLI_ARG_WORDS, "arguments", NULL, NULL, NULL, 0);
    cli_set(catalog);
    cli_catalog_text_given(taken);
}

// The other runners: the Navier-Stokes programs on their cfgs and the qasm circuits
static void cli_runners(const char *root, CliCatalog *catalog)
{
    char navier[CLI_PATH_LONGEST];
    char script[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    cli_catalog_join(root, "examples/navier_stokes/run.sh", navier);
    char *const text = cli_catalog_script_taken(navier);
    if (text != NULL)
    {
        (void)cli_catalog_names_room(&s_jobs->parts, cli_catalog_case_names(text, "PROGRAM", NULL, 0u));
        const unsigned int programs =
            (s_jobs->parts.name != NULL)
                ? cli_catalog_case_names(text, "PROGRAM", s_jobs->parts.name, s_jobs->parts.room)
                : 0u;
        cli_catalog_text_given(text);
        cli_catalog_join(root, "examples/navier_stokes/cfg", folder);
        const unsigned int configs = cli_catalog_paths(root, folder, cli_catalog_is_cfg, &s_jobs->runs, 0u);
        cli_relative(root, navier, script);
        cli_catalog_script_job(catalog, root, "run", navier, NULL, "");
        cli_catalog_param(catalog, "program", "choice", 1);
        for (unsigned int at = 0u; at < programs; at += 1u)
        {
            cli_catalog_choice(catalog, s_jobs->parts.name[at]);
        }
        cli_choice(catalog, "cfg", s_jobs->runs.path, configs, 1);
        cli_catalog_step(catalog, CLI_PROGRAM_BASH, script, NULL, 0);
        cli_catalog_arg(catalog, CLI_ARG_VALUE, "program", NULL, NULL, NULL, 0);
        cli_catalog_arg(catalog, CLI_ARG_VALUE, "cfg", NULL, NULL, NULL, 0);
    }
    char qasm[CLI_PATH_LONGEST];
    cli_catalog_join(root, "examples/qasm/run.sh", qasm);
    if (query_is_file(qasm))
    {
        cli_catalog_join(root, "examples/qasm/test", folder);
        const unsigned int circuits = cli_catalog_paths(root, folder, cli_catalog_is_qasm, &s_jobs->runs, 0u);
        cli_relative(root, qasm, script);
        cli_catalog_script_job(catalog, root, "run", qasm, NULL, "");
        cli_catalog_param(catalog, "circuit", "choice-or-file", 1);
        for (unsigned int at = 0u; at < circuits; at += 1u)
        {
            cli_catalog_choice(catalog, s_jobs->runs.path[at]);
        }
        cli_catalog_words(catalog);
        cli_catalog_step(catalog, CLI_PROGRAM_BASH, script, NULL, 0);
        cli_catalog_arg(catalog, CLI_ARG_WORDS, "arguments", NULL, NULL, NULL, 0);
        cli_catalog_arg(catalog, CLI_ARG_VALUE, "circuit", NULL, NULL, NULL, 0);
    }
}

// one page builder's job, where its path is not one seen before
static void cli_view(const char *root, CliCatalog *catalog, const char *path, unsigned int *seen)
{
    for (unsigned int at = 0u; at < *seen; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_jobs->paths.path[at], .other = path, .cap = CLI_CELLS_POOL}))
        {
            return;
        }
    }
    if (*seen < s_jobs->paths.room)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_jobs->paths.path[*seen],
                                .src = path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
        *seen += 1u;
    }
    char file[CLI_PATH_LONGEST];
    char stem[CLI_NAME_LONGEST];
    cli_relative(root, path, file);
    cli_catalog_stem(path, stem);
    const char *title = stem;
    while (cellul.starts(&(CatenaFinitaCfg){.src = title, .other = "build_", .cap = CLI_CELLS_POOL}))
    {
        title += 6;
    }
    cli_catalog_script_job(catalog, root, "view", path, title, "views");
    cli_catalog_param(catalog, "input", "file", 0);
    cli_catalog_words(catalog);
    cli_catalog_step(catalog, CLI_PROGRAM_PYTHON, file, NULL, 0);
    cli_catalog_arg(catalog, CLI_ARG_VALUE, "input", NULL, NULL, NULL, 0);
    cli_catalog_arg(catalog, CLI_ARG_WORDS, "arguments", NULL, NULL, NULL, 0);
}

// Every page builder: the viewers, and the builders beside the examples that write a page. A builder that takes a file
// as its first word gets one, and every one takes free words after it
static void cli_views(const char *root, CliCatalog *catalog)
{
    char examples[CLI_PATH_LONGEST];
    unsigned int places = 0u;
    unsigned int seen = 0u;
    cli_catalog_join(root, "examples", examples);
    cli_catalog_entries(examples, 1, NULL, &s_jobs->subjects);
    (void)cli_catalog_paths_room(&s_jobs->runs, 1u + (2u * s_jobs->subjects.count));
    if (s_jobs->runs.room != 0u)
    {
        cli_catalog_join(root, "examples/00_blob_viz_tools", s_jobs->runs.path[places]);
        places += 1u;
    }
    for (unsigned int at = 0u; (at < s_jobs->subjects.count) && ((places + 2u) <= s_jobs->runs.room); at += 1u)
    {
        char subject[CLI_PATH_LONGEST];
        cli_catalog_join(examples, s_jobs->subjects.name[at], subject);
        cli_catalog_join(subject, "maint", s_jobs->runs.path[places]);
        memor.cpy(&(MemoriaCfg){.dst = s_jobs->runs.path[places + 1u],
                                .src = subject,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = subject, .cap = CLI_CELLS_POOL}) + 1u});
        places += 2u;
    }
    for (unsigned int at = 0u; at < places; at += 1u)
    {
        char path[CLI_PATH_LONGEST];
        cli_catalog_entries(s_jobs->runs.path[at], 0, cli_catalog_is_builder, &s_jobs->files);
        (void)cli_catalog_paths_room(&s_jobs->paths, seen + s_jobs->files.count);
        for (unsigned int each = 0u; each < s_jobs->files.count; each += 1u)
        {
            cli_catalog_join(s_jobs->runs.path[at], s_jobs->files.name[each], path);
            cli_view(root, catalog, path, &seen);
        }
        cli_catalog_entries(s_jobs->runs.path[at], 0, cli_catalog_is_figure, &s_jobs->files);
        (void)cli_catalog_paths_room(&s_jobs->paths, seen + s_jobs->files.count);
        for (unsigned int each = 0u; each < s_jobs->files.count; each += 1u)
        {
            cli_catalog_join(s_jobs->runs.path[at], s_jobs->files.name[each], path);
            cli_view(root, catalog, path, &seen);
        }
    }
}

// whether a folder is a stage's: its name two or more letters, a digit and then `_`
static int cli_is_stage(const char *name)
{
    return (cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) > 2u) && (name[0] >= '0') &&
           (name[0] <= '9') && (name[1] == '_');
}

// Each subject's stage scripts, one job a script, and one pipeline a subject that runs its stages in order and stops
// at the first that fails
static void cli_stages(const char *root, CliCatalog *catalog)
{
    char examples[CLI_PATH_LONGEST];
    cli_catalog_join(root, "examples", examples);
    cli_catalog_entries(examples, 1, NULL, &s_jobs->subjects);
    for (unsigned int at = 0u; at < s_jobs->subjects.count; at += 1u)
    {
        char subject[CLI_PATH_LONGEST];
        cli_catalog_join(examples, s_jobs->subjects.name[at], subject);
        cli_catalog_entries(subject, 1, NULL, &s_jobs->inner);
        unsigned int every = 0u;
        for (unsigned int stage = 0u; stage < s_jobs->inner.count; stage += 1u)
        {
            if (!cli_is_stage(s_jobs->inner.name[stage]))
            {
                continue;
            }
            char folder[CLI_PATH_LONGEST];
            cli_catalog_join(subject, s_jobs->inner.name[stage], folder);
            cli_catalog_entries(folder, 0, cli_catalog_is_py, &s_jobs->files);
            (void)cli_catalog_paths_room(&s_jobs->paths, every + s_jobs->files.count);
            for (unsigned int each = 0u; each < s_jobs->files.count; each += 1u)
            {
                char script[CLI_PATH_LONGEST];
                char file[CLI_PATH_LONGEST];
                char title[CLI_PATH_LONGEST];
                cli_catalog_join(folder, s_jobs->files.name[each], script);
                cli_relative(root, script, file);
                cli_relative(subject, script, title);
                if (every < s_jobs->paths.room)
                {
                    memor.cpy(&(MemoriaCfg){
                        .dst = s_jobs->paths.path[every],
                        .src = file,
                        .bytes = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}) + 1u});
                    every += 1u;
                }
                cli_catalog_script_job(catalog, root, "stage", script, title, "");
                cli_catalog_words(catalog);
                cli_catalog_step(catalog, CLI_PROGRAM_PYTHON, file, NULL, 0);
                cli_catalog_arg(catalog, CLI_ARG_WORDS, "arguments", NULL, NULL, NULL, 0);
            }
        }
        if (every == 0u)
        {
            continue;
        }
        char name[CLI_NAME_LONGEST];
        char id[CLI_PATH_LONGEST];
        char file[CLI_PATH_LONGEST];
        char readme[CLI_PATH_LONGEST];
        cli_catalog_stem(subject, name);
        CliLine line = {id, sizeof(id), 0ull};
        cli_line_text(&line, "pipeline/");
        cli_line_text(&line, name);
        cli_line_closed(&line);
        cli_relative(root, subject, file);
        cli_catalog_join(subject, "README.md", readme);
        char *const about = query_is_file(readme) ? cli_catalog_markdown_taken(readme) : NULL;
        cli_catalog_job(catalog, id, "pipeline", name, file, (about != NULL) ? about : "", "");
        cli_catalog_text_given(about);
        for (unsigned int each = 0u; each < every; each += 1u)
        {
            cli_catalog_step(catalog, CLI_PROGRAM_PYTHON, s_jobs->paths.path[each], NULL, 0);
            cli_catalog_arg(catalog, CLI_ARG_WORDS, "arguments", NULL, NULL, NULL, 0);
        }
    }
}

// every file named `name` under `under`, here and in its folders depth first, put into the list from `count`: the
// count after
static unsigned int cli_collect(const char *under, const char *name, unsigned int count, unsigned int depth)
{
    char here[CLI_PATH_LONGEST];
    cli_catalog_join(under, name, here);
    if (query_is_file(here) && cli_catalog_paths_room(&s_jobs->paths, count + 1u))
    {
        memor.cpy(&(MemoriaCfg){.dst = s_jobs->paths.path[count],
                                .src = here,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = here, .cap = CLI_CELLS_POOL}) + 1u});
        count += 1u;
    }
    if (depth >= 16u)
    {
        return count;
    }
    // the folders read before going down, so a deeper read takes another room
    cli_catalog_entries(under, 1, NULL, &s_jobs->dirs[depth]);
    for (unsigned int at = 0u; at < s_jobs->dirs[depth].count; at += 1u)
    {
        char dir[CLI_PATH_LONGEST];
        cli_catalog_join(under, s_jobs->dirs[depth].name[at], dir);
        count = cli_collect(dir, name, count, depth + 1u);
    }
    cli_catalog_entries_given(&s_jobs->dirs[depth]);
    return count;
}

// The test suites: every run.sh under utils/test and under an example's test folder
static void cli_tests(const char *root, CliCatalog *catalog)
{
    char under[CLI_PATH_LONGEST];
    char examples[CLI_PATH_LONGEST];
    cli_catalog_join(root, "utils/test", under);
    unsigned int count = cli_collect(under, "run.sh", 0u, 0u);
    cli_catalog_join(root, "examples", examples);
    cli_catalog_entries(examples, 1, NULL, &s_jobs->subjects);
    for (unsigned int at = 0u; at < s_jobs->subjects.count; at += 1u)
    {
        char subject[CLI_PATH_LONGEST];
        cli_catalog_join(examples, s_jobs->subjects.name[at], subject);
        cli_catalog_join(subject, "test", under);
        count = cli_collect(under, "run.sh", count, 0u);
    }
    cli_paths_sorted(s_jobs->paths.path, count);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_catalog_bash_alone(catalog, root, "test", s_jobs->paths.path[at]);
    }
}

// every list of the reading given back
static void cli_jobs_given(void)
{
    cli_catalog_paths_given(&s_jobs->runs);
    cli_catalog_entries_given(&s_jobs->parts);
    cli_catalog_paths_given(&s_jobs->paths);
    cli_catalog_entries_given(&s_jobs->subjects);
    cli_catalog_entries_given(&s_jobs->inner);
    cli_catalog_entries_given(&s_jobs->files);
    for (unsigned int depth = 0u; depth < 17u; depth += 1u)
    {
        cli_catalog_entries_given(&s_jobs->dirs[depth]);
    }
}

void cli_catalog_read(const char *root, CliCatalog *catalog)
{
    cli_lock_take(&s_lock);
    s_jobs = cli_cell_taken_by(&g_cli_catalog_cells, sizeof(*s_jobs), "a reading of the jobs");
    if (s_jobs == NULL)
    {
        cli_lock_give(&s_lock);
        return;
    }
    memor.set(&(MemoriaCfg){.dst = s_jobs, .val = 0, .bytes = sizeof(*s_jobs)});
    cli_catalog_builds(root, catalog);
    cli_executables_jobs(root, catalog);
    cli_deploy_jobs(root, catalog);
    cli_query_jobs(root, catalog);
    cli_driver(root, catalog);
    cli_runners(root, catalog);
    cli_catalog_sims(root, catalog);
    cli_views(root, catalog);
    cli_stages(root, catalog);
    cli_tests(root, catalog);
    cli_jobs_given();
    cli_cell_given_by(&g_cli_catalog_cells, s_jobs);
    s_jobs = NULL;
    cli_lock_give(&s_lock);
}
