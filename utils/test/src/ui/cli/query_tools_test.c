// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_tools_test.c: the query's builds: the compiles pace as the interface's queue does; every pattern of every tool
// reaches a file of the tree; paths given the compilers are forward slashed; no two sources of a tool share an object;
// the file definitions are what every program links; a tool is stale where its binary is missing or older than a
// source; an object older than a header its source reads is compiled again; a missing compiler is said before any
// build; and the writer builds and runs
#include "../../../../../ui/query/src/tools/tools_internal.h"
#include "cli_check.h"

#include "../../../../../ui/cli/src/text/text.h"

static char s_root[QUERY_PATH_LONGEST];
static QueryPaths s_found;
static char s_header[1u << 16u];

// the number the interface's header defines under `name`, read off its `#define` line
static unsigned long long tools_interface_number(const char *name)
{
    char path[QUERY_PATH_LONGEST];
    cli_full(s_root, "src/cu/transpiler/lstar/interface/interface.h", path);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_header, sizeof(s_header) - 1u);
    s_header[(size != QRY_MEMBER_UNREAD) ? size : 0ull] = '\0';
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    for (const char *line = s_header; line[0] != '\0';)
    {
        if ((cellul.starts(&(CatenaFinitaCfg){.src = line, .other = "#define ", .cap = CLI_CELLS_POOL})) &&
            cli_text_same(line + 8, name, length) && ((line[8u + length] == ' ') || (line[8u + length] == '\t')))
        {
            const char *digit = line + 8u + length;
            while ((digit[0] == ' ') || (digit[0] == '\t'))
            {
                digit += 1;
            }
            unsigned long long value = 0ull;
            while ((digit[0] >= '0') && (digit[0] <= '9'))
            {
                value = (value * 10ull) + (unsigned long long)(digit[0] - '0');
                digit += 1;
            }
            return value;
        }
        const char *const end = cellul.chr(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL, .byte = '\n'});
        line = (end != NULL) ? (end + 1) : (line + cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL}));
    }
    cli_check(0, name);
    return 0ull;
}

// whether one pattern names a file under the tree
static int tools_reaches(const char *pattern)
{
    const char *const patterns[2] = {pattern, NULL};
    query_paths_given(&s_found);
    const int reached = query_sources(s_root, patterns, &s_found) > 0u;
    query_paths_given(&s_found);
    return reached;
}

// every pattern of every tool reaches a file of the tree
static void tools_patterns(void)
{
    int reached = 1;
    for (const QueryTool *tool = g_query_tools; tool->name != NULL; tool += 1)
    {
        for (unsigned int group = 0u; group < QUERY_TOOL_GROUPS; group += 1u)
        {
            const char *const *const lists[2] = {tool->c[group], tool->cxx[group]};
            for (unsigned int list = 0u; list < 2u; list += 1u)
            {
                for (unsigned int at = 0u; (lists[list] != NULL) && (lists[list][at] != NULL); at += 1u)
                {
                    const int one = tools_reaches(lists[list][at]);
                    cli_check(one, lists[list][at]);
                    reached = reached && one;
                }
            }
        }
        for (unsigned int at = 0u; (tool->cxx_tool != NULL) && (tool->cxx_tool[at] != NULL); at += 1u)
        {
            const int one = tools_reaches(tool->cxx_tool[at]);
            cli_check(one, tool->cxx_tool[at]);
            reached = reached && one;
        }
    }
    cli_check(reached, "every pattern of every tool reaches a file of the tree");
}

