// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_writer_test.c: the query's programs and the writer's words: every role has a name a person reads; the C
// compiler is found on the PATH here; a program beside nvcc is looked for beside it; what is not found is said by its
// name; a run is started, handed, read and stopped; and a program's output is teed into the run
#include "../../../../../ui/query/src/tools/tools_internal.h"
#include "cli_check.h"

static char s_root[QUERY_PATH_LONGEST];
static QueryPrograms s_programs;

// a line said, let go
static void writer_said(void *context, const char *text, unsigned long long length)
{
    (void)context;
    (void)text;
    (void)length;
}

// the folder of `path`, its last part cut, into `folder`, which holds QUERY_PATH_LONGEST
static void writer_folder_of(const char *path, char *folder)
{
    memor.cpy(&(MemoriaCfg){
        .dst = folder, .src = path, .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    char *cut = folder + cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL});
    while ((cut > folder) && (cut[-1] != '/') && (cut[-1] != '\\'))
    {
        cut -= 1;
    }
    cut[(cut > folder) ? -1 : 0] = '\0';
}

// nothing is found, each said by its name
static int writer_nothing(void *context, QueryRole role, char *path, size_t room, CliLine *said)
{
    (void)context;
    (void)room;
    path[0] = '\0';
    cli_line_text(said, query_role_name(role));
    cli_line_text(said, " is not found");
    return 0;
}

// every role has a name a person reads; the C compiler is found here; a program beside nvcc is looked for beside it;
// and what is not found is said by its name
static void writer_roles(void)
{
    int named = 1;
    for (unsigned int role = 0u; role < (unsigned int)QUERY_ROLE_COUNT; role += 1u)
    {
        const char *const name = query_role_name((QueryRole)role);
        named = named && (name != NULL) && (name[0] != '\0') &&
                (cellul.find(&(CatenaFinitaCfg){
                     .src = name, .cap = CLI_CELLS_POOL, .other = "Role", .other_cap = CLI_CELLS_POOL}) == NULL);
    }
    cli_check(named, "every role has a name a person reads");
    char path[QUERY_PATH_LONGEST];
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    const int c = s_programs.program(s_programs.context, QUERY_ROLE_C, path, sizeof(path), &said);
    (void)cli_line_closed(&said);
    cli_check(
        c ? query_is_file(path)
          : (cellul.find(&(CatenaFinitaCfg){
                 .src = why, .cap = CLI_CELLS_POOL, .other = "the C compiler", .other_cap = CLI_CELLS_POOL}) != NULL),
        "the C compiler is found on the PATH here, or said not to be");
    char nvcc[QUERY_PATH_LONGEST];
    said.at = 0ull;
    if (s_programs.program(s_programs.context, QUERY_ROLE_NVCC, nvcc, sizeof(nvcc), &said))
    {
        char nvdisasm[QUERY_PATH_LONGEST];
        char one[QUERY_PATH_LONGEST];
        char other[QUERY_PATH_LONGEST];
        said.at = 0ull;
        const int found =
            s_programs.program(s_programs.context, QUERY_ROLE_NVDISASM, nvdisasm, sizeof(nvdisasm), &said);
        writer_folder_of(nvcc, one);
        writer_folder_of(nvdisasm, other);
        cli_check(found && (cellul.eq(&(CatenaFinitaCfg){.src = one, .other = other, .cap = CLI_CELLS_POOL})),
                  "a program beside nvcc is looked for beside it");
    }
    const QueryPrograms nothing = {writer_nothing, NULL, NULL};
    said.at = 0ull;
    cli_check(!nothing.program(nothing.context, QUERY_ROLE_PYTHON, path, sizeof(path), &said) &&
                  (cli_line_closed(&said) != 0ull) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = why, .other = "python is not found", .cap = CLI_CELLS_POOL})),
              "what is not found is said by its name");
}

// a writer built into build/query_test/`name`, its run's .qry there: 1, or 0 where there is no C compiler
static int writer_in(const char *name, char *out, QueryWriter *writer)
{
    char compiler[QUERY_PATH_LONGEST];
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    if (!s_programs.program(s_programs.context, QUERY_ROLE_C, compiler, sizeof(compiler), &said))
    {
        return 0;
    }
    char under[QUERY_PATH_LONGEST];
    CliLine line = {under, sizeof(under), 0ull};
    cli_line_text(&line, "build/query_test/");
    cli_line_text(&line, name);
    (void)cli_line_closed(&line);
    cli_full(s_root, under, out);
    (void)query_removed(out);
    (void)query_folder_made(out);
    memor.set(&(MemoriaCfg){.dst = writer, .val = 0, .bytes = sizeof(*writer)});
    said.at = 0ull;
    cli_check(
        query_tool_built_into(s_root, &s_programs, "qry_writer", out, writer->binary, sizeof(writer->binary), &said),
        "the writer is built");
    cli_full(out, "query1-test.qry", writer->qry);
    memor.cpy(&(MemoriaCfg){.dst = writer->top,
                            .src = s_root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_root, .cap = CLI_CELLS_POOL}) + 1u});
    writer->path = s_programs.run_path(s_programs.context);
    return 1;
}

