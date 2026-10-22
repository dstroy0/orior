// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// catalog_test.c: the jobs' catalog's check: case names reads the line after the case, a build script is traced by
// default and quiet where asked, short keeps the folder and the name, the driver's parts join its literals, and the
// tree gives every group
#include "../../../../../ui/cli/src/command_line/catalog_internal.h"
#include "../../../../../ui/cli/src/runs/runner.h"
#include "cli_check.h"

static CliJob s_jobs[2048];
static CliParam s_params[8192];
static CliStep s_steps[4096];
static CliArg s_args[16384];
static unsigned int s_lists[32768];
static char s_text[1u << 21u];
static CliCatalog s_catalog;
static CliValues s_values;
static char s_shown[512];

// the catalog made empty
static CliCatalog *catalog_empty(void)
{
    cli_catalog_begun(&s_catalog, s_jobs, 2048u, s_params, 8192u, s_steps, 4096u, s_args, 16384u, s_lists, 32768u,
                      s_text, sizeof(s_text));
    return &s_catalog;
}

// the param `key` of the job `job`, NULL where it has none
static const CliParam *catalog_param(unsigned int job, const char *key)
{
    const CliJob *const one = &s_catalog.job[job];
    for (unsigned int at = 0u; at < one->params; at += 1u)
    {
        const CliParam *const param = &s_catalog.param[one->param_first + at];
        if (cellul.eq(
                &(CatenaFinitaCfg){.src = cli_catalog_at(&s_catalog, param->key), .other = key, .cap = CLI_CELLS_POOL}))
        {
            return param;
        }
    }
    return NULL;
}

// the line the job's step shows kept
static void catalog_kept(void *context, const char *shown)
{
    (void)context;
    memor.cpy(&(MemoriaCfg){.dst = s_shown,
                            .src = shown,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = shown, .cap = CLI_CELLS_POOL}) + 1u});
}

// the line the job shows with its trace `trace`: 1, or 0 where it shows none
static int catalog_shown(const char *root, unsigned int job, const char *trace)
{
    cli_values_begun(&s_values);
    cli_values_put(&s_values, CLI_TRACE, trace);
    s_shown[0] = '\0';
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    return cli_runner_shown(root, &s_catalog, job, &s_values, catalog_kept, NULL, &said);
}

// a build script is traced by default and quiet where asked
static void catalog_trace(void)
{
    char root[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    cli_check_folder("catalog_trace", root);
    cli_full(root, "src", path);
    (void)query_folder_made(path);
    cli_full(root, "src/build_it.sh", path);
    (void)query_file_written(path, "#!/usr/bin/env bash\necho built\n", 31ull);
    cli_catalog_bash_alone(catalog_empty(), root, "build", path);
    const CliParam *const trace = catalog_param(0u, CLI_TRACE);
    cli_check((trace != NULL) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = cli_catalog_at(&s_catalog, trace->fallback),
                                                .other = CLI_TRACE_VERBOSE,
                                                .cap = CLI_CELLS_POOL})) &&
                  (trace->choices == 2u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = cli_catalog_at(&s_catalog, s_catalog.list[trace->first]),
                                                .other = CLI_TRACE_VERBOSE,
                                                .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = cli_catalog_at(&s_catalog, s_catalog.list[trace->first + 1u]),
                                                .other = CLI_TRACE_QUIET,
                                                .cap = CLI_CELLS_POOL})),
              "a build's trace is verbose by default, or quiet");
    if (catalog_shown(root, 0u, CLI_TRACE_VERBOSE))
    {
        cli_check(cellul.eq(&(CatenaFinitaCfg){
                      .src = s_shown, .other = "SHELLOPTS=xtrace bash src/build_it.sh", .cap = CLI_CELLS_POOL}),
                  "a build traced runs bash with xtrace");
        (void)catalog_shown(root, 0u, CLI_TRACE_QUIET);
        cli_check(cellul.eq(&(CatenaFinitaCfg){.src = s_shown, .other = "bash src/build_it.sh", .cap = CLI_CELLS_POOL}),
                  "and quiet without");
    }
    cli_catalog_bash_alone(catalog_empty(), root, "run", path);
    cli_check(catalog_param(0u, CLI_TRACE) == NULL, "a script that is no build is not traced");
}