// no two sources of a tool share an object
static void tools_objects(void)
{
    static char s_named[3u * QUERY_SOURCES_MOST][QUERY_PATH_LONGEST];
    int apart = 1;
    for (const QueryTool *tool = g_query_tools; tool->name != NULL; tool += 1)
    {
        unsigned int c = 0u;
        unsigned int cxx = 0u;
        unsigned int tools = 0u;
        query_every_source(s_root, tool, &c, &cxx, &tools);
        unsigned int count = 0u;
        char (*const lists[3])[QUERY_PATH_LONGEST] = {g_query_c_sources.path, g_query_cxx_sources.path,
                                                      g_query_tool_sources.path};
        const unsigned int counts[3] = {c, cxx, tools};
        for (unsigned int list = 0u; list < 3u; list += 1u)
        {
            for (unsigned int at = 0u; at < counts[list]; at += 1u)
            {
                query_object_of(s_root, "objects", lists[list][at], s_named[count]);
                count += 1u;
            }
        }
        int tool_apart = 1;
        for (unsigned int one = 0u; tool_apart && (one < count); one += 1u)
        {
            for (unsigned int other = one + 1u; tool_apart && (other < count); other += 1u)
            {
                tool_apart =
                    !cellul.eq(&(CatenaFinitaCfg){.src = s_named[one], .other = s_named[other], .cap = CLI_CELLS_POOL});
            }
        }
        cli_check(tool_apart, tool->name);
        apart = apart && tool_apart;
        query_every_source_given();
    }
    cli_check(apart, "no two sources of a tool share an object");
}

// the file definitions are what every program links; and paths given the compilers are forward slashed
static void tools_file_defs(void)
{
    const unsigned int count = query_sources(s_root, g_query_file_defs, &s_found);
    int kfile_lines = 0;
    int kdm = 0;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const found = s_found.path[at];
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = found, .cap = CLI_CELLS_POOL});
        kfile_lines =
            kfile_lines ||
            ((length >= 14ull) && (cellul.eq(&(CatenaFinitaCfg){
                                      .src = found + length - 14u, .other = "/kfile_lines.c", .cap = CLI_CELLS_POOL})));
        kdm = kdm ||
              ((length >= 6ull) &&
               (cellul.eq(&(CatenaFinitaCfg){.src = found + length - 6u, .other = "/kdm.c", .cap = CLI_CELLS_POOL})));
    }
    cli_check(kfile_lines && kdm && (count > 10u) && (s_found.room <= (2u * count) + QUERY_PATHS_FIRST),
              "the file definitions are what every program links, listed in a room that grew to them");
    query_paths_given(&s_found);
    char source[QUERY_PATH_LONGEST];
    char objects[QUERY_PATH_LONGEST];
    char object[QUERY_PATH_LONGEST];
    cli_full(s_root, "src/cu/includes/file_defs/kfile_lines/kfile_lines.c", source);
    cli_full(s_root, "build/objects", objects);
    query_object_of(s_root, objects, source, object);
    cli_check((cellul.chr(&(CatenaFinitaCfg){.src = source, .cap = CLI_CELLS_POOL, .byte = '\\'}) == NULL) &&
                  (cellul.chr(&(CatenaFinitaCfg){.src = object, .cap = CLI_CELLS_POOL, .byte = '\\'}) == NULL),
              "paths given the compilers are forward slashed");
}

// a tool is stale where its binary is missing or older than a source; and an object older than a header its source
// reads is compiled again
static void tools_stale(void)
{
    char out[QUERY_PATH_LONGEST];
    char name[QUERY_PATH_LONGEST];
    char binary[QUERY_PATH_LONGEST];
    cli_full(s_root, "build/query_test/stale", out);
    (void)query_removed(out);
    (void)query_folder_made(out);
    query_binary_name("orior_qry_writer", name, sizeof(name));
    cli_full(out, name, binary);
    const QueryTool *const tool = query_tool_named("orior_qry_writer");
    cli_check((tool != NULL) && query_stale(s_root, tool, binary), "a missing binary is stale");
    (void)query_file_written(binary, "", 0ull);
    cli_check((tool != NULL) && !query_stale(s_root, tool, binary), "a binary newer than every source stands");
    cli_check(query_modified_set(binary, 86400ull) && (tool != NULL) && query_stale(s_root, tool, binary),
              "a binary older than its sources is stale");
    (void)query_removed(out);
    cli_check(query_fresh(1, 10ull, 1, 20ull, 5ull), "an object newer than its source and every header stands");
    cli_check(!query_fresh(1, 10ull, 1, 20ull, 30ull), "a header newer than the object compiles it again");
    cli_check(!query_fresh(1, 30ull, 1, 20ull, 0ull), "a source newer than the object compiles it again");
    cli_check(!query_fresh(1, 10ull, 0, 0ull, 0ull), "no object is compiled");
    cli_check(query_fresh(1, 10ull, 1, 20ull, 0ull), "no header newer stands");
}

