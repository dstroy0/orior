// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_dotnet.c: what a build that made its program can be reported of it past what it loads: a .NET program
// that needs a .NET runtime a program opened on its own does not find, and the modules a PyInstaller build warns it did
// not find that the tree's own code imports, each with the hidden import that takes it in
#include "../command_line/callback_cfg.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "executables_internal.h"

// the most places the .NET runtime is looked for, and the most numbers of a version read
#define CLI_DOTNET_POSITIONS 8u

#define CLI_DOTNET_NUMBERS 8u

typedef struct
{
    QuerySink sink;
    const char *text;
    char (*positions)[CLI_PATH_MAX_LEN];
    const char *version;
    unsigned long long *numbers;
    const CliJson *json;
    unsigned int framework;
    unsigned int position_count;
    const char *file;
    const char *folder;
    const char *importer;
    unsigned long long length;
} ExecutablesDotnetCtx;
// a line printed through `sink`
EMBED_INLINE void cli_print(const ExecutablesDotnetCtx *args)
{
    EMBED_CALL(args->sink.print, CliEachLineSizeCfg, .context = args->sink.context, .line = args->text,
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL}));
}

// the folders a program opened on its own looks in for the .NET runtime, DOTNET_ROOT where the system sets it, then
// where .NET installs for every user, into `positions`: their count
EMBED_INLINE unsigned int cli_dotnet_positions(const ExecutablesDotnetCtx *args)
{
    unsigned int count = 0u;
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "DOTNET_ROOT",
                   .value_out = args->positions[count], .capacity = CLI_PATH_MAX_LEN))
    {
        count += 1u;
    }
#if defined(_WIN32)
    char files[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ProgramFiles",
                   .value_out = files, .capacity = sizeof(files)) &&
        EMBED_CALL(cli_executables_name_out_folder_internal.join, CliExecutablesNameOutFolderCtx, .folder = files,
                   .name = "dotnet", .out = args->positions[count]))
    {
        count += 1u;
    }
#else
    char text[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_executables_capacity_internal.text_read, CliExecutablesCapacityCtx,
                   .path = "/etc/dotnet/install_location", .bytes = text, .capacity = sizeof(text)))
    {
        const char *from = text;
        unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
        EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx, .text_pointer = &from,
                   .length_pointer = &length);
        EMBED_CALL(cli_executables_length_text_internal.scan_copy, CliExecutablesLengthTextCtx, .text = from,
                   .length = length, .out = args->positions[count], .capacity = CLI_PATH_MAX_LEN);
        count += 1u;
    }
    static const char *const s_usual[3] = {"/usr/share/dotnet", "/usr/lib/dotnet", "/usr/local/share/dotnet"};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = args->positions[count],
                          .src = s_usual[at],
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_usual[at], .cap = CLI_CELLS_POOL}) + 1u});
        count += 1u;
    }
#endif
    return count;
}

// a version's numbers, its parts between dots and dashes that are numbers, into `numbers`: their count
EMBED_INLINE unsigned int cli_numbers(const ExecutablesDotnetCtx *args)
{
    unsigned int count = 0u;
    const char *part = args->version;
    while ((part[0] != '\0') && (count < CLI_DOTNET_NUMBERS))
    {
        const char *end = part;
        end += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = end, .at = 0ull,
                          .size = cellul.len(&(CatenaFinitaCfg){.src = end, .cap = CLI_CELLS_POOL}), .set = ".-");
        const unsigned long long digits = (unsigned long long)(end - part);
        const int number = (digits != 0ull) &&
                           (EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = part,
                                       .at = 0ull, .size = digits, .classes = CLI_TEXT_DIGITS, .set = "") == digits);
        const unsigned long long value = number ? EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = part,
                                                             .at = 0ull, .size = digits, .end = NULL)
                                                : 0ull;
        if (number && (value <= 0xffffffffull))
        {
            args->numbers[count] = value;
            count += 1u;
        }
        part = (end[0] != '\0') ? (end + 1) : end;
    }
    return count;
}

