// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_go.c: the main packages of a Go module, and the programs that can build a package.json's bin
#include "../text/text.h"
#include "executables_internal.h"
#include "sea.h"

// the walk of a Go module's folders, and a Go file read, under the lock of the read they are made for
static CliWalk s_go_walk;
static char s_more[CLI_EXECUTABLES_BYTES];

// an entry of a folder of a Go module: a folder to look in after, where it is no module of its own, or a Go file other
// than a test that says `package main`
static void cli_go_each(void *context, const char *name, int is_folder)
{
    CliWalk *const walk = (CliWalk *)context;
    char path[CLI_PATH_LONGEST];
    if (!cli_executables_joined(walk->folder, name, path))
    {
        return;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    if (is_folder)
    {
        char module[CLI_PATH_LONGEST];
        const int own_module = cli_executables_joined(path, "go.mod", module) && query_is_file(module);
        if (((walk->at_depth + 1u) < CLI_EXECUTABLES_DEEP) && !own_module && (name[0] != '.') && (name[0] != '_') &&
            (!cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "testdata", .cap = CLI_CELLS_POOL})) &&
            !cli_executables_passed(name) && (walk->pending_count < CLI_EXECUTABLES_PENDING))
        {
            memor.cpy(&(MemoriaCfg){.dst = walk->pending[walk->pending_count],
                                    .src = path,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
            walk->depth[walk->pending_count] = walk->at_depth + 1u;
            walk->pending_count += 1u;
        }
    }
    else if (!walk->main && (length > 3ull) &&
             (cellul.eq(&(CatenaFinitaCfg){.src = name + length - 3ull, .other = ".go", .cap = CLI_CELLS_POOL})) &&
             !((length > 8ull) && (cellul.eq(&(CatenaFinitaCfg){
                                      .src = name + length - 8ull, .other = "_test.go", .cap = CLI_CELLS_POOL}))))
    {
        char package[CLI_NAME_LONGEST];
        walk->main = cli_text_read(path, s_more, sizeof(s_more)) && cli_go_package(s_more, package) &&
                     (cellul.eq(&(CatenaFinitaCfg){.src = package, .other = "main", .cap = CLI_CELLS_POOL}));
    }
}

// the main packages of the Go module whose go.mod is `module`: each named by its folder, and the module's own folder by
// the last part of the module's path, with the package's folder as `go build` takes it
void cli_go_mains(const char *module, const char *text, CliNames *mains)
{
    mains->count = 0u;
    char path[CLI_PATH_LONGEST] = "";
    for (const char *line = text; line[0] != '\0';)
    {
        const char *stop = line;
        while ((stop[0] != '\0') && (stop[0] != '\n'))
        {
            stop += 1;
        }
        const char *from = line;
        unsigned long long length = (unsigned long long)(stop - line);
        cli_scan_trim(&from, &length);
        if ((length >= 7ull) && (memor.cmp(&(MemoriaCfg){.src = from, .other = "module ", .bytes = 7u}) == 0))
        {
            cli_toml_value(from + 7, length - 7ull, path, sizeof(path));
            break;
        }
        line = (stop[0] != '\0') ? (stop + 1) : stop;
    }
    char top[CLI_PATH_LONGEST];
    if (!cli_path_parent(module, top))
    {
        memor.cpy(&(MemoriaCfg){
            .dst = top, .src = ".", .bytes = cellul.len(&(CatenaFinitaCfg){.src = ".", .cap = CLI_CELLS_POOL}) + 1u});
    }
    CliWalk *const walk = &s_go_walk;
    walk->found_count = 0u;
    cli_executables_walked(walk, top, cli_go_each);
    const unsigned long long top_length = cellul.len(&(CatenaFinitaCfg){.src = top, .cap = CLI_CELLS_POOL});
    for (unsigned int at = 0u; at < walk->found_count; at += 1u)
    {
        char package[CLI_PATH_LONGEST];
        const char *inner = walk->found[at] + top_length;
        inner += ((inner[0] == '/') || (inner[0] == '\\')) ? 1 : 0;
        memor.cpy(&(MemoriaCfg){.dst = package,
                                .src = inner,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = inner, .cap = CLI_CELLS_POOL}) + 1u});
        for (char *slash = package; slash[0] != '\0'; slash += 1)
        {
            slash[0] = (slash[0] == '\\') ? '/' : slash[0];
        }
        const char *const named = (package[0] == '\0') ? path : package;
        const char *const name =
            (cli_text_last(named, cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}), '/') != NULL)
                ? (cli_text_last(named, cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}), '/') + 1)
                : named;
        char shown[CLI_PATH_LONGEST];
        CliLine line = {shown, sizeof(shown), 0ull};
        cli_line_text(&line, (package[0] == '\0') ? "." : "./");
        cli_line_text(&line, package);
        cli_line_closed(&line);
        if ((name[0] != '\0') && (mains->count < CLI_EXECUTABLES_NAMES))
        {
            memor.cpy(&(MemoriaCfg){.dst = mains->name[mains->count],
                                    .src = name,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
            memor.cpy(&(MemoriaCfg){.dst = mains->value[mains->count],
                                    .src = shown,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = shown, .cap = CLI_CELLS_POOL}) + 1u});
            mains->count += 1u;
        }
    }
    // the mains in the order of their names, then their folders
    for (unsigned int at = 1u; at < mains->count; at += 1u)
    {
        for (unsigned int place = at; place > 0u; place -= 1u)
        {
            const int order = cli_text_order(mains->name[place - 1u], mains->name[place]);
            if ((order < 0) || ((order == 0) && (cli_text_order(mains->value[place - 1u], mains->value[place]) <= 0)))
            {
                break;
            }
            char name[CLI_NAME_LONGEST];
            char value[CLI_PATH_LONGEST];
            memor.cpy(&(MemoriaCfg){
                .dst = name,
                .src = mains->name[place],
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = mains->name[place], .cap = CLI_CELLS_POOL}) + 1u});
            memor.cpy(&(MemoriaCfg){
                .dst = value,
                .src = mains->value[place],
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = mains->value[place], .cap = CLI_CELLS_POOL}) + 1u});
            memor.cpy(
                &(MemoriaCfg){.dst = mains->name[place], .bytes = CLI_NAME_LONGEST, .src = mains->name[place - 1u]});
            memor.cpy(
                &(MemoriaCfg){.dst = mains->value[place], .bytes = CLI_PATH_LONGEST, .src = mains->value[place - 1u]});
            memor.cpy(&(MemoriaCfg){.dst = mains->name[place - 1u],
                                    .src = name,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
            memor.cpy(&(MemoriaCfg){.dst = mains->value[place - 1u],
                                    .src = value,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL}) + 1u});
        }
    }
}

