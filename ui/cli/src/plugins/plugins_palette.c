// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugins_palette.c: a language plugin's palette, the color each of its grammar's token names takes, read from the
// plugin.json of the plugin that is under its id and passed out one color at a time
#include "../command_line/cli_internal.h"
#include "../command_line/pool.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "plugins.h"

// The plugin sought: its id, where each color goes, and whether a plugin of the id is with a palette
typedef struct
{
    const char *id;
    CliPluginColorEach each;
    void *context;
    int found;
} PaletteNeedle;

// the plugin's palette read where its id is the one sought: each member of the `palette` object whose value is a
// string passed on as a color, in the order the plugin writes them
static void cli_palette_each(const CliEachPluginCfg *args)
{
    PaletteNeedle *const needle = (PaletteNeedle *)args->context;
    if (needle->found ||
        !cellul.eq(&(CatenaFinitaCfg){.src = args->plugin->id, .other = needle->id, .cap = CLI_POOL_BYTES}))
    {
        return;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                   .text = args->plugin->text, .size = args->plugin->size, .nodes_more = 0u, .text_more = 0ULL,
                   .err_msg = &err_msg, .owner = g_cli_plugins_owner);
    if (root == CLI_JSON_NONE)
    {
        return;
    }
    const unsigned int palette = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                            .object = root, .name = "palette");
    if (EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = &json, .node = palette,
                   .type = CFG_JSON_OBJECT))
    {
        needle->found = 1;
        for (unsigned int member = json.node[palette].first; member != CLI_JSON_NONE; member = json.node[member].next)
        {
            const char *const value = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                                 .json_read = &json, .node = member, .length_pointer = NULL);
            if (value != NULL)
            {
                needle->each(&(CliPluginColorCfg){
                    .context = needle->context, .name = json.text + json.node[member].key, .value = value});
            }
        }
    }
    g_cli_guard->persistent_buf_release(json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
}

int cli_plugins_palette_each(const CliPluginsIdEachCfg *args)
{
    PaletteNeedle needle = {args->id, args->each, args->context, 0};
    EMBED_CALL(cli_plugins_each_table.each, CliPluginsEachCfg, .each = cli_palette_each, .context = &needle);
    return needle.found;
}

// A plugin's JSON as a tree: the plugin, the tree, room left in it for `nodes_more` nodes and `text_more` bytes, its
// root, and where the root goes as the tree is read
typedef struct
{
    const CliPlugin *plugin;
    CliJson *json;
    unsigned int nodes_more;
    unsigned long long text_more;
    unsigned int root;
    unsigned int *root_pointer;
} PaletteCtx;

// the plugin's JSON read into the tree: its root, or CLI_JSON_NONE where it does not read
EMBED_INLINE unsigned int cli_palette_tree(const PaletteCtx *args)
{
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = args->json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = args->json,
                      .text = args->plugin->text, .size = args->plugin->size, .nodes_more = args->nodes_more,
                      .text_more = args->text_more, .err_msg = &err_msg, .owner = g_cli_plugins_owner);
}

// the tree's cell given back
EMBED_INLINE void cli_palette_tree_release(const PaletteCtx *args)
{
    g_cli_guard->persistent_buf_release(args->json->node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), args->json->node));
}

// the `palette` object of the tree's root, CLI_JSON_NONE where the root carries none
EMBED_INLINE unsigned int cli_palette_object(const PaletteCtx *args)
{
    const unsigned int root = EMBED_CALL(cli_palette_tree, PaletteCtx, .plugin = args->plugin, .json = args->json,
                                         .nodes_more = args->nodes_more, .text_more = args->text_more);
    if (root == CLI_JSON_NONE)
    {
        return CLI_JSON_NONE;
    }
    if (args->root_pointer != NULL)
    {
        *args->root_pointer = root;
    }
    const unsigned int palette = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                            .json_read = args->json, .object = root, .name = "palette");
    if (!EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = args->json, .node = palette,
                    .type = CFG_JSON_OBJECT))
    {
        EMBED_CALL(cli_palette_tree_release, PaletteCtx, .json = args->json);
        return CLI_JSON_NONE;
    }
    return palette;
}

