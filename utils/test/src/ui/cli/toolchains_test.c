// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// toolchains_test.c: the toolchains' check: a login shell's PATH is read and a shell that says nothing gives none; the
// jobs need git, bash and python; a tree uses only the tools its files call for and those; what each search found is
// kept and taken by the next, and a PATH that changed searches again; the manifest reads and each
// id is one tool; the reader adds and takes out toolchains and groups; a star matches any run and versions sort as
// numbers; a folder already on the PATH is not added twice; a tool in a chosen folder is found there first, orior's own
// folder a folder of the run's; and the PATH a step runs with begins with the folders put ahead of it, each once
#include "../../../../../ui/cli/src/toolchains/toolchains_internal.h"
#include "cli_check.h"

#if defined(_WIN32)
#define TOOLCHAINS_SEPARATOR ";"
#define TOOLCHAINS_GHDL "ghdl.exe"
#else
#define TOOLCHAINS_SEPARATOR ":"
#define TOOLCHAINS_GHDL "ghdl"
#endif

#if !defined(_WIN32)
// a login shell's PATH is read and a shell that says nothing gives none
static void toolchains_shell(void)
{
    static const char s_shell[] = "#!/bin/sh\nprintf '%s' /opt/from/profile:/usr/bin\n";
    static const char s_quiet[] = "#!/bin/sh\nexit 0\n";
    char dir[CLI_PATH_LONGEST];
    char shell[CLI_PATH_LONGEST];
    char quiet[CLI_PATH_LONGEST];
    cli_check_folder("toolchains_shell", dir);
    cli_full(dir, "shell", shell);
    cli_full(dir, "quiet", quiet);
    cli_check(query_file_written(shell, s_shell, sizeof(s_shell) - 1u) &&
                  query_file_written(quiet, s_quiet, sizeof(s_quiet) - 1u) && cli_file_runnable(shell) &&
                  cli_file_runnable(quiet),
              "two shells are written");
    char path[CLI_PATH_LONGEST];
    cli_check(
        cli_toolchains_path_of_shell(shell, path, sizeof(path)) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = path, .other = "/opt/from/profile:/usr/bin", .cap = CLI_CELLS_POOL})),
        "a login shell's PATH is read");
    cli_check(!cli_toolchains_path_of_shell(quiet, path, sizeof(path)), "and a shell that says nothing gives none");
}
#endif

// the jobs need git, bash and python
static void toolchains_needed(void)
{
    CliJson json;
    const unsigned int manifest = cli_toolchains_manifest(&json, 0u, 0ull);
    char needed[256];
    CliLine line = {needed, sizeof(needed), 0ull};
    for (unsigned int tool = cli_json_is(&json, manifest, CFG_JSON_ARRAY) ? json.node[manifest].first : CLI_JSON_NONE;
         tool != CLI_JSON_NONE; tool = json.node[tool].next)
    {
        if (cli_json_is(&json, cli_json_member(&json, tool, "needed"), CFG_JSON_TRUE))
        {
            const char *const id = cli_json_string(&json, cli_json_member(&json, tool, "id"), NULL);
            cli_line_text(&line, (line.at > 0ull) ? "|" : "");
            cli_line_text(&line, (id != NULL) ? id : "");
        }
    }
    cli_json_given(&json);
    cli_check_text(needed, line.at, "git|bash|python", "the jobs need git, bash and python");
}

// whether a tree of Python files alone holds a file of the name or the ending `kind`
static int toolchains_seen(const void *context, const char *kind)
{
    (void)context;
    return cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = ".py", .cap = CLI_CELLS_POOL});
}

// whether the check said tool `id` of its array `tools` is used
static int toolchains_used(const CliJson *json, unsigned int tools, const char *id)
{
    for (unsigned int tool = json->node[tools].first; tool != CLI_JSON_NONE; tool = json->node[tool].next)
    {
        const char *const named = cli_json_string(json, cli_json_member(json, tool, "id"), NULL);
        if ((named != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = named, .other = id, .cap = CLI_CELLS_POOL})))
        {
            return cli_json_is(json, cli_json_member(json, tool, "used"), CFG_JSON_TRUE);
        }
    }
    return 0;
}

