// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_scan_more.c: a Go file's package, a .NET project's program, the string a call's keyword is set to, a
// build.zig's executables, the program a PyInstaller spec freezes, and a package.json's bins
#include "../text/json_tree.h"
#include "../text/text.h"
#include "executables_internal.h"

int cli_go_package(const CliExecutablesTextOutCtx *args)
{
    int in_comment = 0;
    const char *line = args->text;
    while (line[0] != '\0')
    {
        const char *stop = line;
        stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ULL,
                           .size = cellul.len(&(CatenaFinitaCfg){.src = stop, .cap = CLI_CELLS_POOL}), .set = "\n");
        const char *const next = (stop[0] != '\0') ? (stop + 1) : stop;
        const char *from = line;
        unsigned long long length = (unsigned long long)(stop - line);
        EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx, .text_pointer = &from,
                   .length_pointer = &length);
        line = next;
        // whether the line closes a comment it is in
        int closes = 0;
        {
            unsigned long long at = 0ULL;
            while (!closes && ((at + 1ULL) < length))
            {
                closes = (from[at] == '*') && (from[at + 1ULL] == '/');
                at += 1ULL;
            }
        }
        if (in_comment)
        {
            in_comment = !closes;
            continue;
        }
        if ((length == 0ULL) || ((length >= 2ULL) && (from[0] == '/') && (from[1] == '/')))
        {
            continue;
        }
        if ((length >= 2ULL) && (from[0] == '/') && (from[1] == '*'))
        {
            in_comment = !closes;
            continue;
        }
        if ((length < 8ULL) || (memor.cmp(&(MemoriaCfg){.src = from, .other = "package ", .bytes = 8u}) != 0))
        {
            return 0;
        }
        const char *word_text = from + 8;
        const char *const end = from + length;
        while ((word_text < end) &&
               EMBED_CALL(cli_executables_internal.scan_white, CliExecutablesCtx, .letter = word_text[0]))
        {
            word_text += 1;
        }
        const char *word_past = word_text;
        while ((word_past < end) &&
               !EMBED_CALL(cli_executables_internal.scan_white, CliExecutablesCtx, .letter = word_past[0]))
        {
            word_past += 1;
        }
        EMBED_CALL(cli_executables_length_text_internal.scan_copy, CliExecutablesLengthTextCtx, .text = word_text,
                   .length = (unsigned long long)(word_past - word_text), .out = args->out,
                   .capacity = CLI_NAME_MAX_LEN);
        return 1;
    }
    return 0;
}

typedef struct
{
    const char *text;
    const char *tag;
    unsigned long long *length;
    unsigned long long length_value;
    const char *word_text;
} ExecutablesScanMoreCtx;
// the text of the first element `tag` contains in an MSBuild project, trimmed: its start, its length into `length`, or
// NULL where it has none
EMBED_INLINE const char *cli_msbuild_value(const ExecutablesScanMoreCtx *args)
{
    const char *text = args->text;
    unsigned long long *length = args->length;
    char open[CLI_NAME_MAX_LEN];
    char close[CLI_NAME_MAX_LEN];
    VerbaLinea opened = {open, sizeof(open), 0ULL};
    VerbaLinea closed = {close, sizeof(close), 0ULL};
    mmgr_line_ch(&opened, '<');
    mmgr_line_put(&opened, args->tag);
    mmgr_line_ch(&opened, '>');
    mmgr_line_put(&closed, "</");
    mmgr_line_put(&closed, args->tag);
    mmgr_line_ch(&closed, '>');
    mmgr_line_finish(&opened);
    mmgr_line_finish(&closed);
    const char *const found =
        cellul.find(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL, .other = open, .other_cap = CLI_CELLS_POOL});
    if (found == NULL)
    {
        return NULL;
    }
    const char *start = found + cellul.len(&(CatenaFinitaCfg){.src = open, .cap = CLI_CELLS_POOL});
    const char *const end = cellul.find(
        &(CatenaFinitaCfg){.src = start, .cap = CLI_CELLS_POOL, .other = close, .other_cap = CLI_CELLS_POOL});
    if (end == NULL)
    {
        return NULL;
    }
    *length = (unsigned long long)(end - start);
    EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx, .text_pointer = &start,
               .length_pointer = length);
    return start;
}