// A framework looked for among a place's versions of it: the numbers wanted, and whether one installed is new enough
typedef struct
{
    unsigned long long required[CLI_DOTNET_NUMBERS];
    unsigned int requested_count;
    int satisfied;
} CliFramework;

// a version a place has: one with the same first number as the one wanted and at least as new satisfies it
static void cli_version_each(const CliEachNameIsFolderCfg *args)
{
    CliFramework *const framework = (CliFramework *)args->context;
    unsigned long long have[CLI_DOTNET_NUMBERS];
    const unsigned int count = EMBED_CALL(cli_numbers, ExecutablesDotnetCtx, .version = args->name, .numbers = have);
    if ((count == 0u) || (framework->requested_count == 0u) || (have[0] != framework->required[0]))
    {
        return;
    }
    // the numbers compared in turn, the longer newer where one runs out
    int order = 0;
    for (unsigned int at = 0u; (order == 0) && (at < count) && (at < framework->requested_count); at += 1u)
    {
        order = (have[at] < framework->required[at]) ? -1 : (have[at] > framework->required[at]) ? 1 : 0;
    }
    order = (order != 0) ? order : ((count < framework->requested_count) ? -1 : 0);
    framework->satisfied = framework->satisfied || (order >= 0);
}

// a framework a runtimeconfig.json names reported where no place has a version of it
static void cli_framework_check_print(const ExecutablesDotnetCtx *args)
{
    const CliJson *json = args->json;
    const char *const name =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->framework, .name = "name"),
                   .length_pointer = NULL);
    const char *const version =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->framework, .name = "version"),
                   .length_pointer = NULL);
    if ((name == NULL) || (version == NULL))
    {
        return;
    }
    CliFramework required;
    required.requested_count =
        EMBED_CALL(cli_numbers, ExecutablesDotnetCtx, .version = version, .numbers = required.required);
    required.satisfied = 0;
    for (unsigned int at = 0u; !required.satisfied && (at < args->position_count); at += 1u)
    {
        char shared[CLI_PATH_MAX_LEN];
        char folder[CLI_PATH_MAX_LEN];
        if (EMBED_CALL(cli_executables_name_out_folder_internal.join, CliExecutablesNameOutFolderCtx,
                       .folder = args->positions[at], .name = "shared", .out = shared) &&
            EMBED_CALL(cli_executables_name_out_folder_internal.join, CliExecutablesNameOutFolderCtx, .folder = shared,
                       .name = name, .out = folder))
        {
            EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = folder,
                       .each = cli_version_each, .context_bytess = &required);
        }
    }
    if (required.satisfied)
    {
        return;
    }
    const char *const base =
        (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->file,
                    .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}),
                    .byte = '/') != NULL)
            ? (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->file,
                          .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}),
                          .byte = '/') +
               1)
        : (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->file,
                      .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}),
                      .byte = '\\') != NULL)
            ? (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->file,
                          .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}),
                          .byte = '\\') +
               1)
            : args->file;
    char print_buf[CLI_PATH_MAX_LEN * (CLI_DOTNET_POSITIONS + 2u)];
    VerbaLinea line = {print_buf, sizeof(print_buf), 0ull};
    mmgr_line_put(&line, base);
    mmgr_line_put(&line, " needs the .NET runtime ");
    mmgr_line_put(&line, name);
    mmgr_line_ch(&line, ' ');
    mmgr_line_put(&line, version);
    mmgr_line_put(&line, ", which a program opened on its own looks for in ");
    for (unsigned int at = 0u; at < args->position_count; at += 1u)
    {
        mmgr_line_put(&line, (at != 0u) ? " and " : "");
        mmgr_line_put(&line, args->positions[at]);
    }
    mmgr_line_put(&line, " and does not find: Publish as One File, the job's output one file, carries it inside the "
                         "program");
    mmgr_line_finish(&line);
    EMBED_CALL(cli_print, ExecutablesDotnetCtx, .sink = args->sink, .text = print_buf);
}

