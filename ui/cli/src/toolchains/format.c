// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// format.c: a file's language by the plugins, the toolchain that formats a language, and a text formatted by it
#include "../plugins/plugins.h"
#include "../text/text.h"
#include "format_internal.h"

CliFormatWorkState g_cli_format;

// A choice of a file's language: the extension in lower case, and the plugin chosen and where it came from
typedef struct
{
    char extension[CLI_NAME_MAX_LEN];
    char id[CLI_NAME_MAX_LEN];
    int chosen;
} CliLanguageChoice;

static CliLanguageChoice s_choice;

typedef struct
{
    const char *one;
    unsigned long long one_length;
    const char *other;
    const char *word_text;
    const char *file;
    const char *edition;
    unsigned long long *used;
} FormatCtx;
// whether two texts are the same, ASCII's case aside
EMBED_INLINE int cli_same_case_aside(const FormatCtx *args)
{
    if (args->one_length != cellul.len(&(CatenaFinitaCfg){.src = args->other, .cap = CLI_CELLS_POOL}))
    {
        return 0;
    }
    for (unsigned long long at = 0ULL; at < args->one_length; at += 1ULL)
    {
        const char left =
            ((args->one[at] >= 'A') && (args->one[at] <= 'Z')) ? (char)(args->one[at] + 32) : args->one[at];
        const char right =
            ((args->other[at] >= 'A') && (args->other[at] <= 'Z')) ? (char)(args->other[at] + 32) : args->other[at];
        if (left != right)
        {
            return 0;
        }
    }
    return 1;
}

static void cli_plugin_each(const CliEachPluginCfg *args)
{
    CliJson json;
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ULL};
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                   .text = args->plugin->text, .size = args->plugin->size, .nodes_more = 0u, .text_more = 0ULL,
                   .err_msg = &unread, .owner = &g_cli_toolchains_cells);
    const unsigned int extensions = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                               .json_read = &json, .object = root, .name = "extensions");
    int names = 0;
    for (unsigned int at = 0u;
         !names && (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &json,
                               .array = extensions, .index = at) != CLI_JSON_NONE);
         at += 1u)
    {
        unsigned long long length = 0ULL;
        const char *const one =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = &json, .array = extensions, .index = at),
                       .length_pointer = &length);
        names = (one != NULL) && EMBED_CALL(cli_same_case_aside, FormatCtx, .one = one, .one_length = length,
                                            .other = s_choice.extension);
    }
    if (names && (!s_choice.chosen ||
                  (cellul.eq(&(CatenaFinitaCfg){.src = args->plugin->source, .other = "user", .cap = CLI_CELLS_POOL}))))
    {
        memor.cpy(&(MemoriaCfg){
            .dst = s_choice.id,
            .src = args->plugin->id,
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->plugin->id, .cap = CLI_CELLS_POOL}) + 1u});
        s_choice.chosen = 1;
    }
    g_cli_cells->persistent_buf_release(json.node,
                                        EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg, .cell = json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
}

int cli_format_language_get(const CliFormatCfg *args)
{
    const char *name = args->path;
    const unsigned long long name_end =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}), .set = "/\\");
    name = (name_end != 0ULL) ? (args->path + name_end) : name;
    const char *const dot =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = name,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), .byte = '.');
    if ((dot == NULL) || (dot == name))
    {
        return 0;
    }
    (void)EMBED_CALL(cli_text_out_capacity.lowered_into, CliTextOutCapacityCfg, .out_capacity = s_choice.extension,
                     .capacity = CLI_NAME_MAX_LEN, .text = dot + 1);
    s_choice.chosen = 0;
    EMBED_CALL(cli_plugins_each_table.each, CliPluginsEachCfg, .each = cli_plugin_each, .context = NULL);
    const int chosen = s_choice.chosen;
    if (chosen)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = args->id,
                          .src = s_choice.id,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_choice.id, .cap = CLI_CELLS_POOL}) + 1u});
    }
    return chosen;
}