// whether `length` bytes of `text` are `word`, ASCII's case aside
EMBED_INLINE int cli_same(const ExecutablesScanMoreCtx *args)
{
    if (args->length_value != cellul.len(&(CatenaFinitaCfg){.src = args->word_text, .cap = CLI_CELLS_POOL}))
    {
        return 0;
    }
    for (unsigned long long at = 0ULL; at < args->length_value; at += 1ULL)
    {
        const char letter = ((args->text[at] >= 'A') && (args->text[at] <= 'Z')) ? (char)(args->text[at] + ('a' - 'A'))
                                                                                 : args->text[at];
        if (letter != args->word_text[at])
        {
            return 0;
        }
    }
    return 1;
}

int cli_dotnet_program(const CliExecutablesTextOutCtx *args)
{
    unsigned long long length = 0ULL;
    const char *const output = EMBED_CALL(cli_msbuild_value, ExecutablesScanMoreCtx, .text = args->text,
                                          .tag = "OutputType", .length = &length);
    if ((output == NULL) ||
        (!EMBED_CALL(cli_same, ExecutablesScanMoreCtx, .text = output, .length_value = length, .word_text = "exe") &&
         !EMBED_CALL(cli_same, ExecutablesScanMoreCtx, .text = output, .length_value = length, .word_text = "winexe")))
    {
        return 0;
    }
    unsigned long long name_length = 0ULL;
    const char *const name = EMBED_CALL(cli_msbuild_value, ExecutablesScanMoreCtx, .text = args->text,
                                        .tag = "AssemblyName", .length = &name_length);
    int named = (name != NULL) && (name_length != 0ULL);
    {
        unsigned long long at = 0ULL;
        while (named && ((at + 1ULL) < name_length))
        {
            named = !((name[at] == '$') && (name[at + 1ULL] == '('));
            at += 1ULL;
        }
    }
    if (named)
    {
        EMBED_CALL(cli_executables_length_text_internal.scan_copy, CliExecutablesLengthTextCtx, .text = name,
                   .length = name_length, .out = args->out, .capacity = CLI_NAME_MAX_LEN);
        return 1;
    }
    // the project file's name without its extension
    const char *stem = args->project;
    const unsigned long long stem_end =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->project,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->project, .cap = CLI_CELLS_POOL}), .set = "/\\");
    stem = (stem_end != 0ULL) ? (args->project + stem_end) : stem;
    const char *dot = NULL;
    const unsigned long long dot_end =
        (stem[0] != '\0')
            ? EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = stem + 1,
                         .at = cellul.len(&(CatenaFinitaCfg){.src = stem + 1, .cap = CLI_CELLS_POOL}), .set = ".")
            : 0ULL;
    dot = (dot_end != 0ULL) ? (stem + dot_end) : NULL;
    EMBED_CALL(cli_executables_length_text_internal.scan_copy, CliExecutablesLengthTextCtx, .text = stem,
               .length = (dot != NULL) ? (unsigned long long)(dot - stem)
                                       : cellul.len(&(CatenaFinitaCfg){.src = stem, .cap = CLI_CELLS_POOL}),
               .out = args->out, .capacity = CLI_NAME_MAX_LEN);
    return stem[0] != '\0';
}

int cli_keyword(const CliExecutablesLengthCtx *args)
{
    const unsigned long long key_length = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_CELLS_POOL});
    unsigned long long from = 0ULL;
    while ((from + key_length) <= args->length)
    {
        unsigned long long at = from;
        while (((at + key_length) <= args->length) &&
               (memor.cmp(&(MemoriaCfg){.src = args->call + at, .other = args->key, .bytes = key_length}) != 0))
        {
            at += 1ULL;
        }
        if ((at + key_length) > args->length)
        {
            return 0;
        }
        from = at + key_length;
        const char before = (at > 0ULL) ? args->call[at - 1ULL] : ' ';
        if (((before >= 'a') && (before <= 'z')) || ((before >= 'A') && (before <= 'Z')) ||
            ((before >= '0') && (before <= '9')) || (before == '_') || ((unsigned char)before >= 0x80u))
        {
            continue;
        }
        unsigned long long rest = from;
        while ((rest < args->length) &&
               EMBED_CALL(cli_executables_internal.scan_white, CliExecutablesCtx, .letter = args->call[rest]))
        {
            rest += 1ULL;
        }
        if ((rest >= args->length) || (args->call[rest] != '='))
        {
            continue;
        }
        rest += 1ULL;
        while ((rest < args->length) &&
               EMBED_CALL(cli_executables_internal.scan_white, CliExecutablesCtx, .letter = args->call[rest]))
        {
            rest += 1ULL;
        }
        if ((rest >= args->length) || ((args->call[rest] != '"') && (args->call[rest] != '\'')))
        {
            return 0;
        }
        const char quote = args->call[rest];
        unsigned long long end = rest + 1ULL;
        while ((end < args->length) && (args->call[end] != quote))
        {
            end += 1ULL;
        }
        EMBED_CALL(cli_executables_length_text_internal.scan_copy, CliExecutablesLengthTextCtx,
                   .text = args->call + rest + 1ULL, .length = end - rest - 1ULL, .out = args->out,
                   .capacity = CLI_NAME_MAX_LEN);
        return 1;
    }
    return 0;
}

