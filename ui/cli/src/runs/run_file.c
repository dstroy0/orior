// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// run_file.c: the shell line that runs one file with its language's toolchain, its names filled in
#include "run_file.h"
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "../toolchains/toolchains.h"

// the most toolchains that name a way to run a file
#define CLI_RUN_NAMED 64u

void cli_run_file_quoted(VerbaLinea *out, const char *path)
{
    mmgr_line_ch(out, '\'');
    const unsigned long long whole = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    for (unsigned long long at = 0ull; at < whole;)
    {
        const unsigned long long stop = cli_text_to_any(path, at, whole, "'");
        const size_t from = out->at;
        const size_t taken = mmgr_line_put_n(out, path + at, (size_t)(stop - at));
        cli_text_swapped(out->out + from, taken, '\\', '/');
        if (stop < whole)
        {
            mmgr_line_put(out, "'\\''");
        }
        at = stop + 1ull;
    }
    mmgr_line_ch(out, '\'');
}

// the file name of `path` as its stem and its extension, split at the last dot, a dot that starts the name splitting
// nothing: the name's start into `name`, the stem's length into `stem`, and the extension, NULL where it has none
static const char *cli_name_split(const char *path, const char **name, unsigned long long *stem)
{
    const char *start = path;
    const unsigned long long start_past =
        cli_text_back_any(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), "/\\");
    start = (start_past != 0ull) ? (path + start_past) : start;
    *name = start;
    const char *const dot =
        cli_text_last(start, cellul.len(&(CatenaFinitaCfg){.src = start, .cap = CLI_CELLS_POOL}), '.');
    const int split = (dot != NULL) && (dot != start) &&
                      (!cellul.eq(&(CatenaFinitaCfg){.src = start, .other = "..", .cap = CLI_CELLS_POOL}));
    // the dot lies in the name, at or past its start
    *stem =
        split ? (unsigned long long)(dot - start) : cellul.len(&(CatenaFinitaCfg){.src = start, .cap = CLI_CELLS_POOL});
    return split ? (dot + 1) : NULL;
}

// A way to run the file: the toolchain and its line
typedef struct
{
    unsigned int tool;
    const char *line;
} CliNamed;

// the ways the toolchains give to run the file, by its language and extension first and then its language alone,
// into `named`: their count
static unsigned int cli_ways(const CliJson *json, unsigned int manifest, const char *language, const char *extension,
                             CliNamed *named)
{
    char keys[2][CLI_NAME_MAX_LEN];
    VerbaLinea with = {keys[0], sizeof(keys[0]), 0ull};
    mmgr_line_put(&with, language);
    mmgr_line_ch(&with, '.');
    cli_text_put_lowered(&with, (extension != NULL) ? extension : "");
    mmgr_line_finish(&with);
    memor.cpy(&(MemoriaCfg){.dst = keys[1],
                            .src = language,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = language, .cap = CLI_CELLS_POOL}) + 1u});
    unsigned int count = 0u;
    for (unsigned int key = 0u; key < 2u; key += 1u)
    {
        for (unsigned int index = 0u; count < CLI_RUN_NAMED; index += 1u)
        {
            const unsigned int tool = cli_json_item(json, manifest, index);
            if (tool == CLI_JSON_NONE)
            {
                break;
            }
            const unsigned int runs = cli_json_member(json, tool, "runs");
            const char *const line = cli_json_string(json, cli_json_member(json, runs, keys[key]), NULL);
            if (line != NULL)
            {
                named[count] = (CliNamed){tool, line};
                count += 1u;
            }
        }
    }
    return count;
}

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

// the name `{name}`, `length` bytes, filled into the line: 1, or 0 with why given
static int cli_filled(const CliFilling *filling, const char *name, unsigned long long length, VerbaLinea *line,
                      VerbaLinea *err_msg)
{
    char key[CLI_NAME_MAX_LEN];
    const unsigned long long copied_len = (length < sizeof(key)) ? length : (sizeof(key) - 1ull);
    memor.cpy(&(MemoriaCfg){.dst = key, .src = name, .bytes = copied_len});
    key[copied_len] = '\0';
    if (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "file", .cap = CLI_CELLS_POOL}))
    {
        cli_run_file_quoted(line, filling->path);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "path", .cap = CLI_CELLS_POOL}))
    {
        (void)cli_text_put_swapped(line, filling->path, '\\', '/');
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "dir", .cap = CLI_CELLS_POOL}))
    {
        char parent[CLI_PATH_MAX_LEN];
        cli_run_file_quoted(line, cli_path_parent(filling->path, parent) ? parent : filling->root);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "stem", .cap = CLI_CELLS_POOL}))
    {
        mmgr_line_put_n(line, filling->name, filling->stem);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "out", .cap = CLI_CELLS_POOL}))
    {
        char made[CLI_PATH_MAX_LEN];
        VerbaLinea folder = {made, sizeof(made), 0ull};
        mmgr_line_put(&folder, filling->root);
        mmgr_line_put(&folder, "/build/run");
        if ((mmgr_line_finish(&folder) == 0ull) || !query_folder_made(made))
        {
            mmgr_line_put(err_msg, "build/run: it could not be made");
            return 0;
        }
        char out[CLI_PATH_MAX_LEN];
        VerbaLinea file = {out, sizeof(out), 0ull};
        mmgr_line_put(&file, made);
        mmgr_line_ch(&file, '/');
        mmgr_line_put_n(&file, filling->name, filling->stem);
#if defined(_WIN32)
        mmgr_line_put(&file, ".exe");
#endif
        mmgr_line_finish(&file);
        cli_run_file_quoted(line, out);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "program", .cap = CLI_CELLS_POOL}))
    {
        cli_run_file_quoted(line, filling->program);
    }
    else
    {
        const char *const names[1] = {key};
        char other[CLI_PATH_MAX_LEN];
        if (!cli_toolchains_program_in(filling->folder, names, 1u, other))
        {
            mmgr_line_put(err_msg, filling->tool_name);
            mmgr_line_put(err_msg, " has no ");
            mmgr_line_put(err_msg, key);
            mmgr_line_put(err_msg, " in ");
            mmgr_line_put(err_msg, filling->folder);
            return 0;
        }
        cli_run_file_quoted(line, other);
    }
    return 1;
}