// a tree uses the tools its files' names and endings call for and the ones the jobs need, and no other
static void toolchains_tree_uses(void)
{
    static char s_out[1u << 18u];
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_toolchains_check(0, toolchains_seen, NULL, &out);
    unsigned int tools = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_out, out.at, &tools);
    cli_check((json != NULL) && cli_json_is(json, tools, CFG_JSON_ARRAY) && toolchains_used(json, tools, "black") &&
                  toolchains_used(json, tools, "ruff") && toolchains_used(json, tools, "git"),
              "a tree of Python files uses Python's formatter and checkers, and the tools the jobs need");
    cli_check((json != NULL) && !toolchains_used(json, tools, "lean") && !toolchains_used(json, tools, "rust") &&
                  !toolchains_used(json, tools, "docker"),
              "and no tool whose files it does not hold");
    CliLine none = {s_out, sizeof(s_out), 0ull};
    cli_toolchains_check(0, NULL, NULL, &none);
    const CliJson *const open = cli_check_parsed(s_out, none.at, &tools);
    cli_check((open != NULL) && !toolchains_used(open, tools, "black") && toolchains_used(open, tools, "python"),
              "with no tree open, only the tools the jobs need are used");
}

// what each tool's search found is kept in orior's own folder and taken by the next search, and a PATH that changed
// searches again
static void toolchains_kept(void)
{
    static char s_out[1u << 18u];
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_toolchains_check(0, NULL, NULL, &out);
    char kept[CLI_PATH_LONGEST];
    cli_check(cli_toolchains_file("toolchains_found.json", kept) && query_is_file(kept),
              "a check keeps what each tool's search found in orior's own folder");
    unsigned int count = 0u;
    char (*const path)[CLI_PATH_LONGEST] = cli_toolchains_path_taken(&count);
    CliFound found;
    cli_check(cli_toolchains_found_kept("git", (const char (*)[CLI_PATH_LONGEST])path, count, &found),
              "and the next search of a tool takes it");
    static const char s_other[1][CLI_PATH_LONGEST] = {"/a/folder/put/on/the/path"};
    cli_check(!cli_toolchains_found_kept("git", s_other, 1u, &found), "and a PATH that changed searches again");
    cli_cell_given(path);
}

// the manifest reads and each id is one tool, each naming a program and an install page, every page https
static void toolchains_manifest(void)
{
    static char s_ids[512][CLI_NAME_LONGEST];
    CliJson json;
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    // the program's own bytes are toolchains.json's text
    const unsigned int read =
        cli_json_taken(&json, (const char *)g_cli_toolchains_json, g_cli_toolchains_json_size, 0u, 0ull, &said);
    const unsigned int tools = cli_json_member(&json, read, "tools");
    unsigned int count = 0u;
    int once = 1;
    int named = 1;
    int paged = 1;
    for (unsigned int tool = cli_json_is(&json, tools, CFG_JSON_ARRAY) ? json.node[tools].first : CLI_JSON_NONE;
         (tool != CLI_JSON_NONE) && (count < 512u); tool = json.node[tool].next)
    {
        const char *const id = cli_json_string(&json, cli_json_member(&json, tool, "id"), NULL);
        once = once && (id != NULL);
        for (unsigned int at = 0u; once && (at < count); at += 1u)
        {
            once = !cellul.eq(&(CatenaFinitaCfg){.src = s_ids[at], .other = id, .cap = CLI_CELLS_POOL});
        }
        memor.cpy(&(MemoriaCfg){
            .dst = s_ids[count],
            .src = (id != NULL) ? id : "",
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = (id != NULL) ? id : "", .cap = CLI_CELLS_POOL}) + 1u});
        count += 1u;
        const unsigned int programs = cli_json_member(&json, tool, "programs");
        named = named && cli_json_is(&json, programs, CFG_JSON_ARRAY) && (json.node[programs].count > 0u);
        const unsigned int install = cli_json_member(&json, tool, "install");
        paged = paged && cli_json_is(&json, install, CFG_JSON_OBJECT) && (json.node[install].count > 0u);
        for (unsigned int page = cli_json_is(&json, install, CFG_JSON_OBJECT) ? json.node[install].first
                                                                              : CLI_JSON_NONE;
             page != CLI_JSON_NONE; page = json.node[page].next)
        {
            const char *const address = cli_json_string(&json, page, NULL);
            paged = paged && (address != NULL) &&
                    (cellul.starts(&(CatenaFinitaCfg){.src = address, .other = "https://", .cap = CLI_CELLS_POOL}));
        }
    }
    cli_json_given(&json);
    cli_check((count > 0u) && once, "the manifest reads and each id is one tool");
    cli_check(named, "each names a program");
    cli_check(paged, "and has an install page, every page https");
}

// the reader's toolchain `name` in `group`, its program zig, added: 1, its id into `id`, or 0 with why into `why`
static int toolchains_added(const char *tool, char *id, char *why, unsigned long long room)
{
    unsigned int root = CLI_JSON_NONE;
    const CliJson *const json =
        cli_check_parsed(tool, cellul.len(&(CatenaFinitaCfg){.src = tool, .cap = CLI_CELLS_POOL}), &root);
    CliLine said = {why, room, 0ull};
    const int added = cli_toolchains_add(json, root, id, &said);
    (void)cli_line_closed(&said);
    return added;
}