void cli_zig_exes(const CliExecutablesNamesCtx *args)
{
    args->names->count = 0u;
    // each line up to its `//`, the lines joined by a newline, no longer than the text
    const unsigned long long capacity = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL}) + 1ULL;
    char *const code_cell = g_cli_cells->temporary_buf_alloc(capacity, &g_cli_executables_cells);
    if (code_cell == NULL)
    {
        return;
    }
    VerbaLinea code = {code_cell, capacity, 0ULL};
    const char *line = args->text;
    int first = 1;
    while (line[0] != '\0')
    {
        const char *stop = line;
        stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ULL,
                           .size = cellul.len(&(CatenaFinitaCfg){.src = stop, .cap = CLI_CELLS_POOL}), .set = "\n");
        const char *end = ((stop > line) && (stop[-1] == '\r')) ? (stop - 1) : stop;
        const unsigned long long code_size = (unsigned long long)(end - line);
        const unsigned long long comment =
            EMBED_CALL(cli_text_at_size_first.to_pair, CliTextAtSizeFirstCfg, .text = line, .at = 0ULL,
                       .size = code_size, .first = '/', .second = '/');
        end = (comment < code_size) ? (line + comment) : end;
        mmgr_line_put(&code, first ? "" : "\n");
        mmgr_line_put_n(&code, line, (unsigned long long)(end - line));
        line = (stop[0] != '\0') ? (stop + 1) : stop;
        first = 0;
    }
    mmgr_line_finish(&code);
    CliCall *calls = NULL;
    const unsigned int count =
        EMBED_CALL(cli_executables_code_command_internal.calls_alloc, CliExecutablesCodeCommandCtx, .code = code_cell,
                   .command = "addExecutable", .calls_pointer = &calls);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *call = calls[at].text;
        unsigned long long length = calls[at].length;
        while ((length > 0ULL) && EMBED_CALL(cli_executables_internal.scan_white, CliExecutablesCtx, .letter = call[0]))
        {
            call += 1;
            length -= 1ULL;
        }
        char name[CLI_NAME_MAX_LEN];
        name[0] = '\0';
        if ((length > 0ULL) && (call[0] == '"'))
        {
            unsigned long long end = 1ULL;
            end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = call, .at = end,
                             .size = length, .set = "\"");
            EMBED_CALL(cli_executables_length_text_internal.scan_copy, CliExecutablesLengthTextCtx, .text = call + 1,
                       .length = end - 1ULL, .out = name, .capacity = sizeof(name));
        }
        else if (!EMBED_CALL(cli_executables_length_internal.keyword, CliExecutablesLengthCtx, .call = call,
                             .length = length, .key = ".name", .out = name))
        {
            name[0] = '\0';
        }
        if (name[0] != '\0')
        {
            EMBED_CALL(cli_executables_name_names_internal.names_append, CliExecutablesNameNamesCtx,
                       .names = args->names, .name = name,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), .value = NULL);
        }
    }
    g_cli_cells->persistent_buf_release(calls, &g_cli_executables_cells);
    g_cli_cells->persistent_buf_release(code_cell, &g_cli_executables_cells);
}