// each plugin with a palette passed on by its id
static void cli_palettes_each(const CliEachPluginCfg *args)
{
    const CliPluginsEachIdCfg *const asked = (const CliPluginsEachIdCfg *)args->context;
    CliJson json;
    if (EMBED_CALL(cli_palette_object, PaletteCtx, .plugin = args->plugin, .json = &json) == CLI_JSON_NONE)
    {
        return;
    }
    EMBED_CALL(cli_palette_tree_release, PaletteCtx, .json = &json);
    asked->each(&(CliPluginIdCfg){.context = asked->context, .id = args->plugin->id});
}

void cli_plugins_palettes_each(const CliPluginsEachIdCfg *args)
{
    EMBED_CALL(cli_plugins_each_table.each, CliPluginsEachCfg, .each = cli_palettes_each,
               .context = (void *)(embed_word)args);
}

// The color set: the plugin sought, the color's name and its value, and whether the plugin's file was written
typedef struct
{
    const CliPluginsIdNameValueCfg *asked;
    int written;
} PaletteSetNeedle;

// the plugin's JSON written whole under the reader's plugins folder in a folder of the plugin's id, as its file positions
// it out: 1, or 0 where the folder or the file could not be made
EMBED_INLINE int cli_palette_file_write(const PaletteCtx *args)
{
    char dir[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_folder_internal.home_plugins, CliFolderCtx, .folder_out = dir))
    {
        return 0;
    }
    VerbaLinea line = {folder, sizeof(folder), 0ULL};
    mmgr_line_put(&line, dir);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->plugin->id);
    VerbaLinea named = {file, sizeof(file), 0ULL};
    if (mmgr_line_finish(&line) == 0ULL)
    {
        return 0;
    }
    mmgr_line_put(&named, folder);
    mmgr_line_put(&named, "/plugin.json");
    if ((mmgr_line_finish(&named) == 0ULL) ||
        !EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder))
    {
        return 0;
    }
    // the plugin arranged as its file positions it, each value on one line where it fits
    CliGrownLine out;
    cli_grown_begin(&out, CLI_PLUGIN_TEXT_CAPACITY, g_cli_plugins_owner);
    if (out.cell == NULL)
    {
        return 0;
    }
    EMBED_CALL(cli_plugins_json_out.text_get, CliPluginsJsonOutCfg, .json = args->json, .plugin = args->root,
               .out = &out.line);
    const int written =
        mmgr_line_ok(&out.line) && EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx,
                                              .path = file, .bytes = out.cell, .size = cli_grown_size(&out));
    cli_grown_end(&out);
    return written;
}

// the color made in the plugin of the id sought and its file written, each plugin of the id in turn so the reader's
// own is last
static void cli_palette_set_each(const CliEachPluginCfg *args)
{
    PaletteSetNeedle *const needle = (PaletteSetNeedle *)args->context;
    if (!cellul.eq(&(CatenaFinitaCfg){.src = args->plugin->id, .other = needle->asked->id, .cap = CLI_POOL_BYTES}))
    {
        return;
    }
    CliJson json;
    unsigned int root = CLI_JSON_NONE;
    const unsigned long long length =
        cellul.len(&(CatenaFinitaCfg){.src = needle->asked->value, .cap = CLI_NAME_MAX_LEN});
    const unsigned int palette = EMBED_CALL(cli_palette_object, PaletteCtx, .plugin = args->plugin, .json = &json,
                                            .nodes_more = 1u, .text_more = length + 1ULL, .root_pointer = &root);
    if (palette == CLI_JSON_NONE)
    {
        return;
    }
    const unsigned int value = EMBED_CALL(cli_json_tree_text_length_type.json_string_init, CliJsonTreeTextLengthTypeCfg,
                                          .json = &json, .text = needle->asked->value, .length = length);
    if (value != CLI_JSON_NONE)
    {
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = &json, .object = palette,
                   .name = needle->asked->name, .value = value);
        needle->written =
            EMBED_CALL(cli_palette_file_write, PaletteCtx, .plugin = args->plugin, .json = &json, .root = root);
    }
    EMBED_CALL(cli_palette_tree_release, PaletteCtx, .json = &json);
}

int cli_plugins_palette_set(const CliPluginsIdNameValueCfg *args)
{
    PaletteSetNeedle needle = {args, 0};
    EMBED_CALL(cli_plugins_each_table.each, CliPluginsEachCfg, .each = cli_palette_set_each, .context = &needle);
    return needle.written;
}
