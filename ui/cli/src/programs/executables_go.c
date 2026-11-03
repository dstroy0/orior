// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_go.c: the main packages of a Go module, and the programs that can build a package.json's bin
#include "../command_line/callback_cfg.h"
#include "../text/text.h"
#include "executables_internal.h"
#include "sea.h"

// an entry of a folder of a Go module: a folder to look in after, where it is no module of its own, or a Go file other
// than a test that declares `package main`
static void cli_go_each(const CliEachNameIsFolderCfg *args)
{
    const char *name = args->name;
    CliWalk *const walk = (CliWalk *)args->context;
    char path[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_executables_name_out_folder_internal.join, CliExecutablesNameOutFolderCtx,
                    .folder = walk->folder, .name = name, .out = path))
    {
        return;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    if (args->is_folder)
    {
        char module[CLI_PATH_MAX_LEN];
        const int own_module = EMBED_CALL(cli_executables_name_out_folder_internal.join, CliExecutablesNameOutFolderCtx,
                                          .folder = path, .name = "go.mod", .out = module) &&
                               EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = module);
        if (((walk->at_depth + 1u) < CLI_EXECUTABLES_DEEP) && !own_module && (name[0] != '.') && (name[0] != '_') &&
            (!cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "testdata", .cap = CLI_CELLS_POOL})) &&
            !EMBED_CALL(cli_executables_name_out_folder_internal.pass, CliExecutablesNameOutFolderCtx, .name = name))
        {
            (void)EMBED_CALL(cli_executables_walk_internal.pending_append, CliExecutablesWalkCtx, .walk = walk,
                             .path = path, .depth = walk->at_depth + 1u);
        }
    }
    else if (!walk->main && (length > 3ULL) &&
             (cellul.eq(&(CatenaFinitaCfg){.src = name + length - 3ULL, .other = ".go", .cap = CLI_CELLS_POOL})) &&
             !((length > 8ULL) && (cellul.eq(&(CatenaFinitaCfg){
                                      .src = name + length - 8ULL, .other = "_test.go", .cap = CLI_CELLS_POOL}))))
    {
        char package[CLI_NAME_MAX_LEN];
        char *const text = EMBED_CALL(cli_executables_walk_internal.text_alloc, CliExecutablesWalkCtx, .path = path);
        walk->main = (text != NULL) &&
                     EMBED_CALL(cli_executables_text_out_internal.go_package, CliExecutablesTextOutCtx, .text = text,
                                .out = package) &&
                     (cellul.eq(&(CatenaFinitaCfg){.src = package, .other = "main", .cap = CLI_CELLS_POOL}));
        g_cli_cells->persistent_buf_release(text, &g_cli_executables_cells);
    }
}

// the main packages of the Go module whose go.mod is `module`: each named by its folder, and the module's own folder by
// the last part of the module's path, with the package's folder as `go build` takes it
void cli_go_mains(const CliExecutablesTextModuleCtx *args)
{
    args->mains->count = 0u;
    char path[CLI_PATH_MAX_LEN] = "";
    for (const char *line = args->text; line[0] != '\0';)
    {
        const char *stop = line;
        stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ULL,
                           .size = cellul.len(&(CatenaFinitaCfg){.src = stop, .cap = CLI_CELLS_POOL}), .set = "\n");
        const char *from = line;
        unsigned long long length = (unsigned long long)(stop - line);
        EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx, .text_pointer = &from,
                   .length_pointer = &length);
        if ((length >= 7ULL) && (memor.cmp(&(MemoriaCfg){.src = from, .other = "module ", .bytes = 7u}) == 0))
        {
            EMBED_CALL(cli_executables_length_value_internal.toml_value, CliExecutablesLengthValueCtx,
                       .value = from + 7, .length = length - 7ULL, .out = path, .capacity = sizeof(path));
            break;
        }
        line = (stop[0] != '\0') ? (stop + 1) : stop;
    }
    char top[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = args->module_text, .out = top))
    {
        memor.cpy(&(MemoriaCfg){
            .dst = top, .src = ".", .bytes = cellul.len(&(CatenaFinitaCfg){.src = ".", .cap = CLI_CELLS_POOL}) + 1u});
    }
    CliWalk go_walk;
    CliWalk *const walk = &go_walk;
    EMBED_CALL(cli_executables_walk_internal.walk_alloc, CliExecutablesWalkCtx, .walk = walk);
    EMBED_CALL(cli_executables_walk_top_internal.walk, CliExecutablesWalkTopCtx, .walk = walk, .top = top,
               .each = cli_go_each);
    const unsigned long long top_length = cellul.len(&(CatenaFinitaCfg){.src = top, .cap = CLI_CELLS_POOL});
    for (unsigned int at = 0u; at < walk->found_count; at += 1u)
    {
        char package[CLI_PATH_MAX_LEN];
        const char *inner = walk->found[at] + top_length;
        inner += ((inner[0] == '/') || (inner[0] == '\\')) ? 1 : 0;
        memor.cpy(&(MemoriaCfg){.dst = package,
                                .src = inner,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = inner, .cap = CLI_CELLS_POOL}) + 1u});
        EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = package,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = package, .cap = CLI_CELLS_POOL}), .from = '\\',
                   .to = '/');
        const char *const named = (package[0] == '\0') ? path : package;
        const char *const name =
            (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = named,
                        .length_value = cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}),
                        .byte = '/') != NULL)
                ? (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = named,
                              .length_value = cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}),
                              .byte = '/') +
                   1)
                : named;
        char displayed[CLI_PATH_MAX_LEN];
        VerbaLinea line = {displayed, sizeof(displayed), 0ULL};
        mmgr_line_put(&line, (package[0] == '\0') ? "." : "./");
        mmgr_line_put(&line, package);
        mmgr_line_finish(&line);
        if ((name[0] != '\0') && (args->mains->count < CLI_EXECUTABLES_NAMES))
        {
            memor.cpy(&(MemoriaCfg){.dst = args->mains->name[args->mains->count],
                                    .src = name,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
            memor.cpy(
                &(MemoriaCfg){.dst = args->mains->value[args->mains->count],
                              .src = displayed,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = displayed, .cap = CLI_CELLS_POOL}) + 1u});
            args->mains->count += 1u;
        }
    }
    EMBED_CALL(cli_executables_walk_internal.walk_free, CliExecutablesWalkCtx, .walk = walk);
    // the mains in the order of their names, then their folders
    for (unsigned int at = 1u; at < args->mains->count; at += 1u)
    {
        for (unsigned int place = at; place > 0u; place -= 1u)
        {
            const int order =
                EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = args->mains->name[place - 1u],
                           .other_pointer = args->mains->name[place]);
            if ((order < 0) || ((order == 0) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg,
                                                            .one = args->mains->value[place - 1u],
                                                            .other_pointer = args->mains->value[place]) <= 0)))
            {
                break;
            }
            char name[CLI_NAME_MAX_LEN];
            char value[CLI_PATH_MAX_LEN];
            memor.cpy(&(MemoriaCfg){
                .dst = name,
                .src = args->mains->name[place],
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->mains->name[place], .cap = CLI_CELLS_POOL}) + 1u});
            memor.cpy(&(MemoriaCfg){
                .dst = value,
                .src = args->mains->value[place],
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->mains->value[place], .cap = CLI_CELLS_POOL}) + 1u});
            memor.cpy(&(MemoriaCfg){
                .dst = args->mains->name[place], .bytes = CLI_NAME_MAX_LEN, .src = args->mains->name[place - 1u]});
            memor.cpy(&(MemoriaCfg){
                .dst = args->mains->value[place], .bytes = CLI_PATH_MAX_LEN, .src = args->mains->value[place - 1u]});
            memor.cpy(&(MemoriaCfg){.dst = args->mains->name[place - 1u],
                                    .src = name,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
            memor.cpy(&(MemoriaCfg){.dst = args->mains->value[place - 1u],
                                    .src = value,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL}) + 1u});
        }
    }
}

