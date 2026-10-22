// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_kept_test.c: the keepers: a folder whose newest writer runs is left; the writer's account is read from its
// record; a process of the tree on the part is found whichever way its path is written; the part's allowed use is
// read; folders with runs skip git's and the restored; the part held used is said on a line of its own; and the median
// rate names the runs under half of it
#include "../../../../../ui/query/src/runs/kept_internal.h"
#include "cli_check.h"

static char s_root[QUERY_PATH_LONGEST];
static char s_heard[8][1024];
static unsigned int s_heard_count;
static QueryRun s_live;

// a line said, kept
static void kept_said(void *context, const char *text, unsigned long long length)
{
    (void)context;
    if (s_heard_count < 8u)
    {
        CliLine line = {s_heard[s_heard_count], sizeof(s_heard[0]), 0ull};
        cli_line_counted(&line, text, length);
        (void)cli_line_closed(&line);
        s_heard_count += 1u;
    }
}

// whether a line heard holds `text`
static int kept_heard(const char *text)
{
    for (unsigned int at = 0u; at < s_heard_count; at += 1u)
    {
        if (cellul.find(&(CatenaFinitaCfg){
                .src = s_heard[at], .cap = CLI_CELLS_POOL, .other = text, .other_cap = CLI_CELLS_POOL}) != NULL)
        {
            return 1;
        }
    }
    return 0;
}

// a folder whose newest writer runs is left
static void kept_live(QueryContext *context, QueryPrograms *programs)
{
    char compiler[QUERY_PATH_LONGEST];
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    if (!programs->program(programs->context, QUERY_ROLE_C, compiler, sizeof(compiler), &said))
    {
        cli_check(1, "no C compiler to build the writer with");
        return;
    }
    char base[QUERY_PATH_LONGEST];
    char folder[QUERY_PATH_LONGEST];
    char old[QUERY_PATH_LONGEST];
    cli_full(s_root, "build/query_test/kept_live", base);
    (void)query_removed(base);
    cli_full(base, "qry", folder);
    said.at = 0ull;
    cli_check(query_run_started(&s_live, s_root, folder, programs, context, &said), "a run is live");
    cli_full(folder, "query0-20250101T000000.qry", old);
    (void)query_file_written(old, "", 0ull);
    s_heard_count = 0u;
    query_kept_folder(context, folder, NULL, 64u, &s_live.writer);
    cli_check(kept_heard("left as it is, its newest run's writer runs"), "a folder whose newest writer runs is left");
    cli_check(query_is_file(old), "its runs as they were");
    said.at = 0ull;
    (void)query_run_stopped(&s_live, &said);
    (void)query_removed(base);
}

// the writer's account is read from its record
static void kept_account(void)
{
    static const unsigned long long s_words[6] = {3ull, 1234567ull, 0ull, 0ull, 0ull, 2250000ull};
    unsigned char blob[48];
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        for (unsigned int byte = 0u; byte < 8u; byte += 1u)
        {
            // a byte of the word, the low byte first
            blob[(at * 8u) + byte] = (unsigned char)((s_words[at] >> (8u * byte)) & 0xffu);
        }
    }
    QueryAccount account;
    cli_check(query_account_read(blob, sizeof(blob), &account) && (account.records == 3ull) &&
                  (account.bytes == 1234567ull) && (account.ms == 2.25) && (account.panics == 0ull),
              "the writer's account is read from its record");
    const double wanted = (1234567.0 / 1048576.0) / (2.25 / 1000.0);
    const double rate = query_account_rate(&account);
    cli_check(((rate > wanted) ? (rate - wanted) : (wanted - rate)) < 1e-6, "its rate in MiB a second");
    cli_check(!query_account_read((const unsigned char *)"nothing here", 12ull, &account), "and nothing else read");
    memor.set(&(MemoriaCfg){.dst = blob + 40u, .val = 0, .bytes = 8u});
    cli_check(query_account_read(blob, sizeof(blob), &account) && (query_account_rate(&account) == 0.0),
              "no time writing is no rate");
}

// a process of the tree on the part is found whichever way its path is written; and the part's allowed use is read
static void kept_part(void)
{
    static const char s_root_given[] = "C:/Users/x/orior-engine";
    cli_check(query_on_the_part("C:\\Users\\x\\orior-engine\\build\\engine\\orior_cubin_run.exe\n", s_root_given),
              "nvidia-smi writes the host's separators");
    cli_check(query_on_the_part("c:/users/x/orior-engine/build/engine/khw_write.exe\n", s_root_given),
              "and either case");
    cli_check(!query_on_the_part("C:\\Users\\x\\other_tree\\build\\engine\\orior_cubin_run.exe\n", s_root_given) &&
                  !query_on_the_part("", s_root_given),
              "another tree's process is not the tree's");
    cli_check((query_part_used_most(NULL) == 10ll) && (query_part_used_most("25") == 25ll), "the part's allowed use");
    // a negative bound, which every reading is past, holds the part used whatever it reads
    cli_check((query_part_used_most("-1") == -1ll) && (query_part_used_most("many") == 10ll),
              "a negative bound, and one that is no number");
}

