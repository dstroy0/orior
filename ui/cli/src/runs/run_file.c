// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// run_file.c: the shell line that runs one file with its language's toolchain, its names filled in
#include "run_file.h"
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "../toolchains/toolchains.h"

// A way to run the file: the toolchain and its line
typedef struct
{
    unsigned int tool;
    const char *line;
} CliNamed;
// A line being filled: the tree, the file, the toolchain, its program and the folder it is in, the file's stem, and
// the language, which a failure names
typedef struct
{
    const char *root;
    const char *path;
    const char *tool_name;
    const char *program;
    const char *folder;
    const char *name;
    unsigned long long stem;
    const char *language;
} CliFilling;
// the most toolchains that name a way to run a file
#define CLI_RUN_NAMED 64u

void cli_run_file_quote(const CliRunFileCfg *args)
{
    mmgr_line_ch(args->out, '\'');
    const unsigned long long whole = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL});
    for (unsigned long long at = 0ULL; at < whole;)
    {
        const unsigned long long stop = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                   .text = args->path, .at = at, .size = whole, .set = "'");
        const size_t from = args->out->at;
        const size_t reserved = mmgr_line_put_n(args->out, args->path + at, (size_t)(stop - at));
        EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = args->out->out + from, .size = reserved,
                   .from = '\\', .to = '/');
        if (stop < whole)
        {
            mmgr_line_put(args->out, "'\\''");
        }
        at = stop + 1ULL;
    }
    mmgr_line_ch(args->out, '\'');
}

typedef struct
{
    const char *path;
    const char **name;
    unsigned long long *stem;
    const CliJson *json;
    unsigned int manifest;
    const char *language;
    const char *extension;
    CliNamed *named;
    const CliFilling *filling;
    const char *name_value;
    unsigned long long length;
    VerbaLinea *line;
    VerbaLinea *err_msg;
    const char *template;
    const char *root;
    const CliNamed *ways;
    unsigned int named_u32;
    const char (*folders)[CLI_PATH_MAX_LEN];
    unsigned int count;
    VerbaLinea *out;
} RunFileCtx;
// the file name of `path` as its stem and its extension, split at the last dot, a dot that starts the name splitting
// nothing: the name's start into `name`, the stem's length into `stem`, and the extension, NULL where it has none
EMBED_INLINE const char *cli_name_split(const RunFileCtx *args)
{
    const char **name = args->name;
    unsigned long long *stem = args->stem;
    const char *start = args->path;
    const unsigned long long start_end =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}), .set = "/\\");
    start = (start_end != 0ULL) ? (args->path + start_end) : start;
    *name = start;
    const char *const dot =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = start,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = start, .cap = CLI_CELLS_POOL}), .byte = '.');
    const int split = (dot != NULL) && (dot != start) &&
                      (!cellul.eq(&(CatenaFinitaCfg){.src = start, .other = "..", .cap = CLI_CELLS_POOL}));
    // the dot lies in the name, at or past its start
    *stem =
        split ? (unsigned long long)(dot - start) : cellul.len(&(CatenaFinitaCfg){.src = start, .cap = CLI_CELLS_POOL});
    return split ? (dot + 1) : NULL;
}

// the ways the toolchains give to run the file, by its language and extension first and then its language alone,
// into `named`: their count
EMBED_INLINE unsigned int cli_ways(const RunFileCtx *args)
{
    const CliJson *json = args->json;
    char keys[2][CLI_NAME_MAX_LEN];
    VerbaLinea with = {keys[0], sizeof(keys[0]), 0ULL};
    mmgr_line_put(&with, args->language);
    mmgr_line_ch(&with, '.');
    EMBED_CALL(cli_text_out_name.append_lower, CliTextOutNameCfg, .out = &with,
               .text = (args->extension != NULL) ? args->extension : "");
    mmgr_line_finish(&with);
    memor.cpy(
        &(MemoriaCfg){.dst = keys[1],
                      .src = args->language,
                      .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->language, .cap = CLI_CELLS_POOL}) + 1u});
    unsigned int count = 0u;
    for (unsigned int key = 0u; key < 2u; key += 1u)
    {
        unsigned int index = 0u;
        while (count < CLI_RUN_NAMED)
        {
            const unsigned int tool = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                 .json_read = json, .array = args->manifest, .index = index);
            if (tool == CLI_JSON_NONE)
            {
                break;
            }
            const unsigned int runs = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                 .json_read = json, .object = tool, .name = "runs");
            const char *const line =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = runs, .name = keys[key]),
                           .length_pointer = NULL);
            if (line != NULL)
            {
                args->named[count] = (CliNamed){tool, line};
                count += 1u;
            }
            index += 1u;
        }
    }
    return count;
}

