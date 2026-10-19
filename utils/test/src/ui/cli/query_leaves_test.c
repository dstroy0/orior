// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_leaves_test.c: what a run leaves, asked before it is written: a diff names what changed and keeps the rest
// short; a diff of the same lines is all the same; a record file is shown by its counts; and a refused file is handed
// back as the tree holds it
#include "../../../../../ui/query/src/query_internal.h"
#include "cli_check.h"

static char s_root[QUERY_PATH_LONGEST];
static char s_shown[1u << 16u];
static char s_heard[4096];
static QueryRun s_run;

// `tree` against `held` shown as the run's leaf `path`, into s_shown
static const char *leaves_shown(const char *path, const char *tree, const char *held)
{
    CliLine shown = {s_shown, sizeof(s_shown), 0ull};
    query_leaf_shown(path, (const unsigned char *)tree,
                     cellul.len(&(CatenaFinitaCfg){.src = tree, .cap = CLI_CELLS_POOL}), (const unsigned char *)held,
                     cellul.len(&(CatenaFinitaCfg){.src = held, .cap = CLI_CELLS_POOL}), &shown);
    (void)cli_line_closed(&shown);
    return s_shown;
}

// the lines of s_shown led by `lead`, and every line
static unsigned int leaves_lines(char lead, unsigned int *all)
{
    unsigned int count = 0u;
    *all = 0u;
    for (const char *line = s_shown; line[0] != '\0';)
    {
        count += (line[0] == lead) ? 1u : 0u;
        *all += 1u;
        const char *const end = cellul.chr(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL, .byte = '\n'});
        line = (end != NULL) ? (end + 1) : (line + cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL}));
    }
    return count;
}

// a diff names what changed and keeps the rest short; a diff of the same lines is all the same; and a record file is
// shown by its counts
static void leaves_diffs(void)
{
    char before[256];
    char after[256];
    CliLine one = {before, sizeof(before), 0ull};
    CliLine two = {after, sizeof(after), 0ull};
    for (unsigned int at = 0u; at < 20u; at += 1u)
    {
        cli_line_text(&one, "same\n");
        cli_line_text(&two, (at == 10u) ? "changed\n" : "same\n");
    }
    (void)cli_line_closed(&one);
    (void)cli_line_closed(&two);
    (void)leaves_shown("a.txt", before, after);
    unsigned int all = 0u;
    cli_check((leaves_lines('-', &all) == 1u) && (leaves_lines('+', &all) == 1u), "a diff names what changed");
    cli_check(
        (cellul.find(&(CatenaFinitaCfg){
             .src = s_shown, .cap = CLI_CELLS_POOL, .other = "-same", .other_cap = CLI_CELLS_POOL}) != NULL) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = s_shown, .cap = CLI_CELLS_POOL, .other = "+changed", .other_cap = CLI_CELLS_POOL}) != NULL),
        "the line taken and given");
    cli_check((cellul.find(&(CatenaFinitaCfg){.src = s_shown,
                                              .cap = CLI_CELLS_POOL,
                                              .other = "... 4 lines the same ...",
                                              .other_cap = CLI_CELLS_POOL}) != NULL) &&
                  (all <= 17u),
              "each unchanged run folded to its first three, a count and its last three");
    (void)leaves_shown("a.txt", "a\nb\nc\n", "a\nb\nc\n");
    cli_check((leaves_lines('-', &all) == 0u) && (leaves_lines('+', &all) == 0u),
              "a diff of the same lines is all the same");
    (void)leaves_shown("a.txt", "", "a\n");
    cli_check(cellul.find(&(CatenaFinitaCfg){
                  .src = s_shown, .cap = CLI_CELLS_POOL, .other = "+a", .other_cap = CLI_CELLS_POOL}) != NULL,
              "a line added to nothing");
    (void)leaves_shown("a.txt", "a\n", "");
    cli_check(cellul.find(&(CatenaFinitaCfg){
                  .src = s_shown, .cap = CLI_CELLS_POOL, .other = "-a", .other_cap = CLI_CELLS_POOL}) != NULL,
              "and one taken to nothing");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = leaves_shown("a/sm_86.kqr", "12345", "1234567"),
                                           .other = "sm_86.kqr grew by 2 bytes",
                                           .cap = CLI_CELLS_POOL}),
              "a record file that grew");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = leaves_shown("a/sm_86.kqr", "1234567", "12345"),
                                           .other = "sm_86.kqr: 7 bytes in the tree, 5 as the run leaves it",
                                           .cap = CLI_CELLS_POOL}),
              "and one that did not");
}