// what a .NET program that is no single file needs of the .NET runtime, reported where a program opened on its own
// finds none: each framework its runtimeconfig.json names, by name and version, that no place has a version of with the
// same first number and at least as new
EMBED_INLINE void cli_dotnet_missing(const ExecutablesDotnetCtx *args)
{
    // the program's path with its extension put as runtimeconfig.json
    char config[CLI_PATH_MAX_LEN];
    const char *const base =
        (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->file,
                    .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}),
                    .byte = '/') != NULL)
            ? (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->file,
                          .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}),
                          .byte = '/') +
               1)
        : (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->file,
                      .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}),
                      .byte = '\\') != NULL)
            ? (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->file,
                          .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}),
                          .byte = '\\') +
               1)
            : args->file;
    const char *const dot =
        (base[0] != '\0')
            ? EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = base + 1,
                         .length_value = cellul.len(&(CatenaFinitaCfg){.src = base + 1, .cap = CLI_CELLS_POOL}),
                         .byte = '.')
            : NULL;
    VerbaLinea named = {config, sizeof(config), 0ull};
    mmgr_line_put_n(&named, args->file,
                    (dot != NULL) ? (unsigned long long)(dot - args->file)
                                  : cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&named, ".runtimeconfig.json");
    if (mmgr_line_finish(&named) == 0ull)
    {
        return;
    }
    CliJson json;
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea reason = {why, sizeof(why), 0ull};
    const unsigned int root =
        EMBED_CALL(cli_json_tree_err_msg.json_file_alloc, CliJsonTreeErrMsgCfg, .json = &json, .path = config,
                   .nodes_more = 0u, .text_more = 0ull, .err_msg = &reason, .owner = &g_cli_executables_cells);
    const unsigned int options = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                            .object = root, .name = "runtimeOptions");
    char (*const positions)[CLI_PATH_MAX_LEN] =
        ((options != CLI_JSON_NONE) &&
         (EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json, .object = options,
                     .name = "includedFrameworks") == CLI_JSON_NONE))
            ? g_cli_cells->persistent_buf_alloc((unsigned long long)CLI_DOTNET_POSITIONS * CLI_PATH_MAX_LEN,
                                                &g_cli_executables_cells)
            : NULL;
    if (positions == NULL)
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
        return;
    }
    const unsigned int position_count = EMBED_CALL(cli_dotnet_positions, ExecutablesDotnetCtx, .positions = positions);
    const unsigned int single = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                           .object = options, .name = "framework");
    if (single != CLI_JSON_NONE)
    {
        EMBED_CALL(cli_framework_check_print, ExecutablesDotnetCtx, .json = &json, .framework = single,
                   .positions = positions, .position_count = position_count, .file = args->file, .sink = args->sink);
    }
    const unsigned int many = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                         .object = options, .name = "frameworks");
    for (unsigned int item = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json,
                                        .node = many, .kind = CFG_JSON_ARRAY)
                                 ? json.node[many].first
                                 : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json.node[item].next)
    {
        EMBED_CALL(cli_framework_check_print, ExecutablesDotnetCtx, .json = &json, .framework = item,
                   .positions = positions, .position_count = position_count, .file = args->file, .sink = args->sink);
    }
    g_cli_cells->persistent_buf_release(positions, &g_cli_executables_cells);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
}

// The tree's own code beside a spec, and what is reported of the modules it imports that PyInstaller did not find
typedef struct
{
    const char *folder;
    QuerySink sink;
} CliOwn;

