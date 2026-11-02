// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugins_text.c: a plugin laid out as its file lays out it, a sample in its language, and the plugin written to a
// folder of its own
#include "plugins.h"

// the widest line a plugin's file is laid out to
#define CLI_WIDE 110ull

typedef struct
{
    const char *text;
    unsigned long long length;
    const CliJson *json;
    unsigned int node;
    VerbaLinea *out;
    unsigned long long indent;
    unsigned int plugin;
    const char *name;
    const char *dir;
    int replace;
    char *folder;
    VerbaLinea *err_msg;
} PluginsTextCtx;
// the bytes a string takes written as JSON, its quotes with it
EMBED_INLINE unsigned long long cli_quoted_length(const PluginsTextCtx *args)
{
    const char *text = args->text;
    unsigned long long length = args->length;
    char buf[4096];
    VerbaLinea line = {buf, sizeof(buf), 0ull};
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &line, .text = text,
               .length = length);
    if (mmgr_line_ok(&line))
    {
        return line.at;
    }
    // a text too long for the capacity is longer than any line it would fit
    return CLI_WIDE + 1ull;
}

static void cli_write(const PluginsTextCtx *args)
{
    const CliJson *json = args->json;
    unsigned int node = args->node;
    VerbaLinea *out = args->out;
    unsigned long long indent = args->indent;
    const unsigned long long begun = out->at;
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = node, .out = out,
               .indent = 0u, .sorted = 0);
    const unsigned long long flat = out->at - begun;
    const CfgJsonKind kind = json->node[node].kind;
    if ((mmgr_line_ok(out) && ((flat + indent) <= CLI_WIDE)) || ((kind != CFG_JSON_ARRAY) && (kind != CFG_JSON_OBJECT)))
    {
        return;
    }
    out->at = begun;
    const unsigned long long inner = indent + 2ull;
    int strings = kind == CFG_JSON_ARRAY;
    for (unsigned int item = json->node[node].first; strings && (item != CLI_JSON_NONE); item = json->node[item].next)
    {
        strings = json->node[item].kind == CFG_JSON_STRING;
    }
    mmgr_line_put(out, (kind == CFG_JSON_OBJECT) ? "{\n" : "[\n");
    unsigned long long line = 0ull;
    int first = 1;
    for (unsigned int item = json->node[node].first; item != CLI_JSON_NONE; item = json->node[item].next)
    {
        const int last = json->node[item].next == CLI_JSON_NONE;
        if (strings)
        {
            // the words packed onto lines, a line broken where the next would pass the width
            const unsigned long long word =
                EMBED_CALL(cli_quoted_length, PluginsTextCtx, .text = json->text + json->node[item].text,
                           .length = json->node[item].length);
            if (!first && ((inner + line + word + 2ull) > CLI_WIDE))
            {
                mmgr_line_put(out, ",\n");
                line = 0ull;
            }
            else if (!first)
            {
                mmgr_line_put(out, ", ");
                line += 2ull;
            }
            if (line == 0ull)
            {
                for (unsigned long long space = 0ull; space < inner; space += 1ull)
                {
                    mmgr_line_ch(out, ' ');
                }
            }
            EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
                       .text = json->text + json->node[item].text, .length = json->node[item].length);
            line += word;
            first = 0;
            if (last)
            {
                mmgr_line_ch(out, '\n');
            }
            continue;
        }
        for (unsigned long long space = 0ull; space < inner; space += 1ull)
        {
            mmgr_line_ch(out, ' ');
        }
        if (kind == CFG_JSON_OBJECT)
        {
            const char *const key = json->text + json->node[item].key;
            EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = key,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}));
            mmgr_line_put(out, ": ");
        }
        EMBED_CALL(cli_write, PluginsTextCtx, .json = json, .node = item, .out = out, .indent = inner);
        mmgr_line_put(out, last ? "\n" : ",\n");
    }
    for (unsigned long long space = 0ull; space < indent; space += 1ull)
    {
        mmgr_line_ch(out, ' ');
    }
    mmgr_line_ch(out, (kind == CFG_JSON_OBJECT) ? '}' : ']');
}

void cli_plugins_text_get(const CliPluginsJsonOutCfg *args)
{
    EMBED_CALL(cli_write, PluginsTextCtx, .json = args->json, .node = args->plugin, .out = args->out, .indent = 0ull);
    mmgr_line_ch(args->out, '\n');
}