unsigned int cli_format_tool(const CliFormatLanguageCtx *args)
{
    CliJson *const json = &g_cli_format.json;
    g_cli_cells->persistent_buf_release(json->node,
                                        EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg, .cell = json->node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = json, .node_json_node_pointer = NULL,
               .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    const unsigned int manifest = EMBED_CALL(cli_toolchains_json_nodes_more.manifest, CliToolchainsJsonNodesMoreCfg,
                                             .json_out = json, .nodes_more = 0u, .text_more = 0ULL);
    for (unsigned int index = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = json, .array = manifest, .index = index) != CLI_JSON_NONE;
         index += 1u)
    {
        const unsigned int tool = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = json, .array = manifest, .index = index);
        const unsigned int format = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                               .json_read = json, .object = tool, .name = "format");
        const unsigned int formats = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                .json_read = json, .object = tool, .name = "formats");
        for (unsigned int at = 0u; (format != CLI_JSON_NONE) &&
                                   (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                               .json_read = json, .array = formats, .index = at) != CLI_JSON_NONE);
             at += 1u)
        {
            const char *const one =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                           .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                              .json_read = json, .array = formats, .index = at),
                           .length_pointer = NULL);
            if ((one != NULL) &&
                (cellul.eq(&(CatenaFinitaCfg){.src = one, .other = args->language, .cap = CLI_CELLS_POOL})))
            {
                return tool;
            }
        }
    }
    mmgr_line_put(args->err_msg, "orior_cli has no formatter for ");
    mmgr_line_put(args->err_msg, args->language);
    mmgr_line_put(args->err_msg, ": it formats with ");
    unsigned int written = 0u;
    for (unsigned int index = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = json, .array = manifest, .index = index) != CLI_JSON_NONE;
         index += 1u)
    {
        const unsigned int tool = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = json, .array = manifest, .index = index);
        const char *const name =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = tool, .name = "name"),
                       .length_pointer = NULL);
        if ((EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = tool,
                        .name = "format") != CLI_JSON_NONE) &&
            (name != NULL))
        {
            mmgr_line_put(args->err_msg, (written != 0u) ? ", " : "");
            mmgr_line_put(args->err_msg, name);
            written += 1u;
        }
    }
    return CLI_JSON_NONE;
}

int cli_format_program_get(const CliFormatLanguageCfg *args)
{
    char why[CLI_NAME_MAX_LEN * 4u];
    VerbaLinea unknown = {why, sizeof(why), 0ULL};
    const unsigned int tool = EMBED_CALL(cli_format_language_internal.tool, CliFormatLanguageCtx,
                                         .language = args->language, .err_msg = &unknown);
    const char *const named =
        (tool != CLI_JSON_NONE)
            ? EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                         .json_read = &g_cli_format.json,
                         .node = EMBED_CALL(
                             cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &g_cli_format.json,
                             .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                  .json_read = &g_cli_format.json, .object = tool, .name = "format"),
                             .name = "program"),
                         .length_pointer = NULL)
            : NULL;
    if (named != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = args->program,
                                .src = named,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}) + 1u});
    }
    g_cli_cells->persistent_buf_release(
        g_cli_format.json.node, EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg, .cell = g_cli_format.json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&g_cli_format.json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    g_cli_cells->persistent_buf_release(g_cli_format.settings, &g_cli_toolchains_cells);
    g_cli_format.settings = NULL;
    g_cli_format.settings_size = 0ULL;
    return named != NULL;
}

void cli_format_edition_get(const CliFormatCtx *args)
{
    static const char *const s_names[1] = {"Cargo.toml"};
    memor.cpy(&(MemoriaCfg){.dst = args->edition,
                            .src = "2021",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "2021", .cap = CLI_CELLS_POOL}) + 1u});
    if (!EMBED_CALL(cli_format_path_internal.nearest, CliFormatPathCtx, .path = args->path, .names = s_names,
                    .count = 1u))
    {
        return;
    }
    for (const char *line = g_cli_format.settings; line[0] != '\0';)
    {
        const char *end = cellul.chr(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL, .byte = '\n'});
        end = (end != NULL) ? end : (line + cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL}));
        const char *at = line;
        line = (end[0] == '\n') ? (end + 1) : end;
        at += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = at, .at = 0ULL,
                         .size = (unsigned long long)(end - at), .set = " \t");
        if (((end - at) < 7) || (memor.cmp(&(MemoriaCfg){.src = at, .other = "edition", .bytes = 7u}) != 0))
        {
            continue;
        }
        at += 7;
        at += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = at, .at = 0ULL,
                         .size = (unsigned long long)(end - at), .set = " \t");
        if ((at >= end) || (at[0] != '='))
        {
            continue;
        }
        at += 1;
        const char *stop = end;
        at += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = at, .at = 0ULL,
                         .size = (unsigned long long)(stop - at), .set = "  \t\n\v\f\r\"");
        stop = at + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = at,
                               .at = (unsigned long long)(stop - at), .set = "  \t\n\v\f\r\"");
        // the edition lies between its quotes, its end at or past its start
        const unsigned long long length = (unsigned long long)(stop - at);
        if (length < CLI_NAME_MAX_LEN)
        {
            memor.cpy(&(MemoriaCfg){.dst = args->edition, .src = at, .bytes = length});
            args->edition[length] = '\0';
        }
        return;
    }
}