// the groups of the reader's file, joined by `|`, into `out`, which holds `room`
static void toolchains_added_groups(char *out, unsigned long long room)
{
    CliJson json;
    char path[CLI_PATH_LONGEST];
    CliLine line = {out, room, 0ull};
    (void)cli_toolchains_file("user_toolchains.json", path);
    const unsigned int read = cli_toolchains_file_read(&json, path, 0u, 0ull);
    const unsigned int groups = cli_json_member(&json, read, "groups");
    for (unsigned int item = cli_json_is(&json, groups, CFG_JSON_ARRAY) ? json.node[groups].first : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json.node[item].next)
    {
        const char *const group = cli_json_string(&json, item, NULL);
        cli_line_text(&line, (line.at > 0ull) ? "|" : "");
        cli_line_text(&line, (group != NULL) ? group : "");
    }
    (void)cli_line_closed(&line);
    cli_json_given(&json);
}

// a group put after the others, joined by `|`
static void toolchains_group_kept(void *context, const char *group)
{
    CliLine *const out = context;
    cli_line_text(out, (out->at > 0ull) ? "|" : "");
    cli_line_text(out, group);
}

// a reader adds and takes out toolchains and groups
static void toolchains_reader(void)
{
    static const char s_zig[] =
        "{\"id\":\"\",\"name\":\"Zig 0.13\",\"group\":\"Systems\",\"for\":\"\",\"programs\":[\"zig\"]}";
    static const char s_git[] =
        "{\"id\":\"\",\"name\":\"Git\",\"group\":\"Systems\",\"for\":\"\",\"programs\":[\"zig\"]}";
    char id[CLI_NAME_LONGEST];
    char why[1024];
    cli_check(toolchains_added(s_zig, id, why, sizeof(why)) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = id, .other = "zig-0-13", .cap = CLI_CELLS_POOL})),
              "a toolchain is added, its id made from its name");
    cli_check(!toolchains_added(s_zig, id, why, sizeof(why)) &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = why, .cap = CLI_CELLS_POOL, .other = "already", .other_cap = CLI_CELLS_POOL}) != NULL),
              "and is not added twice");
    cli_check(!toolchains_added(s_git, id, why, sizeof(why)), "an id orior_cli has is refused");
    CliLine said = {why, sizeof(why), 0ull};
    cli_check(cli_toolchains_add_group("Proof assistants", &said), "an empty group is added");
    char groups[1024];
    toolchains_added_groups(groups, sizeof(groups));
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = groups, .other = "Systems|Proof assistants", .cap = CLI_CELLS_POOL}),
              "the reader's groups are kept");
    CliLine every = {groups, sizeof(groups), 0ull};
    cli_toolchains_groups(toolchains_group_kept, &every);
    const unsigned long long length = cli_line_closed(&every);
    static const char s_last[] = "|Systems|Proof assistants";
    cli_check((length >= sizeof(s_last) - 1u) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = groups + length - (sizeof(s_last) - 1u), .other = s_last, .cap = CLI_CELLS_POOL})),
              "and come after orior's own");
    said.at = 0ull;
    cli_check(!cli_toolchains_remove_group("Systems", &said), "a group holding a tool stays");
    said.at = 0ull;
    cli_check(
        !cli_toolchains_remove("git", &said) && (cli_line_closed(&said) != 0ull) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = why, .cap = CLI_CELLS_POOL, .other = "comes with orior", .other_cap = CLI_CELLS_POOL}) != NULL),
        "a toolchain orior_cli has is not taken out");
    said.at = 0ull;
    cli_check(cli_toolchains_remove("zig-0-13", &said), "a toolchain the reader added is taken out");
    said.at = 0ull;
    cli_check(cli_toolchains_remove_group("Systems", &said), "and its group once empty");
    toolchains_added_groups(groups, sizeof(groups));
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = groups, .other = "Proof assistants", .cap = CLI_CELLS_POOL}),
              "the reader's other group stays");
}

// a star matches any run and versions sort as numbers
static void toolchains_star(void)
{
    cli_check(cli_toolchains_matches("Python3*", "Python314"), "a star matches the run after it");
    cli_check(cli_toolchains_matches("R-*", "R-4.6.1"), "with dots in it");
    cli_check(!cli_toolchains_matches("v*x", "v13.3"), "and the end after a star must match");
    cli_check((cli_toolchains_natural("Python314", "Python312") > 0) &&
                  (cli_toolchains_natural("Python312", "Python39") > 0),
              "versions sort as numbers");
}

