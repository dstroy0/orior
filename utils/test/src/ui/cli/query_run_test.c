// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_run_test.c: a query run: runs of a folder are numbered newest first; the keeper removes all but the newest and
// the restored; a folder's runs held are counted to the query and given back fall off its count; the crystal's program
// is given where it is built; a path is named under the top of the tree; a failed loop still stops its writer; a step
// joins the run held open and its end leaves the writer running; carried takes a name from the latest run that holds
// it, else the disk; and a file read is handed under its path and remembered as a leaf
#include "../../../../../ui/query/src/query_internal.h"
#include "cli_check.h"

static char s_root[QUERY_PATH_LONGEST];
static QueryRun s_run;
static QueryRun s_step;
static QueryPrograms s_programs;
static QueryContext s_context;

// a line said, let go
static void run_said(void *context, const char *text, unsigned long long length)
{
    (void)context;
    (void)text;
    (void)length;
}

// the folder `under` the tree's top into `folder`, made empty
static void run_folder(const char *under, char *folder)
{
    cli_full(s_root, under, folder);
    (void)query_removed(folder);
    (void)query_folder_made(folder);
}

// the runs of the folder, their numbers newest first joined by `,`, into `out`, which holds `room`
static const char *run_numbers(const char *folder, char *out, unsigned long long room)
{
    static unsigned long long s_numbers[64];
    static char s_paths[64][QUERY_PATH_LONGEST];
    const unsigned int count = query_runs_each(folder, s_numbers, s_paths, 64u);
    CliLine line = {out, room, 0ull};
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(&line, (at > 0u) ? "," : "");
        cli_line_decimal(&line, s_numbers[at], 1u);
    }
    (void)cli_line_closed(&line);
    return out;
}

// runs of a folder are numbered newest first, and the keeper removes all but the newest and the restored
static void run_folders(void)
{
    char folder[QUERY_PATH_LONGEST];
    char path[QUERY_PATH_LONGEST];
    char numbers[256];
    run_folder("build/query_test/each", folder);
    static const char *const s_names[4] = {"query3-20260101T000000.qry", "query12-20260102T000000.qry",
                                           "queryx-bad.qry", "other.txt"};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        cli_full(folder, s_names[at], path);
        (void)query_file_written(path, "", 0ull);
    }
    cli_full(folder, "query7-20260101T000001", path);
    (void)query_folder_made(path);
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = run_numbers(folder, numbers, sizeof(numbers)), .other = "12,7,3", .cap = CLI_CELLS_POOL}),
              "runs of a folder are numbered newest first");
    char next[QUERY_PATH_LONGEST];
    query_next_name(folder, next, sizeof(next));
    cli_check(cellul.starts(&(CatenaFinitaCfg){.src = next, .other = "query13-", .cap = CLI_CELLS_POOL}),
              "the next is numbered after the newest");
    (void)query_removed(folder);
    run_folder("build/query_test/kept", folder);
    cli_full(folder, "restored", path);
    (void)query_folder_made(path);
    for (unsigned int n = 1u; n <= 5u; n += 1u)
    {
        char name[64];
        CliLine line = {name, sizeof(name), 0ull};
        cli_line_text(&line, "query");
        cli_line_decimal(&line, n, 1u);
        cli_line_text(&line, "-20260101T00000");
        cli_line_decimal(&line, n, 1u);
        cli_line_text(&line, ".qry");
        (void)cli_line_closed(&line);
        cli_full(folder, name, path);
        (void)query_file_written(path, "", 0ull);
    }
    query_kept(folder, 2u);
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = run_numbers(folder, numbers, sizeof(numbers)), .other = "5,4", .cap = CLI_CELLS_POOL}),
              "the keeper keeps the newest");
    cli_full(folder, "restored", path);
    cli_check(!query_is_folder(path), "and the restored go");
    (void)query_removed(folder);
}

// a folder's runs listed are kept counted to the query, and given back the query holds none of them, its mark standing
static void run_cells(void)
{
    char folder[QUERY_PATH_LONGEST];
    char path[QUERY_PATH_LONGEST];
    run_folder("build/query_test/cells", folder);
    cli_full(folder, "query1-20260101T000000.qry", path);
    (void)query_file_written(path, "", 0ull);
    const unsigned long long before = g_query_cells.holds;
    QueryRuns runs;
    const int taken = query_runs_taken(folder, &runs);
    const unsigned long long holding = g_query_cells.holds;
    query_runs_given(&runs);
    cli_check(taken && (runs.count == 0u) && (holding > before) && (g_query_cells.holds == before) &&
                  (g_query_cells.hw >= holding),
              "a folder's runs are counted to the query while held, and its count falls back as they are given");
    (void)query_removed(folder);
}