// what is said, kept joined by lines
static void leaves_said(void *context, const char *text, unsigned long long length)
{
    (void)context;
    CliLine line = {s_heard, sizeof(s_heard), cellul.len(&(CatenaFinitaCfg){.src = s_heard, .cap = CLI_CELLS_POOL})};
    cli_line_counted(&line, text, length);
    cli_line_character(&line, '\n');
    (void)cli_line_closed(&line);
}

// the answers given in turn
static QueryAnswer leaves_says(void *context, const char *path)
{
    (void)path;
    unsigned int *const asked = context;
    static const QueryAnswer s_answers[2] = {QUERY_ANSWER_NO, QUERY_ANSWER_ALL};
    const QueryAnswer answer = s_answers[(*asked < 2u) ? *asked : 1u];
    *asked += 1u;
    return answer;
}

// whether the run's current file at `path` holds `text`
static int leaves_current_is(const char *path, const char *text)
{
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    return query_writer_current(&s_run.writer, path, &bytes, &size) &&
           (size == cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL})) &&
           (memor.cmp(&(MemoriaCfg){.src = bytes, .other = text, .bytes = size}) == 0);
}

// a refused file is handed back as the tree holds it
static void leaves_refused(void)
{
    QueryPrograms programs = query_on_path();
    char compiler[QUERY_PATH_LONGEST];
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    if (!programs.program(programs.context, QUERY_ROLE_C, compiler, sizeof(compiler), &said))
    {
        cli_check(1, "no C compiler to build the writer with");
        return;
    }
    char base[QUERY_PATH_LONGEST];
    char folder[QUERY_PATH_LONGEST];
    char files[3][QUERY_PATH_LONGEST];
    static const char *const s_names[3] = {"a.khw", "b.krs", "c.krs"};
    static const char *const s_tree[3] = {"tree", "tree", "same"};
    static const char *const s_left[3] = {"run", "run", "same"};
    cli_full(s_root, "build/query_test/leaves", base);
    (void)query_removed(base);
    (void)query_folder_made(base);
    cli_full(base, "qry", folder);
    unsigned int asked = 0u;
    const QueryAsker asker = {leaves_says, &asked};
    QueryContext context = {s_root, &programs, {leaves_said, NULL}, QUERY_LEAVE_ASK, asker, {{0}}, 0u, ""};
    said.at = 0ull;
    cli_check(query_run_started(&s_run, s_root, folder, &programs, &context, &said), "a run starts");
    const char *paths[3];
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        cli_full(base, s_names[at], files[at]);
        paths[at] = files[at];
        (void)query_file_written(files[at], s_tree[at],
                                 cellul.len(&(CatenaFinitaCfg){.src = s_tree[at], .cap = CLI_CELLS_POOL}));
        said.at = 0ull;
        (void)query_writer_file(&s_run.writer, files[at], s_left[at],
                                cellul.len(&(CatenaFinitaCfg){.src = s_left[at], .cap = CLI_CELLS_POOL}), &said);
    }
    query_run_left(&s_run, paths, 3u);
    s_heard[0] = '\0';
    said.at = 0ull;
    cli_check(query_leaves_settled(&s_run, QUERY_LEAVE_ASK, asker, &context, &said), "the leaves are settled");
    cli_check(leaves_current_is(files[0], "tree"), "a refused file is handed back as the tree holds it");
    cli_check(leaves_current_is(files[1], "run"), "and one after all is asked is left as the run leaves it");
    cli_check((cellul.find(&(CatenaFinitaCfg){
                   .src = s_heard, .cap = CLI_CELLS_POOL, .other = "a.khw:", .other_cap = CLI_CELLS_POOL}) != NULL) &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = s_heard, .cap = CLI_CELLS_POOL, .other = "b.krs:", .other_cap = CLI_CELLS_POOL}) != NULL),
              "each changed file is shown");
    cli_check(cellul.find(&(CatenaFinitaCfg){
                  .src = s_heard, .cap = CLI_CELLS_POOL, .other = "c.krs", .other_cap = CLI_CELLS_POOL}) == NULL,
              "a file the same is passed over");
    said.at = 0ull;
    (void)query_run_stopped(&s_run, &said);
    (void)query_removed(base);
}

int main(void)
{
    leaves_diffs();
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
    leaves_refused();
    return cli_checked();
}