// the tree gives every group
static void catalog_tree(void)
{
    static const char *const s_groups[10] = {"build", "protocol", "ingest", "run",      "render",
                                             "sim",   "view",     "stage",  "pipeline", "test"};
    char root[CLI_PATH_LONGEST];
    if (!cli_tree_found(root))
    {
        cli_check(1, "no tree is found, and no group is read");
        return;
    }
    cli_catalog_read(root, catalog_empty());
    int every = 1;
    for (unsigned int group = 0u; group < 10u; group += 1u)
    {
        int found = 0;
        for (unsigned int at = 0u; !found && (at < s_catalog.jobs); at += 1u)
        {
            found = cellul.eq(
                &(CatenaFinitaCfg){.src = s_catalog.job[at].group, .other = s_groups[group], .cap = CLI_CELLS_POOL});
        }
        every &= found;
    }
    cli_check(every, "the tree gives a job of every group");
    static CliCellsTally s_grown_cells = CLI_CELLS_TALLY("catalog_test_grown");
    CliCatalog grown;
    cli_catalog_growing(&grown, &s_grown_cells, 2048u);
    cli_catalog_read(root, &grown);
    const int same = !grown.full && (grown.jobs == s_catalog.jobs) && (grown.params == s_catalog.params) &&
                     (grown.steps == s_catalog.steps) && (grown.args == s_catalog.args) &&
                     (grown.lists == s_catalog.lists) && (grown.used == s_catalog.used) &&
                     (memor.cmp(&(MemoriaCfg){.src = grown.text, .other = s_catalog.text, .bytes = grown.used}) == 0);
    const unsigned long long holding = s_grown_cells.holds;
    cli_catalog_given(&grown);
    cli_check(same && (holding > 0ull) && (s_grown_cells.holds == 0ull),
              "a catalog that grows reads the tree as one handed its room does, and gives its cells back");
    const unsigned int run = cli_catalog_find(&s_catalog, "run/track_driver");
    const CliParam *const parts = (run != CLI_CATALOG_NONE) ? catalog_param(run, "run") : NULL;
    int track = 0;
    for (unsigned int at = 0u; (parts != NULL) && (at < parts->choices); at += 1u)
    {
        track |= cellul.eq(&(CatenaFinitaCfg){.src = cli_catalog_at(&s_catalog, s_catalog.list[parts->first + at]),
                                              .other = "track",
                                              .cap = CLI_CELLS_POOL});
    }
    cli_check(track, "the driver's parts are read from its usage");
    cli_check((g_cli_catalog_cells.holds == 0ull) && (g_cli_catalog_cells.hw != 0ull),
              "the catalog's cells are each given back as the tree is read");
}

int main(void)
{
    char names[8][CLI_NAME_LONGEST];
    const unsigned int count =
        cli_catalog_case_names("case \"$SIM\" in\n    a|b_c|d) ;;\n    *) exit 2 ;;\nesac\n", "SIM", names, 8u);
    cli_check((count == 3u) && (cellul.eq(&(CatenaFinitaCfg){.src = names[0], .other = "a", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = names[1], .other = "b_c", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = names[2], .other = "d", .cap = CLI_CELLS_POOL})),
              "case names reads the line after the case");
    catalog_trace();
    char shortened[CLI_PATH_LONGEST];
    cli_catalog_short("examples/cell_tracking/build_driver.sh", shortened);
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = shortened, .other = "cell_tracking/build_driver.sh", .cap = CLI_CELLS_POOL}),
              "short keeps the folder and the name");
    cli_catalog_short("src/build_engine.sh", shortened);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = shortened, .other = "src/build_engine.sh", .cap = CLI_CELLS_POOL}),
              "of a path one folder deep");
    cli_catalog_short("run.sh", shortened);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = shortened, .other = "run.sh", .cap = CLI_CELLS_POOL}),
              "and the name alone of one at the top");
    static const char s_source[] =
        "\"  --run names one part; the parts running in the order given: schedule, iapx-prove,\"\n"
        "            \" entropy, floor\\n\"";
    char parts[8][CLI_NAME_LONGEST];
    const unsigned int parts_count = cli_catalog_driver_parts(s_source, parts, 8u);
    cli_check((parts_count == 4u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = parts[0], .other = "schedule", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = parts[1], .other = "iapx-prove", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = parts[2], .other = "entropy", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = parts[3], .other = "floor", .cap = CLI_CELLS_POOL})),
              "the driver's parts join its literals");
    catalog_tree();
    return cli_checked();
}