// the crystal's program is given where it is built; and a path is named under the top of the tree
static void run_names(void)
{
    char root[QUERY_PATH_LONGEST];
    char engine[QUERY_PATH_LONGEST];
    char name[QUERY_PATH_LONGEST];
    char program[QUERY_PATH_LONGEST];
    char found[QUERY_PATH_LONGEST];
    run_folder("build/query_test/crystal", root);
    cli_check(!query_crystal(root, found, sizeof(found)), "no crystal where none is built");
    cli_full(root, "build/qry_crystal/engine", engine);
    (void)query_folder_made(engine);
    query_binary_name("qry_crystal", name, sizeof(name));
    cli_full(engine, name, program);
    (void)query_file_written(program, "", 0ull);
    cli_check(query_crystal(root, found, sizeof(found)) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = found, .other = program, .cap = CLI_CELLS_POOL})),
              "the crystal's program is given where it is built");
    (void)query_removed(root);
    char path[QUERY_PATH_LONGEST];
    cli_full(s_root, "src/cu/transpiler/lstar/protocol/table/sm_86.khw", path);
    query_named(s_root, path, name, sizeof(name));
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = name, .other = "src/cu/transpiler/lstar/protocol/table/sm_86.khw", .cap = CLI_CELLS_POOL}),
              "a path is named under the top of the tree");
    // a path outside the tree on each system: a drive's on Windows, the root's elsewhere
#if defined(_WIN32)
    static const char s_outside[] = "C:/elsewhere/forms.txt";
#else
    static const char s_outside[] = "/elsewhere/forms.txt";
#endif
    query_named(s_root, s_outside, name, sizeof(name));
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "forms.txt", .cap = CLI_CELLS_POOL}),
              "and one outside it by its name");
}

// a run started in the folder `under`: 1, or 0
static int run_started(QueryRun *run, const char *under)
{
    char folder[QUERY_PATH_LONGEST];
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    cli_full(s_root, under, folder);
    return query_run_started(run, s_root, folder, &s_programs, &s_context, &said);
}

// the run ended, every file left as it leaves it
static void run_ended(QueryRun *run)
{
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    (void)query_run_ended(run, QUERY_LEAVE_ALL, query_terminal(), &s_context, &said);
}

// whether the latest record `name` of the run holds `text`
static int run_latest_is(const QueryRun *run, const char *name, const char *text)
{
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    return query_writer_latest(&run->writer, name, &bytes, &size) &&
           (size == cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL})) &&
           (memor.cmp(&(MemoriaCfg){.src = bytes, .other = text, .bytes = size}) == 0);
}

// a failed loop still stops its writer; and a step joins the run kept open and its end leaves the writer running
static void run_writers(void)
{
    char folder[QUERY_PATH_LONGEST];
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    run_folder("build/query_test/failed", folder);
    cli_check(run_started(&s_run, "build/query_test/failed/qry") && !s_run.joined && query_writer_runs(&s_run.writer),
              "a run starts its writer");
    (void)query_run_stopped(&s_run, &said);
    cli_check(!query_writer_runs(&s_run.writer) && query_is_file(s_run.writer.qry),
              "a failed loop still stops its writer, its run kept");
    (void)query_removed(folder);
    run_folder("build/query_test/joining", folder);
    cli_check(run_started(&s_run, "build/query_test/joining/qry"), "a run is held open");
    said.at = 0ull;
    cli_check(
        query_run_joining(&s_step, s_root, s_run.writer.qry, &s_programs, &said) && s_step.joined &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_step.writer.qry, .other = s_run.writer.qry, .cap = CLI_CELLS_POOL})),
        "a step joins it");
    said.at = 0ull;
    (void)query_writer_append(&s_step.writer, "step.console", "a step's line\n", 14ull, QRY_ORDINARY, &said);
    said.at = 0ull;
    (void)query_run_stopped(&s_step, &said);
    char line[256];
    CliLine joined = {line, sizeof(line), 0ull};
    cli_check(query_writer_runs(&s_run.writer) && query_writer_read(&s_run.writer, "step.console", &joined),
              "the step's end leaves the writer running");
    cli_check_text(line, joined.at, "a step's line\n", "and holding its line");
    said.at = 0ull;
    (void)query_run_stopped(&s_run, &said);
    said.at = 0ull;
    cli_check(!query_run_joining(&s_step, s_root, s_run.writer.qry, &s_programs, &said),
              "no step joins a run whose writer has stopped");
    (void)query_removed(folder);
}