// no program is found, each said as the window says it
static int tools_none(void *context, QueryRole role, char *path, size_t room, CliLine *said)
{
    (void)context;
    (void)room;
    path[0] = '\0';
    cli_line_text(said, query_role_name(role));
    cli_line_text(said, " is not found: File, Toolchains opens its install page");
    return 0;
}

// no PATH beyond the process's own
static const char *tools_no_path(void *context)
{
    (void)context;
    return "";
}

// a missing compiler is said before any build; and the writer builds and runs
static void tools_built(void)
{
    char out[QUERY_PATH_LONGEST];
    char binary[QUERY_PATH_LONGEST];
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    const QueryPrograms none = {tools_none, tools_no_path, NULL};
    cli_full(s_root, "build/query_test/noc", out);
    (void)query_removed(out);
    cli_check(!query_tool_built_into(s_root, &none, "orior_sass_listing_seeds", out, binary, sizeof(binary), &said) &&
                  (cli_line_closed(&said) != 0ull) &&
                  (cellul.starts(
                      &(CatenaFinitaCfg){.src = why, .other = "the C compiler is not found", .cap = CLI_CELLS_POOL})),
              "a missing compiler is said before any build");
    char name[QUERY_PATH_LONGEST];
    char path[QUERY_PATH_LONGEST];
    query_binary_name("orior_sass_listing_seeds", name, sizeof(name));
    cli_full(out, name, path);
    cli_check(!query_is_file(path), "and nothing is built");
    const QueryPrograms programs = query_on_path();
    char compiler[QUERY_PATH_LONGEST];
    said.at = 0ull;
    if (!programs.program(programs.context, QUERY_ROLE_C, compiler, sizeof(compiler), &said))
    {
        cli_check(1, "no C compiler to build the writer with");
        return;
    }
    cli_full(s_root, "build/query_test/tools", out);
    said.at = 0ull;
    cli_check(query_tool_built_into(s_root, &programs, "orior_qry_writer", out, binary, sizeof(binary), &said),
              "the writer builds");
    const char *const words[2] = {binary, NULL};
    CliRan *const ran = cli_ran_taken();
    said.at = 0ull;
    cli_check(cli_ran(words, NULL, NULL, 0u, ran, &said) && (ran->code == 2ll),
              "orior_qry_writer with no words says its usage and exits 2");
    cli_ran_given(ran);
    const QueryTool *const tool = query_tool_named("orior_qry_writer");
    cli_check((tool != NULL) && !query_stale(s_root, tool, binary), "built, it stands");
}

int main(void)
{
    char found[CLI_PATH_LONGEST];
    if (!cli_tree_found(found))
    {
        cli_check(1, "no tree is found, and its tools are not read");
        return cli_checked();
    }
    memor.cpy(&(MemoriaCfg){.dst = s_root,
                            .src = found,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = found, .cap = CLI_CELLS_POOL}) + 1u});
    for (char *at = s_root; at[0] != '\0'; at += 1)
    {
        at[0] = (at[0] == '\\') ? '/' : at[0];
    }
    cli_check(QUERY_LAUNCHES_AT_ONCE == tools_interface_number("INTERFACE_LAUNCHES_AT_ONCE"),
              "the compiles run as many at once as the interface's queue");
    cli_check(QUERY_LAUNCH_SPACING_MILLISECONDS == tools_interface_number("INTERFACE_LAUNCH_SPACING_MILLISECONDS"),
              "and as far apart");
    tools_patterns();
    tools_objects();
    tools_file_defs();
    tools_stale();
    tools_built();
    return cli_checked();
}
