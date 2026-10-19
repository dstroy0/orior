// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_jobs_test.c: the query's face's check: every command of the query is a job of the protocol group, a learn
// job's words are the command's, and the toolchains give the C compiler where one is found
#include "../../../../../ui/cli/src/command_line/catalog.h"
#include "../../../../../ui/cli/src/runs/protocol.h"
#include "../../../../../ui/cli/src/runs/runner.h"
#include "../../../../../ui/query/src/query_loops.h"
#include "cli_check.h"

static CliJob s_jobs[64];
static CliParam s_params[512];
static CliStep s_steps[64];
static CliArg s_args[1024];
static unsigned int s_lists[2048];
static char s_text[1u << 17u];
static CliCatalog s_catalog;
static CliValues s_values;
static char s_shown[4][1024];
static unsigned int s_shown_count;

// a tree holding the files the bridge's folder names, each empty, into `root`, which holds CLI_PATH_LONGEST
static void protocol_tree(const char *name, char *root)
{
    static const char *const s_whole[9] = {"Lstar.klq", "cu.klm",    "cu.krs",    "sass.klm", "sass.krs",
                                           "sm_86.khw", "sm_86.kqr", "sm_86.ksc", "c.krs"};
    char table[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    cli_check_folder(name, root);
    cli_full(root, QUERY_TABLE, table);
    (void)query_folder_made(table);
    for (unsigned int at = 0u; at < 9u; at += 1u)
    {
        cli_full(table, s_whole[at], path);
        (void)query_file_written(path, "", 0ull);
    }
}

// the jobs of the tree at `root` read into the catalog
static void protocol_jobs_read(const char *root)
{
    cli_catalog_begun(&s_catalog, s_jobs, 64u, s_params, 512u, s_steps, 64u, s_args, 1024u, s_lists, 2048u, s_text,
                      sizeof(s_text));
    cli_query_jobs(root, &s_catalog);
}

// a line a step shows kept
static void protocol_kept(void *context, const char *shown)
{
    (void)context;
    if (s_shown_count < 4u)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_shown[s_shown_count],
                                .src = shown,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = shown, .cap = CLI_CELLS_POOL}) + 1u});
        s_shown_count += 1u;
    }
}

// the lines the job `job` shows with the values `pairs`, `count` of them, each `key=value`: how many
static unsigned int protocol_shown(const char *root, unsigned int job, const char *const *pairs, unsigned int count)
{
    cli_values_begun(&s_values);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char key[CLI_NAME_LONGEST];
        const char *const equals = cellul.chr(&(CatenaFinitaCfg){.src = pairs[at], .cap = CLI_CELLS_POOL, .byte = '='});
        // the `=` lies in the pair, past its start
        const unsigned long long length = (equals != NULL) ? (unsigned long long)(equals - pairs[at]) : 0ull;
        memor.cpy(&(MemoriaCfg){.dst = key, .src = pairs[at], .bytes = length});
        key[length] = '\0';
        cli_values_put(&s_values, key, (equals != NULL) ? equals + 1 : "");
    }
    s_shown_count = 0u;
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    (void)cli_runner_shown(root, &s_catalog, job, &s_values, protocol_kept, NULL, &said);
    return s_shown_count;
}

// whether `line` ends in `end`
static int protocol_ends(const char *line, const char *end)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL});
    return (size >= cellul.len(&(CatenaFinitaCfg){.src = end, .cap = CLI_CELLS_POOL})) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = line + size -
                                                cellul.len(&(CatenaFinitaCfg){.src = end, .cap = CLI_CELLS_POOL}),
                                         .other = end,
                                         .cap = CLI_CELLS_POOL}));
}

// a choice of a param, the empty text past its last
static const char *protocol_choice(const CliParam *param, unsigned int at)
{
    return (at < param->choices) ? cli_catalog_at(&s_catalog, s_catalog.list[param->first + at]) : "";
}