// a formatter's word with `{file}` and `{edition}` written in, put after the words: its start, or NULL where they are
// full
EMBED_INLINE const char *cli_word_fill(const FormatCtx *args)
{
    unsigned long long *used = args->used;
    char *const start = g_cli_format.words + *used;
    VerbaLinea line = {start, CLI_WORDS_TEXT - *used, 0ULL};
    const unsigned long long whole = cellul.len(&(CatenaFinitaCfg){.src = args->word_text, .cap = CLI_CELLS_POOL});
    for (unsigned long long at = 0ULL; at < whole;)
    {
        const unsigned long long stop = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                   .text = args->word_text, .at = at, .size = whole, .set = "{");
        mmgr_line_put_n(&line, args->word_text + at, (size_t)(stop - at));
        at = stop;
        if (stop == whole)
        {
            break;
        }
        if (cellul.starts(&(CatenaFinitaCfg){.src = args->word_text + stop, .other = "{file}", .cap = CLI_CELLS_POOL}))
        {
            mmgr_line_put(&line, args->file);
            at += 6ULL;
        }
        else if (cellul.starts(
                     &(CatenaFinitaCfg){.src = args->word_text + stop, .other = "{edition}", .cap = CLI_CELLS_POOL}))
        {
            mmgr_line_put(&line, args->edition);
            at += 9ULL;
        }
        else
        {
            mmgr_line_ch(&line, '{');
            at += 1ULL;
        }
    }
    mmgr_line_finish(&line);
    if (!mmgr_line_ok(&line))
    {
        return NULL;
    }
    *used += line.at + 1ULL;
    return start;
}