// the first word of the grammar's list `name`, NULL where it has none
EMBED_INLINE const char *cli_first(const PluginsTextCtx *args)
{
    const CliJson *json = args->json;
    const char *name = args->name;
    return EMBED_CALL(
        cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
        .node = EMBED_CALL(
            cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json,
            .array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                     .json_read = json, .object = args->plugin, .name = "grammar"),
                                .name = name),
            .index = 0u),
        .length_pointer = NULL);
}

// A few lines in the new language to open and try: a comment, a keyword, a type, a constant, a string and a number,
// each where the plugin has one
void cli_plugins_sample_get(const CliPluginsJsonOutCfg *args)
{
    const unsigned int comments = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                             .json_read = args->json, .object = args->plugin, .name = "comments");
    const char *const line =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = comments, .name = "line"),
                   .length_pointer = NULL);
    const char *const name =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = args->plugin, .name = "name"),
                   .length_pointer = NULL);
    if (line != NULL)
    {
        mmgr_line_put(args->out, line);
        mmgr_line_ch(args->out, ' ');
        mmgr_line_put(args->out, (name != NULL) ? name : "this file");
        mmgr_line_put(args->out, " opens in this plugin\n");
    }
    const unsigned int block = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = args->json, .object = comments, .name = "block");
    const char *const open =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                   .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                      .json_read = args->json, .array = block, .index = 0u),
                   .length_pointer = NULL);
    const char *const close =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                   .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                      .json_read = args->json, .array = block, .index = 1u),
                   .length_pointer = NULL);
    if ((open != NULL) && (close != NULL))
    {
        mmgr_line_put(args->out, open);
        mmgr_line_put(args->out, " a block comment ");
        mmgr_line_put(args->out, close);
        mmgr_line_ch(args->out, '\n');
    }
    const char *keyword =
        EMBED_CALL(cli_first, PluginsTextCtx, .json = args->json, .plugin = args->plugin, .name = "keywords");
    keyword = (keyword != NULL)
                  ? keyword
                  : EMBED_CALL(cli_first, PluginsTextCtx, .json = args->json, .plugin = args->plugin, .name = "words");
    keyword = (keyword != NULL) ? keyword : "word";
    const char *const kind =
        (EMBED_CALL(cli_first, PluginsTextCtx, .json = args->json, .plugin = args->plugin, .name = "types") != NULL)
            ? EMBED_CALL(cli_first, PluginsTextCtx, .json = args->json, .plugin = args->plugin, .name = "types")
            : "Type";
    const char *const constant =
        (EMBED_CALL(cli_first, PluginsTextCtx, .json = args->json, .plugin = args->plugin, .name = "constants") != NULL)
            ? EMBED_CALL(cli_first, PluginsTextCtx, .json = args->json, .plugin = args->plugin, .name = "constants")
            : "42";
    const char *const quoted = EMBED_CALL(
        cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
        .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = args->json,
                           .array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                               .json_read = args->json, .object = args->plugin, .name = "quotes"),
                           .index = 0u),
        .length_pointer = NULL);
    const char *const quote = (quoted != NULL) ? quoted : "\"";
    mmgr_line_put(args->out, keyword);
    mmgr_line_ch(args->out, ' ');
    mmgr_line_put(args->out, kind);
    mmgr_line_put(args->out, " name = ");
    mmgr_line_put(args->out, constant);
    mmgr_line_put(args->out, ";\n");
    mmgr_line_put(args->out, keyword);
    mmgr_line_put(args->out, " text = ");
    mmgr_line_put(args->out, quote);
    mmgr_line_put(args->out, "a string");
    mmgr_line_put(args->out, quote);
    mmgr_line_put(args->out, ", count = 0x2a + 1.5e3;\n");
}