// carried takes a name from the latest run that holds it, else the disk
static void run_carried(void)
{
    char base[QUERY_PATH_LONGEST];
    char folder[QUERY_PATH_LONGEST];
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    run_folder("build/query_test/carried", base);
    cli_full(base, "qry", folder);
    cli_check(run_started(&s_run, "build/query_test/carried/qry"), "a run starts");
    (void)query_writer_append(&s_run.writer, "carried_check_forms.txt", "from the run before\n", 20ull, QRY_ORDINARY,
                              &said);
    run_ended(&s_run);
    char faulted[QUERY_PATH_LONGEST];
    cli_full(base, "carried_check_faulted.txt", faulted);
    (void)query_file_written(faulted, "from the disk\n", 14ull);
    cli_check(run_started(&s_run, "build/query_test/carried/qry"), "and another");
    char files[3][QUERY_PATH_LONGEST];
    static const char *const s_names[3] = {"carried_check_forms.txt", "carried_check_faulted.txt",
                                           "carried_check_nowhere.txt"};
    const char *paths[3];
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        cli_full(base, s_names[at], files[at]);
        paths[at] = files[at];
    }
    said.at = 0ull;
    cli_check(query_run_carried(&s_run, folder, paths, 3u, NULL, &said), "names are carried");
    cli_check(run_latest_is(&s_run, "carried_check_forms.txt", "from the run before\n"),
              "from the latest run that holds it");
    cli_check(run_latest_is(&s_run, "carried_check_faulted.txt", "from the disk\n"), "else the disk");
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    cli_check(!query_writer_latest(&s_run.writer, "carried_check_nowhere.txt", &bytes, &size), "and none from nowhere");
    run_ended(&s_run);
    (void)query_removed(base);
}

// a file read is handed under its path and remembered as a leaf
static void run_read(void)
{
    char base[QUERY_PATH_LONGEST];
    char file[QUERY_PATH_LONGEST];
    char absent[QUERY_PATH_LONGEST];
    char left[QUERY_PATH_LONGEST];
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    run_folder("build/query_test/read", base);
    cli_full(base, "sm_86.khw", file);
    cli_full(base, "absent.khw", absent);
    cli_full(base, "left.txt", left);
    (void)query_file_written(file, "forms\n", 6ull);
    const unsigned long long before = g_query_cells.holds;
    cli_check(run_started(&s_run, "build/query_test/read/qry"), "a run starts");
    const char *const read[2] = {file, absent};
    cli_check(query_run_read(&s_run, read, 2u, &said) &&
                  run_latest_is(&s_run, "build/query_test/read/sm_86.khw", "forms\n"),
              "a file read is handed under its path");
    cli_check((s_run.read.count == 1u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_run.read.path[0], .other = file, .cap = CLI_CELLS_POOL})),
              "and remembered as a leaf, one not there not");
    const char *const lefts[2] = {file, left};
    query_run_left(&s_run, lefts, 2u);
    cli_check((s_run.left.count == 2u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_run.left.path[1], .other = left, .cap = CLI_CELLS_POOL})),
              "a file left is remembered too");
    cli_check((s_run.read.room == QUERY_PATHS_FIRST) && (s_run.left.room == QUERY_PATHS_FIRST) &&
                  (g_query_cells.holds >= before + (2ull * QUERY_PATHS_FIRST * QUERY_PATH_LONGEST)),
              "the files read and left are held in cells of the query's, a first room each");
    said.at = 0ull;
    (void)query_writer_append(&s_run.writer, "build/query_test/read/sm_86.khw", "forms an earlier step wrote\n", 28ull,
                              QRY_ORDINARY, &said);
    (void)query_file_written(file, "forms on the disk\n", 18ull);
    const char *const again[1] = {file};
    said.at = 0ull;
    cli_check(query_run_read(&s_run, again, 1u, &said) &&
                  run_latest_is(&s_run, "build/query_test/read/sm_86.khw", "forms an earlier step wrote\n"),
              "a later step reads the run's own, not the disk's older copy");
    run_ended(&s_run);
    cli_check((s_run.read.path == NULL) && (s_run.left.path == NULL) && (g_query_cells.holds == before),
              "and the run's end gives their cells back");
    (void)query_removed(base);
}

int main(void)
{
    char found[CLI_PATH_LONGEST];
    if (!cli_tree_found(found))
    {
        cli_check(1, "no tree is found, and no run is made");
        return cli_checked();
    }
    memor.cpy(&(MemoriaCfg){.dst = s_root,
                            .src = found,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = found, .cap = CLI_CELLS_POOL}) + 1u});
    for (char *at = s_root; at[0] != '\0'; at += 1)
    {
        at[0] = (at[0] == '\\') ? '/' : at[0];
    }
    s_programs = query_on_path();
    s_context = (QueryContext){s_root, &s_programs, {run_said, NULL}, QUERY_LEAVE_ALL, query_terminal(), {{0}}, 0u, ""};
    run_folders();
    run_cells();
    run_names();
    char compiler[QUERY_PATH_LONGEST];
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    if (!s_programs.program(s_programs.context, QUERY_ROLE_C, compiler, sizeof(compiler), &said))
    {
        cli_check(1, "no C compiler to build the writer with");
        return cli_checked();
    }
    run_writers();
    run_carried();
    run_read();
    return cli_checked();
}