// the name `{name}`, `length` bytes, filled into the line: 1, or 0 with why given
EMBED_INLINE int cli_on_fill(const RunFileCtx *args)
{
    char key[CLI_NAME_MAX_LEN];
    const unsigned long long copied_len = (args->length < sizeof(key)) ? args->length : (sizeof(key) - 1ULL);
    memor.cpy(&(MemoriaCfg){.dst = key, .src = args->name_value, .bytes = copied_len});
    key[copied_len] = '\0';
    if (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "file", .cap = CLI_CELLS_POOL}))
    {
        EMBED_CALL(cli_run_file.quote, CliRunFileCfg, .out = args->line, .path = args->filling->path);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "path", .cap = CLI_CELLS_POOL}))
    {
        (void)EMBED_CALL(cli_text_from.append_swap, CliTextFromCfg, .out = args->line, .text = args->filling->path,
                         .from = '\\', .to = '/');
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "dir", .cap = CLI_CELLS_POOL}))
    {
        char parent[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_run_file.quote, CliRunFileCfg, .out = args->line,
                   .path = EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx,
                                      .path = args->filling->path, .out = parent)
                               ? parent
                               : args->filling->root);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "stem", .cap = CLI_CELLS_POOL}))
    {
        mmgr_line_put_n(args->line, args->filling->name, args->filling->stem);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "out", .cap = CLI_CELLS_POOL}))
    {
        char made[CLI_PATH_MAX_LEN];
        VerbaLinea folder = {made, sizeof(made), 0ULL};
        mmgr_line_put(&folder, args->filling->root);
        mmgr_line_put(&folder, "/build/run");
        if ((mmgr_line_finish(&folder) == 0ULL) ||
            !EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = made))
        {
            mmgr_line_put(args->err_msg, "build/run: it could not be made");
            return 0;
        }
        char out[CLI_PATH_MAX_LEN];
        VerbaLinea file = {out, sizeof(out), 0ULL};
        mmgr_line_put(&file, made);
        mmgr_line_ch(&file, '/');
        mmgr_line_put_n(&file, args->filling->name, args->filling->stem);
#if defined(_WIN32)
        mmgr_line_put(&file, ".exe");
#endif
        mmgr_line_finish(&file);
        EMBED_CALL(cli_run_file.quote, CliRunFileCfg, .out = args->line, .path = out);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "program", .cap = CLI_CELLS_POOL}))
    {
        EMBED_CALL(cli_run_file.quote, CliRunFileCfg, .out = args->line, .path = args->filling->program);
    }
    else
    {
        const char *const names[1] = {key};
        char other[CLI_PATH_MAX_LEN];
        if (!EMBED_CALL(cli_toolchains_dir.program_in, CliToolchainsDirCfg, .dir = args->filling->folder,
                        .names = names, .count = 1u, .path = other))
        {
            mmgr_line_put(args->err_msg, args->filling->tool_name);
            mmgr_line_put(args->err_msg, " has no ");
            mmgr_line_put(args->err_msg, key);
            mmgr_line_put(args->err_msg, " in ");
            mmgr_line_put(args->err_msg, args->filling->folder);
            return 0;
        }
        EMBED_CALL(cli_run_file.quote, CliRunFileCfg, .out = args->line, .path = other);
    }
    return 1;
}

// the toolchain's template with its names filled in, written into `line`: 1, or 0 with why given
EMBED_INLINE int cli_template_fill(const RunFileCtx *args)
{
    const char *rest = args->template;
    for (const char *open = cellul.chr(&(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL, .byte = '{'});
         open != NULL; open = cellul.chr(&(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL, .byte = '{'}))
    {
        // the open brace lies in the rest, at or past its start
        mmgr_line_put_n(args->line, rest, (unsigned long long)(open - rest));
        const char *const close = cellul.chr(&(CatenaFinitaCfg){.src = open, .cap = CLI_CELLS_POOL, .byte = '}'});
        if (close == NULL)
        {
            mmgr_line_put(args->err_msg, args->filling->tool_name);
            mmgr_line_put(args->err_msg, "'s line for ");
            mmgr_line_put(args->err_msg, args->filling->language);
            mmgr_line_put(args->err_msg, " has an open {");
            return 0;
        }
        // the close brace lies past the open one
        if (!EMBED_CALL(cli_on_fill, RunFileCtx, .filling = args->filling, .name_value = open + 1,
                        .length = (unsigned long long)(close - open) - 1ULL, .line = args->line,
                        .err_msg = args->err_msg))
        {
            return 0;
        }
        rest = close + 1;
    }
    mmgr_line_put(args->line, rest);
    return 1;
}