// whether an importer is the tree's own: a script by its path under the spec's folder, or a module whose first part is
// a package or a Python file of that folder, or the script run
EMBED_INLINE int cli_is_own(const ExecutablesDotnetCtx *args)
{
    const char *folder = args->folder;
    unsigned long long length = args->length;
    const int script =
        (length > 3ull) &&
        (memor.cmp(&(MemoriaCfg){.src = args->importer + length - 3ull, .other = ".py", .bytes = 3u}) == 0);
    const int absolute = (length > 0ull) && ((args->importer[0] == '/') || (args->importer[0] == '\\') ||
                                             ((length > 1ull) && (args->importer[1] == ':')));
    if (script && absolute)
    {
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL});
        return (length >= size) &&
               (memor.cmp(&(MemoriaCfg){.src = args->importer, .other = folder, .bytes = size}) == 0) &&
               ((length == size) || (args->importer[size] == '/') || (args->importer[size] == '\\'));
    }
    unsigned long long top = 0ull;
    top = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->importer, .at = top,
                     .size = length, .set = ".");
    if ((top == 8ull) && (memor.cmp(&(MemoriaCfg){.src = args->importer, .other = "__main__", .bytes = 8u}) == 0))
    {
        return 1;
    }
    char name[CLI_NAME_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char marked[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_executables_length_text_internal.scan_copy, CliExecutablesLengthTextCtx, .text = args->importer,
               .length = top, .out = name, .capacity = sizeof(name));
    VerbaLinea line = {name + top, sizeof(name) - top, 0ull};
    mmgr_line_put(&line, ".py");
    mmgr_line_finish(&line);
    if (EMBED_CALL(cli_executables_name_out_folder_internal.join, CliExecutablesNameOutFolderCtx, .folder = folder,
                   .name = name, .out = path) &&
        (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path) ||
         EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = path)))
    {
        return 1;
    }
    name[top] = '\0';
    return EMBED_CALL(cli_executables_name_out_folder_internal.join, CliExecutablesNameOutFolderCtx, .folder = folder,
                      .name = name, .out = path) &&
           EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = path) &&
           EMBED_CALL(cli_executables_name_out_folder_internal.join, CliExecutablesNameOutFolderCtx, .folder = path,
                      .name = "__init__.py", .out = marked) &&
           EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = marked);
}

// a module PyInstaller did not find reported where the first of its importers that is the tree's own, and imports it
// other than optionally, names it
static void cli_own_each(const CliEachModuleImportersCfg *args)
{
    const CliOwn *const own = (const CliOwn *)args->context;
    const char *one = args->importers;
    const char *const end = args->importers + args->importers_length;
    while (one < end)
    {
        // the importer up to the next `, `
        const char *const start = one;
        const char *stop = one;
        stop = one + EMBED_CALL(cli_text_at_size_first.to_pair, CliTextAtSizeFirstCfg, .text = one, .at = 0ull,
                                .size = (unsigned long long)(end - one), .first = ',', .second = ' ');
        one = (stop < end) ? (stop + 2) : end;
        // `name (kinds)`: the `)` at its end left off, then parted at its last ` (`
        const char *close = stop;
        close = start + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = start,
                                   .at = (unsigned long long)(close - start), .set = ")");
        const char *split = NULL;
        for (const char *at = start; (at + 1) < close; at += 1)
        {
            split = ((at[0] == ' ') && (at[1] == '(')) ? at : split;
        }
        if (split == NULL)
        {
            continue;
        }
        const char *name = start;
        unsigned long long length = (unsigned long long)(split - start);
        EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx, .text_pointer = &name,
                   .length_pointer = &length);
        const char *const kinds = split + 2;
        const unsigned long long kinds_length = (unsigned long long)(close - kinds);
        int optional = 0;
        for (unsigned long long at = 0ull; !optional && ((at + 8ull) <= kinds_length); at += 1ull)
        {
            optional = memor.cmp(&(MemoriaCfg){.src = kinds + at, .other = "optional", .bytes = 8u}) == 0;
        }
        if (optional ||
            !EMBED_CALL(cli_is_own, ExecutablesDotnetCtx, .folder = own->folder, .importer = name, .length = length))
        {
            continue;
        }
        // a script by its file's name, a module by its own
        const char *displayed = name;
        const int script = (length > 3ull) &&
                           (memor.cmp(&(MemoriaCfg){.src = name + length - 3ull, .other = ".py", .bytes = 3u}) == 0);
        for (unsigned long long at = 0ull; script && (at < length); at += 1ull)
        {
            displayed = ((name[at] == '/') || (name[at] == '\\')) ? (name + at + 1) : displayed;
        }
        char print_buf[CLI_PATH_MAX_LEN * 2u];
        VerbaLinea line = {print_buf, sizeof(print_buf), 0ull};
        mmgr_line_put_n(&line, args->module, args->module_length);
        mmgr_line_put(&line, ", which ");
        mmgr_line_put_n(&line, displayed, (unsigned long long)((name + length) - displayed));
        mmgr_line_put(&line, " imports, is not in the frozen program: PyInstaller found it nowhere in the Python that "
                             "ran it; install it there and build again");
        mmgr_line_finish(&line);
        EMBED_CALL(cli_print, ExecutablesDotnetCtx, .sink = own->sink, .text = print_buf);
        return;
    }
}