// the toolchain's template with its names filled in, written into `line`: 1, or 0 with why given
static int cli_template_filled(const CliFilling *filling, const char *template, VerbaLinea *line, VerbaLinea *err_msg)
{
    const char *rest = template;
    for (const char *open = cellul.chr(&(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL, .byte = '{'});
         open != NULL; open = cellul.chr(&(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL, .byte = '{'}))
    {
        // the open brace lies in the rest, at or past its start
        mmgr_line_put_n(line, rest, (unsigned long long)(open - rest));
        const char *const close = cellul.chr(&(CatenaFinitaCfg){.src = open, .cap = CLI_CELLS_POOL, .byte = '}'});
        if (close == NULL)
        {
            mmgr_line_put(err_msg, filling->tool_name);
            mmgr_line_put(err_msg, "'s line for ");
            mmgr_line_put(err_msg, filling->language);
            mmgr_line_put(err_msg, " has an open {");
            return 0;
        }
        // the close brace lies past the open one
        if (!cli_filled(filling, open + 1, (unsigned long long)(close - open) - 1ull, line, err_msg))
        {
            return 0;
        }
        rest = close + 1;
    }
    mmgr_line_put(line, rest);
    return 1;
}

// the line that runs the file found by the ways `named` names, the toolchains in `json` and the PATH's folders in
// `folders`, `count` of them, into `out`: 1, or 0 with why given
static int run_file_found(const char *root, const char *path, const char *language, const CliJson *json,
                          const CliNamed *ways, unsigned int named, const char (*folders)[CLI_PATH_MAX_LEN],
                          unsigned int count, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *name = path;
    unsigned long long stem = 0ull;
    (void)cli_name_split(path, &name, &stem);
    CliFound found_tool;
    unsigned int way = 0u;
    int found = 0;
    for (; !found && (way < named); way += found ? 0u : 1u)
    {
        cli_toolchains_find(json, ways[way].tool, folders, count, &found_tool);
        found = found_tool.has_program;
    }
    if (!found)
    {
        for (unsigned int at = 0u; at < named; at += 1u)
        {
            const char *const shown = cli_json_string(json, cli_json_member(json, ways[at].tool, "name"), NULL);
            mmgr_line_put(err_msg, (at != 0u) ? " or " : "");
            mmgr_line_put(err_msg, (shown != NULL) ? shown : "");
        }
        mmgr_line_put(err_msg,
                      " runs it, and is not found: File, Toolchains opens its install page or takes its folder");
        return 0;
    }
    const char *const tool_name = cli_json_string(json, cli_json_member(json, ways[way].tool, "name"), NULL);
    char folder[CLI_PATH_MAX_LEN];
    if (!cli_path_parent(found_tool.program, folder))
    {
        folder[0] = '\0';
    }
    const CliFilling filling = {root, path,    (tool_name != NULL) ? tool_name : "", found_tool.program, folder, name,
                                stem, language};
    // the line written straight into the answer's string as it is filled
    mmgr_line_put(out, "{\"line\":");
    char piece[1024];
    CliJsonQuoting quoting;
    VerbaLinea run;
    cli_json_quoting_begun(&quoting, out, &run, piece, sizeof(piece));
    mmgr_line_put(&run, "( cd ");
    cli_run_file_quoted(&run, root);
    mmgr_line_put(&run, " && ");
    if (!cli_template_filled(&filling, ways[way].line, &run, err_msg))
    {
        return 0;
    }
    mmgr_line_put(&run, " )");
    cli_json_quoting_ended(&quoting, &run);
    mmgr_line_put(out, ",\"tool\":");
    cli_json_quoted(out, filling.tool_name,
                    cellul.len(&(CatenaFinitaCfg){.src = filling.tool_name, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(out, '}');
    return 1;
}

int cli_run_file_line(const char *root, const char *path, const char *language, VerbaLinea *out, VerbaLinea *err_msg)
{
    CliNamed ways[CLI_RUN_NAMED];
    CliJson json;
    const unsigned int manifest = cli_toolchains_manifest(&json, 0u, 0ull);
    const char *name = path;
    unsigned long long stem = 0ull;
    const char *const extension = cli_name_split(path, &name, &stem);
    const unsigned int named = cli_ways(&json, manifest, language, extension, ways);
    int done = 0;
    if (named == 0u)
    {
        mmgr_line_put(err_msg, "orior_cli has no way to run a ");
        mmgr_line_put(err_msg, language);
        mmgr_line_put(err_msg, " file");
    }
    else
    {
        unsigned int count = 0u;
        char (*const folders)[CLI_PATH_MAX_LEN] = cli_toolchains_path_taken(&count, &g_cli_runs_cells);
        done = run_file_found(root, path, language, &json, ways, named, (const char (*)[CLI_PATH_MAX_LEN])folders,
                              count, out, err_msg);
        g_cli_cells->persistent_buf_release(folders, &g_cli_runs_cells);
    }
    cli_json_given(&json);
    return done;
}