// whether the latest record `name` of the run holds `text`
static int writer_latest_is(const QueryWriter *writer, const char *name, const char *text)
{
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    return query_writer_latest(writer, name, &bytes, &size) &&
           (size == cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL})) &&
           (memor.cmp(&(MemoriaCfg){.src = bytes, .other = text, .bytes = size}) == 0);
}

// a run is started, handed, read and stopped
static void writer_run(void)
{
    static QueryWriter s_writer;
    char out[QUERY_PATH_LONGEST];
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    if (!writer_in("writer", out, &s_writer))
    {
        cli_check(1, "no C compiler to build the writer with");
        return;
    }
    cli_check(!query_writer_runs(&s_writer), "no writer runs before it is started");
    unsigned long long process = 0ull;
    cli_check(query_writer_start(&s_writer, &process, &said), "the writer starts");
    cli_check(query_writer_made(&s_writer) && query_writer_runs(&s_writer), "the writer came ready");
    cli_check(query_writer_joined(&s_writer), "this process is a member of the run");
    said.at = 0ull;
    (void)query_writer_append(&s_writer, "a.txt", "one\ntwo\n", 8ull, QRY_ORDINARY, &said);
    cli_check(writer_latest_is(&s_writer, "a.txt", "one\ntwo\n"), "a record is read back");
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    cli_check(!query_writer_latest(&s_writer, "none.txt", &bytes, &size), "and none that was not handed");
    query_writer_parted(&s_writer);
    said.at = 0ull;
    (void)query_writer_stop(&s_writer, &said);
    QueryProcess waited = {process, 0ull};
    (void)query_process_wait(&waited);
    cli_check(!query_writer_runs(&s_writer), "the writer is stopped");
    cli_check(writer_latest_is(&s_writer, "a.txt", "one\ntwo\n"), "read from the .qry once the writer stopped");
    char joined[64];
    CliLine line = {joined, sizeof(joined), 0ull};
    cli_check(query_writer_read(&s_writer, "a.txt", &line), "and read whole");
    cli_check_text(joined, line.at, "one\ntwo\n", "as handed");
    (void)query_removed(out);
}

// a program's output is teed into the run
static void writer_tee(void)
{
    static QueryWriter s_writer;
    char out[QUERY_PATH_LONGEST];
    char python[QUERY_PATH_LONGEST];
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    if (!writer_in("tee", out, &s_writer) ||
        !s_programs.program(s_programs.context, QUERY_ROLE_PYTHON, python, sizeof(python), &said))
    {
        cli_check(1, "no C compiler or no Python to tee with");
        return;
    }
    unsigned long long process = 0ull;
    said.at = 0ull;
    cli_check(query_writer_start(&s_writer, &process, &said) && query_writer_made(&s_writer) &&
                  query_writer_joined(&s_writer),
              "the writer starts and this process joins it");
    QueryContext context = {s_root, &s_programs, {writer_said, NULL}, QUERY_LEAVE_ALL, query_terminal(), {{0}}, 0u, ""};
    const char *const words[4] = {python, "-c", "import sys; print('out'); print('err', file=sys.stderr); sys.exit(3)",
                                  NULL};
    long long code = 0ll;
    said.at = 0ull;
    cli_check(query_writer_tee(&s_writer, &context, "said", words, &code, &said) && (code == 3ll),
              "a program's exit code comes back");
    static char s_console[4096];
    CliLine console = {s_console, sizeof(s_console), 0ull};
    (void)query_writer_read(&s_writer, "said.console", &console);
    (void)cli_line_closed(&console);
    // the tee hands a piece at a time, a blob each: every blob joined is the console
    cli_check((cellul.find(&(CatenaFinitaCfg){
                   .src = s_console, .cap = CLI_CELLS_POOL, .other = "out", .other_cap = CLI_CELLS_POOL}) != NULL) &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = s_console, .cap = CLI_CELLS_POOL, .other = "err", .other_cap = CLI_CELLS_POOL}) != NULL),
              "its output and its errors are teed into the run");
    query_writer_parted(&s_writer);
    said.at = 0ull;
    (void)query_writer_stop(&s_writer, &said);
    QueryProcess waited = {process, 0ull};
    (void)query_process_wait(&waited);
    (void)query_removed(out);
}

int main(void)
{
    char found[CLI_PATH_LONGEST];
    s_programs = query_on_path();
    writer_roles();
    if (!cli_tree_found(found))
    {
        cli_check(1, "no tree is found, and no writer is built");
        return cli_checked();
    }
    memor.cpy(&(MemoriaCfg){.dst = s_root,
                            .src = found,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = found, .cap = CLI_CELLS_POOL}) + 1u});
    for (char *at = s_root; at[0] != '\0'; at += 1)
    {
        at[0] = (at[0] == '\\') ? '/' : at[0];
    }
    writer_run();
    writer_tee();
    return cli_checked();
}