// a folder already on the PATH is not added twice
static void toolchains_joined(void)
{
    char out[256];
    cli_check(
        cli_toolchains_joined("a" TOOLCHAINS_SEPARATOR "b" TOOLCHAINS_SEPARATOR, "c", out, sizeof(out)) &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = out, .other = "a" TOOLCHAINS_SEPARATOR "b" TOOLCHAINS_SEPARATOR "c", .cap = CLI_CELLS_POOL})),
        "a folder is added at the PATH's end");
    cli_check(!cli_toolchains_joined("a" TOOLCHAINS_SEPARATOR "c", "c", out, sizeof(out)),
              "a folder already on the PATH is not added twice");
    cli_check(cli_toolchains_joined("", "c", out, sizeof(out)) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = out, .other = "c", .cap = CLI_CELLS_POOL})),
              "an empty PATH holds it");
#if defined(_WIN32)
    cli_check(!cli_toolchains_joined("C:\\Tools\\", "c:\\tools", out, sizeof(out)),
              "case and a trailing separator aside");
#endif
}

// a tool in a chosen folder is found there first
static void toolchains_chosen(void)
{
    char dir[CLI_PATH_LONGEST];
    char file[CLI_PATH_LONGEST];
    char program[CLI_PATH_LONGEST];
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check_folder("toolchains_chosen", dir);
    cli_full(dir, TOOLCHAINS_GHDL, file);
    (void)query_file_written(file, "", 0ull);
    cli_check(cli_toolchains_choose("ghdl", dir, program, &said), "a folder is chosen for ghdl");
    CliJson json;
    said.at = 0ull;
    const unsigned int tool = cli_toolchains_tool(&json, cli_toolchains_manifest(&json, 0u, 0ull), "ghdl", &said);
    CliFound found;
    cli_toolchains_find(&json, tool, NULL, 0u, &found);
    cli_json_given(&json);
    cli_check((tool != CLI_JSON_NONE) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = found.state, .other = "chosen", .cap = CLI_CELLS_POOL})),
              "the tool is found in the chosen folder");
    char wanted[CLI_PATH_LONGEST];
    char held[CLI_PATH_LONGEST];
    cli_toolchains_key_of(file, wanted);
    cli_toolchains_key_of(found.program, held);
    cli_check(found.has_program && (cellul.eq(&(CatenaFinitaCfg){.src = held, .other = wanted, .cap = CLI_CELLS_POOL})),
              "its program the one there");
    said.at = 0ull;
    (void)cli_toolchains_forget("ghdl", &said);
}

// the PATH a step runs with begins with the folders put ahead of it, each once, and goes on with the PATH's own
static void toolchains_run_path(void)
{
    char ahead[2][CLI_PATH_LONGEST];
    cli_check_folder("toolchains_ahead", ahead[0]);
    memor.cpy(&(MemoriaCfg){.dst = ahead[1], .src = ahead[0], .bytes = sizeof(ahead[0])});
    char *const path = cli_cell_taken(CLI_PATH_TEXT, "the PATH a step runs with, as checked");
    if (path == NULL)
    {
        cli_check(0, "the PATH a step runs with has its room");
        return;
    }
    cli_toolchains_run_path_with((const char (*)[CLI_PATH_LONGEST])ahead, 2u, path, CLI_PATH_TEXT);
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = ahead[0], .cap = CLI_CELLS_POOL});
    const int first = cellul.starts(&(CatenaFinitaCfg){.src = path, .other = ahead[0], .cap = CLI_CELLS_POOL}) &&
                      (path[length] == TOOLCHAINS_SEPARATOR[0]);
    const int once =
        cellul.find(&(CatenaFinitaCfg){
            .src = path + length, .cap = CLI_CELLS_POOL, .other = ahead[0], .other_cap = CLI_CELLS_POOL}) == NULL;
    cli_check(first && once && (path[length + 1u] != '\0'),
              "the PATH a step runs with begins with the folders put ahead, each once, and goes on");
    cli_cell_given(path);
}

int main(void)
{
    char home[CLI_PATH_LONGEST];
    cli_check_folder("toolchains_home", home);
    (void)query_environment_set("ORIOR_HOME", home);
#if !defined(_WIN32)
    toolchains_shell();
#endif
    toolchains_needed();
    toolchains_tree_uses();
    toolchains_kept();
    toolchains_manifest();
    toolchains_reader();
    toolchains_star();
    toolchains_joined();
    toolchains_chosen();
    toolchains_run_path();
    return cli_checked();
}