// the line that runs the file found by the ways `named` names, the toolchains in `json` and the PATH's folders in
// `folders`, `count` of them, into `out`: 1, or 0 with why given
EMBED_INLINE int run_file_find(const RunFileCtx *args)
{
    const char *path = args->path;
    const CliJson *json = args->json;
    VerbaLinea *out = args->out;
    const char *name = path;
    unsigned long long stem = 0ULL;
    (void)EMBED_CALL(cli_name_split, RunFileCtx, .path = path, .name = &name, .stem = &stem);
    CliFound found_tool;
    unsigned int way = 0u;
    int found = 0;
    for (; !found && (way < args->named_u32); way += found ? 0u : 1u)
    {
        EMBED_CALL(cli_toolchains_json.find, CliToolchainsJsonCfg, .json = json, .tool = args->ways[way].tool,
                   .path_callback = args->folders, .count = args->count, .found = &found_tool);
        found = found_tool.has_program;
    }
    if (!found)
    {
        for (unsigned int at = 0u; at < args->named_u32; at += 1u)
        {
            const char *const displayed =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = args->ways[at].tool, .name = "name"),
                           .length_pointer = NULL);
            mmgr_line_put(args->err_msg, (at != 0u) ? " or " : "");
            mmgr_line_put(args->err_msg, (displayed != NULL) ? displayed : "");
        }
        mmgr_line_put(args->err_msg,
                      " runs it, and is not found: File, Toolchains opens its install page or takes its folder");
        return 0;
    }
    const char *const tool_name =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->ways[way].tool, .name = "name"),
                   .length_pointer = NULL);
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = found_tool.program,
                    .out = folder))
    {
        folder[0] = '\0';
    }
    const CliFilling filling = {
        args->root, path, (tool_name != NULL) ? tool_name : "", found_tool.program, folder, name, stem, args->language};
    // the line written straight into the answer's string as it is filled
    mmgr_line_put(out, "{\"line\":");
    char piece[1024];
    CliJsonQuoting quoting;
    VerbaLinea run;
    EMBED_CALL(cli_json_tree_out.json_quoting_begin, CliJsonTreeOutCfg, .quoting = &quoting, .out = out, .line = &run,
               .piece = piece, .piece_size = sizeof(piece));
    mmgr_line_put(&run, "( cd ");
    EMBED_CALL(cli_run_file.quote, CliRunFileCfg, .out = &run, .path = args->root);
    mmgr_line_put(&run, " && ");
    if (!EMBED_CALL(cli_template_fill, RunFileCtx, .filling = &filling, .template = args->ways[way].line, .line = &run,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    mmgr_line_put(&run, " )");
    EMBED_CALL(cli_json_tree_out.json_quoting_end, CliJsonTreeOutCfg, .quoting = &quoting, .line = &run);
    mmgr_line_put(out, ",\"tool\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = filling.tool_name,
               .length = cellul.len(&(CatenaFinitaCfg){.src = filling.tool_name, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(out, '}');
    return 1;
}

int cli_run_file_line(const CliRunFileCfg *args)
{
    CliNamed ways[CLI_RUN_NAMED];
    CliJson json;
    const unsigned int manifest = EMBED_CALL(cli_toolchains_json_nodes_more.manifest, CliToolchainsJsonNodesMoreCfg,
                                             .json_out = &json, .nodes_more = 0u, .text_more = 0ULL);
    const char *name = args->path;
    unsigned long long stem = 0ULL;
    const char *const extension =
        EMBED_CALL(cli_name_split, RunFileCtx, .path = args->path, .name = &name, .stem = &stem);
    const unsigned int named = EMBED_CALL(cli_ways, RunFileCtx, .json = &json, .manifest = manifest,
                                          .language = args->language, .extension = extension, .named = ways);
    int done = 0;
    if (named == 0u)
    {
        mmgr_line_put(args->err_msg, "orior_cli has no way to run a ");
        mmgr_line_put(args->err_msg, args->language);
        mmgr_line_put(args->err_msg, " file");
    }
    else
    {
        unsigned int count = 0u;
        char (*const folders)[CLI_PATH_MAX_LEN] = cli_toolchains_path_allocated(&count, &g_cli_runs_cells);
        done = EMBED_CALL(run_file_find, RunFileCtx, .root = args->root, .path = args->path, .language = args->language,
                          .json = &json, .ways = ways, .named_u32 = named,
                          .folders = (const char (*)[CLI_PATH_MAX_LEN])folders, .count = count, .out = args->out,
                          .err_msg = args->err_msg);
        g_cli_cells->persistent_buf_release(folders, &g_cli_runs_cells);
    }
    g_cli_cells->persistent_buf_release(json.node,
                                        EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg, .cell = json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return done;
}