int cli_format(const CliFormatPathCfg *args)
{
    const CliJson *const json = &g_cli_format.json;
    const unsigned int tool = EMBED_CALL(cli_format_language_internal.tool, CliFormatLanguageCtx,
                                         .language = args->language, .err_msg = args->err_msg);
    if (tool == CLI_JSON_NONE)
    {
        g_cli_cells->persistent_buf_release(g_cli_format.json.node, EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg,
                                                                               .cell = g_cli_format.json.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&g_cli_format.json),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        g_cli_cells->persistent_buf_release(g_cli_format.settings, &g_cli_toolchains_cells);
        g_cli_format.settings = NULL;
        g_cli_format.settings_size = 0ULL;
        return 0;
    }
    const unsigned int format = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                           .object = tool, .name = "format");
    const char *const named =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = format, .name = "program"),
                   .length_pointer = NULL);
    const char *const tool_name =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = tool, .name = "name"),
                   .length_pointer = NULL);
    const char *const program_name = (named != NULL) ? named : "";
    unsigned int count = 0u;
    char (*const folders)[CLI_PATH_MAX_LEN] = cli_toolchains_path_allocated(&count, &g_cli_toolchains_cells);
    EMBED_CALL(cli_toolchains_json.find, CliToolchainsJsonCfg, .json = json, .tool = tool,
               .path_callback = (const char (*)[CLI_PATH_MAX_LEN])folders, .count = count,
               .found = &g_cli_format.found);
    g_cli_cells->persistent_buf_release(folders, &g_cli_toolchains_cells);
    char program[CLI_PATH_MAX_LEN];
    const char *const names[1] = {program_name};
    if ((g_cli_format.found.folder[0] == '\0') ||
        !EMBED_CALL(cli_toolchains_dir.program_in, CliToolchainsDirCfg, .dir = g_cli_format.found.folder,
                    .names = names, .count = 1u, .path = program))
    {
        mmgr_line_put(args->err_msg, (tool_name != NULL) ? tool_name : program_name);
        mmgr_line_put(args->err_msg, " is not found: File, Toolchains opens its install page or takes its folder");
        g_cli_cells->persistent_buf_release(g_cli_format.json.node, EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg,
                                                                               .cell = g_cli_format.json.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&g_cli_format.json),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        g_cli_cells->persistent_buf_release(g_cli_format.settings, &g_cli_toolchains_cells);
        g_cli_format.settings = NULL;
        g_cli_format.settings_size = 0ULL;
        return 0;
    }
    const unsigned int arguments = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = format, .name = "args");
    char edition[CLI_NAME_MAX_LEN] = "";
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = arguments, .index = at) != CLI_JSON_NONE;
         at += 1u)
    {
        const char *const word_text =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = arguments, .index = at),
                       .length_pointer = NULL);
        if ((word_text != NULL) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = word_text, .cap = CLI_CELLS_POOL, .other = "{edition}", .other_cap = CLI_CELLS_POOL}) != NULL))
        {
            EMBED_CALL(cli_format_internal.edition_get, CliFormatCtx, .path = args->path, .edition = edition);
            break;
        }
    }
    const char *words[CLI_WORDS_MAX + 2u] = {program};
    unsigned int word_count = 1u;
    unsigned long long used = 0ULL;
    for (unsigned int at = 0u; (word_count <= CLI_WORDS_MAX) &&
                               (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                           .json_read = json, .array = arguments, .index = at) != CLI_JSON_NONE);
         at += 1u)
    {
        const char *const word_text =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = arguments, .index = at),
                       .length_pointer = NULL);
        const char *const filled = (word_text != NULL)
                                       ? EMBED_CALL(cli_word_fill, FormatCtx, .word_text = word_text,
                                                    .file = args->path, .edition = edition, .used = &used)
                                       : NULL;
        if (filled != NULL)
        {
            words[word_count] = filled;
            word_count += 1u;
        }
    }
    words[word_count] = NULL;
    EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = g_cli_format.path,
               .size = sizeof(g_cli_format.path));
    const CliSetting settings[1] = {{"PATH", g_cli_format.path}};
    char folder[CLI_PATH_MAX_LEN];
    const int in_folder =
        EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = args->path, .out = folder) &&
        EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = folder);
    CliRunResult *const run_result = cli_run_result_alloc();
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ULL};
    const int started = EMBED_CALL(cli_words_internal.run_result_within, CliWordsCtx, .words = words,
                                   .folder = in_folder ? folder : NULL, .settings = settings, .count = 1u,
                                   .input = args->text, .length = args->length, .milliseconds_u64 = CLI_FORMAT_PATIENCE,
                                   .run_result = run_result, .err_msg = &unstarted);
    g_cli_cells->persistent_buf_release(
        g_cli_format.json.node, EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg, .cell = g_cli_format.json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&g_cli_format.json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    g_cli_cells->persistent_buf_release(g_cli_format.settings, &g_cli_toolchains_cells);
    g_cli_format.settings = NULL;
    g_cli_format.settings_size = 0ULL;
    const int formatted = started && !run_result->stopped && (run_result->code == 0LL);
    if (!started)
    {
        mmgr_line_put(args->err_msg, program);
        mmgr_line_put(args->err_msg, ": ");
        mmgr_line_put_n(args->err_msg, why, unstarted.at);
    }
    else if (run_result->stopped)
    {
        mmgr_line_put(args->err_msg, program_name);
        mmgr_line_put(args->err_msg, " took longer than 30 s and was stopped");
    }
    else if (!formatted)
    {
        EMBED_CALL(cli_format_err_msg_internal.fail, CliFormatErrMsgCtx, .program = program_name, .path = args->path,
                   .run_result = run_result, .err_msg = args->err_msg);
    }
    else if (!EMBED_CALL(cli_format_bytes_internal.utf8, CliFormatBytesCtx, .bytes = run_result->out,
                         .size = run_result->out_size))
    {
        mmgr_line_put(args->err_msg, program_name);
        mmgr_line_put(args->err_msg, " wrote text that is not UTF-8");
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        return 0;
    }
    else
    {
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                   .text = run_result->out, .length = run_result->out_size);
    }
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    return formatted;
}