// the programs that can build a bin, the one a job takes unless instructed otherwise first: Bun where the package
// contains Bun's lock and Deno where it contains Deno's settings; else Node itself where the script requires no module
// outside Node, which a single executable of Node's cannot contain, and else the bundlers, those the machine has before
// those it lacks
void cli_npm_builders(const CliExecutablesFolderCtx *args)
{
    static char s_outside[4][CLI_NAME_MAX_LEN];
    static const char *const s_bun[3] = {"bun", "deno", "node"};
    static const char *const s_deno[3] = {"deno", "bun", "node"};
    static const char *const s_node[3] = {"node", "bun", "deno"};
    const char *const *order = NULL;
    char path[CLI_PATH_MAX_LEN];
    if ((EMBED_CALL(cli_executables_name_out_folder_internal.join, CliExecutablesNameOutFolderCtx,
                    .folder = args->folder, .name = "bun.lock", .out = path) &&
         EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path)) ||
        (EMBED_CALL(cli_executables_name_out_folder_internal.join, CliExecutablesNameOutFolderCtx,
                    .folder = args->folder, .name = "bun.lockb", .out = path) &&
         EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path)))
    {
        order = s_bun;
    }
    else if ((EMBED_CALL(cli_executables_name_out_folder_internal.join, CliExecutablesNameOutFolderCtx,
                         .folder = args->folder, .name = "deno.json", .out = path) &&
              EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path)) ||
             (EMBED_CALL(cli_executables_name_out_folder_internal.join, CliExecutablesNameOutFolderCtx,
                         .folder = args->folder, .name = "deno.jsonc", .out = path) &&
              EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path)))
    {
        order = s_deno;
    }
    else if (EMBED_CALL(cli_sea_table.outside, CliSeaCfg, .text = args->script, .found = s_outside, .capacity = 4u) ==
             0u)
    {
        order = s_node;
    }
    if (order != NULL)
    {
        args->one->with[0] = order[0];
        args->one->with[1] = order[1];
        args->one->with[2] = order[2];
        args->one->withs = 3u;
        return;
    }
    char found[CLI_PATH_MAX_LEN];
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const int has_bun = EMBED_CALL(cli_executables_tool.program, CliExecutablesToolCfg, .tool = "bun", .name = "bun",
                                   .path = found, .err_msg = &err_msg);
    err_msg.at = 0ULL;
    const int has_deno = EMBED_CALL(cli_executables_tool.program, CliExecutablesToolCfg, .tool = "deno", .name = "deno",
                                    .path = found, .err_msg = &err_msg);
    // the bundlers the machine has first, Bun before Deno where it has both or neither
    args->one->with[0] = (!has_bun && has_deno) ? "deno" : "bun";
    args->one->with[1] = (!has_bun && has_deno) ? "bun" : "deno";
    args->one->with[2] = "node";
    args->one->withs = 3u;
}