// every command of the query is a job of the protocol group
static void protocol_protocol(void)
{
    static const char *const s_ids[11] = {"protocol/course", "protocol/tables", "protocol/decode", "protocol/query",
                                          "protocol/learn",  "protocol/cross",  "protocol/write",  "protocol/pairs",
                                          "protocol/stick",  "protocol/kept",   "protocol/sweep"};
    char root[CLI_PATH_LONGEST];
    protocol_tree("query_jobs", root);
    protocol_jobs_read(root);
    int listed = s_catalog.jobs == 11u;
    for (unsigned int at = 0u; listed && (at < 11u); at += 1u)
    {
        const CliJob *const job = &s_catalog.job[at];
        listed = (cellul.eq(&(CatenaFinitaCfg){
                     .src = cli_catalog_at(&s_catalog, job->id), .other = s_ids[at], .cap = CLI_CELLS_POOL})) &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = job->group, .other = "protocol", .cap = CLI_CELLS_POOL})) &&
                 (job->steps > 0u) && (s_catalog.step[job->step_first].program == CLI_PROGRAM_ORIOR);
    }
    cli_check(listed, "every command of the query is a job of the protocol group, run by orior");
    const CliJob *const course = &s_catalog.job[0];
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = cli_catalog_at(&s_catalog, course->title), .other = "protocol", .cap = CLI_CELLS_POOL}),
              "the course is the protocol");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = protocol_choice(&s_catalog.param[course->param_first], 0u),
                                           .other = "vendor tables",
                                           .cap = CLI_CELLS_POOL}),
              "it starts from the vendor's tables");
    static const char *const s_all[1] = {"leave=all"};
    cli_check((protocol_shown(root, 0u, s_all, 1u) > 0u) && protocol_ends(s_shown[0], "protocol course --leave all"),
              "its words leave every file");
    static const char *const s_from[2] = {"from=costs", "leave=none"};
    cli_check((protocol_shown(root, 0u, s_from, 2u) > 0u) &&
                  protocol_ends(s_shown[0], "protocol course --from costs --leave none"),
              "and start where they are asked to");
    const CliJob *const query = &s_catalog.job[3];
    const CliParam *const from = &s_catalog.param[query->param_first];
    const CliParam *const part = &s_catalog.param[query->param_first + 2u];
    cli_check(
        (from->choices == 2u) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = protocol_choice(from, 0u), .other = "cu", .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = protocol_choice(from, 1u), .other = "sass", .cap = CLI_CELLS_POOL})),
        "c has a ruleset and no map, so it is no language yet");
    cli_check((part->choices == 1u) &&
                  (cellul.eq(
                      &(CatenaFinitaCfg){.src = protocol_choice(part, 0u), .other = "sm_86", .cap = CLI_CELLS_POOL})) &&
                  !part->required,
              "a part is a choice and not required");
    static const char *const s_query[4] = {"from=cu", "to=sass", "part=sm_86", "dry=dry"};
    cli_check((protocol_shown(root, 3u, s_query, 4u) == 1u) &&
                  protocol_ends(s_shown[0], "protocol query --from cu --to sass --part sm_86 --dry"),
              "a query's words are its values");
    static const char *const s_bridge[2] = {"from=cu", "to=sass"};
    cli_check((protocol_shown(root, 3u, s_bridge, 2u) > 0u) &&
                  protocol_ends(s_shown[0], "protocol query --from cu --to sass"),
              "and the bridge alone where no part is named");
    static const char *const s_learn[3] = {"step=turns", "base=a.cu", "leave=none"};
    cli_check((protocol_shown(root, cli_catalog_find(&s_catalog, "protocol/learn"), s_learn, 3u) > 0u) &&
                  protocol_ends(s_shown[0], "protocol learn turns --base a.cu --leave none"),
              "a learn job's words are the command's");
}

int main(void)
{
    protocol_protocol();
    const QueryPrograms programs = cli_protocol_programs();
    char path[CLI_PATH_LONGEST];
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    const int found = programs.program(programs.context, QUERY_ROLE_C, path, sizeof(path), &said);
    (void)cli_line_closed(&said);
    cli_check(
        found ? query_is_file(path)
              : (cellul.find(&(CatenaFinitaCfg){
                     .src = why, .cap = CLI_CELLS_POOL, .other = "Toolchains", .other_cap = CLI_CELLS_POOL}) != NULL),
        "the toolchains give the C compiler where one is found, and say where it is not");
    const char *const run_path = programs.run_path(programs.context);
    cli_check((run_path != NULL) && (run_path[0] != '\0'), "and the PATH every program runs with");
    return cli_checked();
}