// the programs that can build a bin, the one a job takes unless told otherwise first: Bun where the package holds
// Bun's lock and Deno where it holds Deno's settings; else Node itself where the script asks for no module outside
// Node, which a single executable of Node's cannot hold, and else the bundlers, those the machine has before those it
// lacks
void cli_npm_builders(const char *folder, const char *script, CliExecutable *one)
{
    static char s_outside[4][CLI_NAME_LONGEST];
    static const char *const s_bun[3] = {"bun", "deno", "node"};
    static const char *const s_deno[3] = {"deno", "bun", "node"};
    static const char *const s_node[3] = {"node", "bun", "deno"};
    const char *const *order = NULL;
    char path[CLI_PATH_LONGEST];
    if ((cli_executables_joined(folder, "bun.lock", path) && query_is_file(path)) ||
        (cli_executables_joined(folder, "bun.lockb", path) && query_is_file(path)))
    {
        order = s_bun;
    }
    else if ((cli_executables_joined(folder, "deno.json", path) && query_is_file(path)) ||
             (cli_executables_joined(folder, "deno.jsonc", path) && query_is_file(path)))
    {
        order = s_deno;
    }
    else if (cli_sea_outside(script, s_outside, 4u) == 0u)
    {
        order = s_node;
    }
    if (order != NULL)
    {
        one->with[0] = order[0];
        one->with[1] = order[1];
        one->with[2] = order[2];
        one->withs = 3u;
        return;
    }
    char found[CLI_PATH_LONGEST];
    char why[CLI_NAME_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    const int has_bun = cli_executables_program("bun", "bun", found, &said);
    said.at = 0ull;
    const int has_deno = cli_executables_program("deno", "deno", found, &said);
    // the bundlers the machine has first, Bun before Deno where it has both or neither
    one->with[0] = (!has_bun && has_deno) ? "deno" : "bun";
    one->with[1] = (!has_bun && has_deno) ? "bun" : "deno";
    one->with[2] = "node";
    one->withs = 3u;
}