void cli_executables_built_advice(const CliExecutablesRootCfg *args)
{
    const char *const colon =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->id,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->id, .cap = CLI_CELLS_POOL}), .byte = ':');
    if ((!cellul.starts(&(CatenaFinitaCfg){.src = args->id, .other = "executable/", .cap = CLI_CELLS_POOL})) ||
        (colon == NULL) || (colon < (args->id + 11)))
    {
        return;
    }
    char script[CLI_PATH_MAX_LEN];
    VerbaLinea scripted = {script, sizeof(script), 0ull};
    mmgr_line_put_n(&scripted, args->id + 11, (unsigned long long)(colon - (args->id + 11)));
    mmgr_line_finish(&scripted);
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_executables_name_out_folder_internal.join, CliExecutablesNameOutFolderCtx, .folder = args->root,
               .name = script, .out = path);
    const char *const base =
        (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = script,
                    .length_value = cellul.len(&(CatenaFinitaCfg){.src = script, .cap = CLI_CELLS_POOL}),
                    .byte = '/') != NULL)
            ? (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = script,
                          .length_value = cellul.len(&(CatenaFinitaCfg){.src = script, .cap = CLI_CELLS_POOL}),
                          .byte = '/') +
               1)
            : script;
    const unsigned int kind =
        EMBED_CALL(cli_executables_name_out_folder_internal.kind_get, CliExecutablesNameOutFolderCtx, .name = base);
    if (kind == CLI_KIND_DOTNET)
    {
        char file[CLI_PATH_MAX_LEN];
        char why[CLI_NAME_MAX_LEN];
        VerbaLinea unnamed = {why, sizeof(why), 0ull};
        if (EMBED_CALL(cli_executables_name_out_err_msg_internal.dotnet_artifact, CliExecutablesNameOutErrMsgCtx,
                       .name = colon + 1, .lines = args->lines, .out = file, .err_msg = &unnamed))
        {
            EMBED_CALL(cli_dotnet_missing, ExecutablesDotnetCtx, .file = file, .sink = args->sink);
        }
    }
    else if (kind == CLI_KIND_PYINSTALLER)
    {
        CliOwn own = {"", args->sink};
        char folder[CLI_PATH_MAX_LEN];
        char *const warned = EMBED_CALL(cli_executables_spec_internal.pyinstaller_warn, CliExecutablesSpecCtx,
                                        .spec = path, .folder_out = folder);
        own.folder = folder;
        EMBED_CALL(cli_executables_text_each_internal.pyinstaller_missing, CliExecutablesTextEachCtx,
                   .text = (warned != NULL) ? warned : "", .each_callback = cli_own_each, .context = &own);
        g_cli_cells->persistent_buf_release(warned, &g_cli_executables_cells);
    }
}