// folders with runs skip git's and the restored
static void kept_folders(void)
{
    char base[QUERY_PATH_LONGEST];
    char path[QUERY_PATH_LONGEST];
    cli_full(s_root, "build/query_test/folders", base);
    (void)query_removed(base);
    static const char *const s_folders[3] = {"a/qry/restored", ".git/qry", "b"};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        cli_full(base, s_folders[at], path);
        (void)query_folder_made(path);
    }
    static const char *const s_files[4] = {"a/qry/query1-x.qry", "a/qry/restored/query1-x.qry", ".git/qry/query1-x.qry",
                                           "b/other.qry"};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        cli_full(base, s_files[at], path);
        (void)query_file_written(path, "", 0ull);
    }
    static char s_found[64][QUERY_PATH_LONGEST];
    const unsigned int count = query_folders_with_runs(base, s_found, 64u);
    cli_full(base, "a/qry", path);
    cli_check((count == 1u) && (cellul.eq(&(CatenaFinitaCfg){.src = s_found[0], .other = path, .cap = CLI_CELLS_POOL})),
              "folders with runs skip git's and the restored");
    (void)query_removed(base);
}

// the part kept used is said on a line of its own; and the median rate names the runs under half of it
static void kept_said_lines(QueryContext *context)
{
    s_heard_count = 0u;
    query_crystals_refused(context, 0, "the part is used 3 percent", 0);
    cli_check(
        (s_heard_count == 2u) &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = s_heard[0], .other = "  no run is kept as its crystal this time:", .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(
                &(CatenaFinitaCfg){.src = s_heard[1], .other = "  the part is used 3 percent", .cap = CLI_CELLS_POOL})),
        "the part held used is said on a line of its own");
    s_heard_count = 0u;
    query_crystals_refused(context, 1, "", 0);
    cli_check((s_heard_count == 1u) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = s_heard[0],
                      .other = "  no run is kept as its crystal this time: the crystal's program is not built",
                      .cap = CLI_CELLS_POOL})),
              "and an unbuilt crystal");
    s_heard_count = 0u;
    query_crystals_refused(context, 1, "", 1);
    cli_check(s_heard_count == 0u, "and nothing where the crystal is built");
    static char s_names[3][QUERY_PATH_LONGEST] = {"query1-a.qry", "query2-b.qry", "query3-c.qry"};
    static const double s_rates[3] = {10.0, 4.0, 12.0};
    s_heard_count = 0u;
    query_median_said(context, "x/qry", s_names, s_rates, 3u);
    cli_check((s_heard_count > 0u) && (cellul.find(&(CatenaFinitaCfg){.src = s_heard[0],
                                                                      .cap = CLI_CELLS_POOL,
                                                                      .other = "3 runs read, the median 10 MiB/s",
                                                                      .other_cap = CLI_CELLS_POOL}) != NULL),
              "the median rate");
    cli_check(kept_heard("query2-b.qry wrote at under half the median"), "names the runs under half of it");
    s_heard_count = 0u;
    query_median_said(context, "x/qry", s_names, s_rates, 0u);
    cli_check(s_heard_count == 0u, "and no runs say nothing");
}

int main(void)
{
    char found[CLI_PATH_LONGEST];
    if (!cli_tree_found(found))
    {
        cli_check(1, "no tree is found, and nothing is kept");
        return cli_checked();
    }
    memor.cpy(&(MemoriaCfg){.dst = s_root,
                            .src = found,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = found, .cap = CLI_CELLS_POOL}) + 1u});
    for (char *at = s_root; at[0] != '\0'; at += 1)
    {
        at[0] = (at[0] == '\\') ? '/' : at[0];
    }
    static QueryPrograms s_programs;
    s_programs = query_on_path();
    static QueryContext s_context;
    s_context =
        (QueryContext){s_root, &s_programs, {kept_said, NULL}, QUERY_LEAVE_ALL, query_terminal(), {{0}}, 0u, ""};
    kept_account();
    kept_part();
    kept_folders();
    kept_said_lines(&s_context);
    kept_live(&s_context, &s_programs);
    return cli_checked();
}