int cli_spec_init(const CliExecutablesTextExeCtx *args)
{
    char *const code = EMBED_CALL(cli_executables_text_internal.uncommented_alloc, CliExecutablesTextCtx,
                                  .text = args->text, .quote = '\'');
    if (code == NULL)
    {
        return 0;
    }
    int found = EMBED_CALL(cli_executables_code_internal.calls, CliExecutablesCodeCtx, .code = code,
                           .command = "Analysis", .calls = NULL, .capacity_u32 = 0u) != 0u;
    int named = 0;
    CliCall *calls = NULL;
    const unsigned int exes =
        found ? EMBED_CALL(cli_executables_code_command_internal.calls_alloc, CliExecutablesCodeCommandCtx,
                           .code = code, .command = "EXE", .calls_pointer = &calls)
              : 0u;
    {
        unsigned int at = 0u;
        while (!named && (at < exes))
        {
            named = EMBED_CALL(cli_executables_length_internal.keyword, CliExecutablesLengthCtx, .call = calls[at].text,
                               .length = calls[at].length, .key = "name", .out = args->exe_capacity);
            at += 1u;
        }
    }
    g_cli_cells->persistent_buf_release(calls, &g_cli_executables_cells);
    calls = NULL;
    found = found && named;
    char folder[CLI_NAME_MAX_LEN];
    int collected = 0;
    const unsigned int collects =
        found ? EMBED_CALL(cli_executables_code_command_internal.calls_alloc, CliExecutablesCodeCommandCtx,
                           .code = code, .command = "COLLECT", .calls_pointer = &calls)
              : 0u;
    {
        unsigned int at = 0u;
        while (!collected && (at < collects))
        {
            collected = EMBED_CALL(cli_executables_length_internal.keyword, CliExecutablesLengthCtx,
                                   .call = calls[at].text, .length = calls[at].length, .key = "name", .out = folder);
            at += 1u;
        }
    }
    g_cli_cells->persistent_buf_release(calls, &g_cli_executables_cells);
    g_cli_cells->persistent_buf_release(code, &g_cli_executables_cells);
    if (!found)
    {
        return 0;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->exe_capacity, .cap = CLI_CELLS_POOL});
    if ((length >= 4ULL) && (cellul.eq(&(CatenaFinitaCfg){
                                .src = args->exe_capacity + length - 4ULL, .other = ".exe", .cap = CLI_CELLS_POOL})))
    {
        args->exe_capacity[length - 4ULL] = '\0';
    }
    char file[CLI_NAME_MAX_LEN];
    EMBED_CALL(cli_executables_name_out_folder_internal.program_file, CliExecutablesNameOutFolderCtx,
               .name = args->exe_capacity, .out = file);
    VerbaLinea line = {args->path_out, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, "dist/");
    if (collected)
    {
        mmgr_line_put(&line, folder);
        mmgr_line_ch(&line, '/');
    }
    mmgr_line_put(&line, file);
    mmgr_line_finish(&line);
    return 1;
}

void cli_npm_bins(const CliExecutablesNamesTextModuleCtx *args)
{
    int *module = args->module;
    args->names->count = 0u;
    *module = 0;
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const unsigned int package =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                   .text = args->text, .size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL}),
                   .nodes_more = 0u, .text_more = 0ULL, .err_msg = &err_msg, .owner = &g_cli_executables_cells);
    if ((package == CLI_JSON_NONE) || !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                  .json_read = &json, .node = package, .kind = CFG_JSON_OBJECT))
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
        return;
    }
    const char *const kind =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                      .object = package, .name = "type"),
                   .length_pointer = NULL);
    *module = (kind != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "module", .cap = CLI_CELLS_POOL}));
    const char *full = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                                  .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                     .json_read = &json, .object = package, .name = "name"),
                                  .length_pointer = NULL);
    full = (full != NULL) ? full : "";
    const char *const name =
        (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = full,
                    .length_value = cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_CELLS_POOL}),
                    .byte = '/') != NULL)
            ? (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = full,
                          .length_value = cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_CELLS_POOL}),
                          .byte = '/') +
               1)
            : full;
    const unsigned int bin = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                        .object = package, .name = "bin");
    const char *const entry = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                         .json_read = &json, .node = bin, .length_pointer = NULL);
    if ((entry != NULL) && (name[0] != '\0'))
    {
        EMBED_CALL(cli_executables_name_names_internal.names_append, CliExecutablesNameNamesCtx, .names = args->names,
                   .name = name, .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}),
                   .value = entry);
    }
    else if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json, .node = bin,
                        .kind = CFG_JSON_OBJECT))
    {
        for (unsigned int member = json.node[bin].first; member != CLI_JSON_NONE; member = json.node[member].next)
        {
            const char *const script = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                                  .json_read = &json, .node = member, .length_pointer = NULL);
            if (script != NULL)
            {
                const char *const key = json.text + json.node[member].key;
                EMBED_CALL(cli_executables_name_names_internal.names_append, CliExecutablesNameNamesCtx,
                           .names = args->names, .name = key,
                           .length = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}),
                           .value = script);
            }
        }
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
}