// the plugin `plugin` of `json` written under `dir` as cli_plugins_create_in writes one: 1, or 0 with why given
EMBED_INLINE int plugins_write(const PluginsTextCtx *args)
{
    const CliJson *json = args->json;
    unsigned int plugin = args->plugin;
    char *folder = args->folder;
    const char *const id =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = plugin, .name = "id"),
                   .length_pointer = NULL);
    char file[CLI_PATH_MAX_LEN];
    VerbaLinea line = {folder, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, args->dir);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, (id != NULL) ? id : "");
    const int folder_fits = mmgr_line_finish(&line) != 0ull;
    VerbaLinea named = {file, sizeof(file), 0ull};
    mmgr_line_put(&named, folder);
    mmgr_line_put(&named, "/plugin.json");
    if (!folder_fits || (mmgr_line_finish(&named) == 0ull))
    {
        mmgr_line_put(args->err_msg, "the plugin's folder is a longer path than orior_cli takes");
        return 0;
    }
    if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = file) && !args->replace)
    {
        mmgr_line_put(args->err_msg, "a plugin ");
        mmgr_line_put(args->err_msg, id);
        mmgr_line_put(args->err_msg, " is there already, in ");
        mmgr_line_put(args->err_msg, folder);
        mmgr_line_put(args->err_msg, ": give another name, or replace it");
        return 0;
    }
    if (!EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder))
    {
        mmgr_line_put(args->err_msg, folder);
        mmgr_line_put(args->err_msg, ": it could not be made");
        return 0;
    }
    char *const text_cell = g_cli_cells->temporary_buf_alloc(CLI_PLUGIN_TEXT_CAPACITY, &g_cli_plugins_cells);
    VerbaLinea text = mmgr_line_over(text_cell, CLI_PLUGIN_TEXT_CAPACITY);
    EMBED_CALL(cli_plugins_json_out.text_get, CliPluginsJsonOutCfg, .json = json, .plugin = plugin, .out = &text);
    int written = mmgr_line_ok(&text) && EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx,
                                                    .path = file, .bytes = text.out, .size = text.at);
    g_cli_cells->persistent_buf_release(text.out, &g_cli_plugins_cells);
    const char *const extension =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json,
                                      .array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                          .json_read = json, .object = plugin, .name = "extensions"),
                                      .index = 0u),
                   .length_pointer = NULL);
    char sample[CLI_PATH_MAX_LEN];
    VerbaLinea sample_name = {sample, sizeof(sample), 0ull};
    mmgr_line_put(&sample_name, folder);
    mmgr_line_put(&sample_name, "/sample.");
    mmgr_line_put(&sample_name, (extension != NULL) ? extension : "txt");
    // 1 where both are written, 0 where plugin.json is not, 2 where the sample is not
    if (written)
    {
        char *const sample_text_cell = g_cli_cells->temporary_buf_alloc(CLI_PLUGIN_TEXT_CAPACITY, &g_cli_plugins_cells);
        VerbaLinea sample_text = mmgr_line_over(sample_text_cell, CLI_PLUGIN_TEXT_CAPACITY);
        EMBED_CALL(cli_plugins_json_out.sample_get, CliPluginsJsonOutCfg, .json = json, .plugin = plugin,
                   .out = &sample_text);
        const int sampled = (mmgr_line_finish(&sample_name) != 0ull) && mmgr_line_ok(&sample_text) &&
                            EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = sample,
                                       .bytes = sample_text.out, .size = sample_text.at);
        g_cli_cells->persistent_buf_release(sample_text.out, &g_cli_plugins_cells);
        written = sampled ? 1 : 2;
    }
    if (written != 1)
    {
        mmgr_line_put(args->err_msg, (written == 0) ? file : sample);
        mmgr_line_put(args->err_msg, ": it could not be written");
        return 0;
    }
    return 1;
}

int cli_plugins_create_in(const CliPluginsErrMsgCfg *args)
{
    CliJson draft = {0};
    const unsigned int answers =
        EMBED_CALL(cli_plugins_err_msg_json_spec.draft_tree, CliPluginsErrMsgJsonSpecCfg, .draft = &draft,
                   .json = args->json, .spec = args->spec, .err_msg = args->err_msg);
    const unsigned int plugin =
        (answers != CLI_JSON_NONE)
            ? EMBED_CALL(cli_plugins_err_msg_json_spec_kind.draft, CliPluginsErrMsgJsonSpecKindCfg, .json_out = &draft,
                         .spec = answers, .err_msg = args->err_msg)
            : CLI_JSON_NONE;
    const int written = (plugin != CLI_JSON_NONE) &&
                        EMBED_CALL(plugins_write, PluginsTextCtx, .dir = args->dir, .json = &draft, .plugin = plugin,
                                   .replace = args->replace, .folder = args->folder, .err_msg = args->err_msg);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &draft);
    return written;
}

int cli_plugins_create(const CliPluginsErrMsgCfg *args)
{
    char dir[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_folder_internal.home_plugins, CliFolderCtx, .folder_out = dir))
    {
        mmgr_line_put(args->err_msg, "orior_cli has no folder of its own to store plugins in");
        return 0;
    }
    return EMBED_CALL(cli_plugins_err_msg.create_in, CliPluginsErrMsgCfg, .dir = dir, .json = args->json,
                      .spec = args->spec, .replace = args->replace, .folder = args->folder, .err_msg = args->err_msg);
}
