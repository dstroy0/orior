// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugins_palette.c: a language plugin's palette, the color each of its grammar's token names takes, read from the
// settings the plugins module keeps for the plugin under its id and passed out one color at a time
#include "../command_line/cli_internal.h"
#include "../command_line/pool.h"
#include "../text/json_tree.h"
#include "plugins.h"

// The plugin sought: where each color goes, and whether the plugin carries a palette
typedef struct
{
    CliPluginColorEach each;
    void *context;
    int found;
} PaletteNeedle;

// the `palette` object of a plugin's tree, CLI_JSON_NONE where the plugin has no tree or carries no palette
EMBED_INLINE unsigned int cli_palette_object(const CliPlugin *plugin)
{
    if (plugin->json == NULL)
    {
        return CLI_JSON_NONE;
    }
    const unsigned int palette = EMBED_CALL(cli_plugins_setting_table.setting, CliPluginsSettingCfg,
                                            .json = plugin->json, .node = plugin->root, .key = "palette");
    return EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = plugin->json,
                      .node = palette, .type = CFG_JSON_OBJECT)
               ? palette
               : CLI_JSON_NONE;
}

// each member of the plugin's `palette` object whose value is a string passed on as a color, in the order the plugin
// writes them
static void cli_palette_each(const CliEachPluginCfg *args)
{
    PaletteNeedle *const needle = (PaletteNeedle *)args->context;
    const CliJson *const json = args->plugin->json;
    const unsigned int palette = cli_palette_object(args->plugin);
    if (palette == CLI_JSON_NONE)
    {
        return;
    }
    needle->found = 1;
    for (unsigned int member = json->node[palette].first; member != CLI_JSON_NONE; member = json->node[member].next)
    {
        const char *const value = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                             .json_read = json, .node = member, .length_pointer = NULL);
        if (value != NULL)
        {
            needle->each(&(CliPluginColorCfg){
                .context = needle->context, .name = json->text + json->node[member].key, .value = value});
        }
    }
}

int cli_plugins_palette_each(const CliPluginsIdEachCfg *args)
{
    // the reader's copy of the plugin, where one exists, takes the place of the one that comes with orior
    PaletteNeedle needle = {args->each, args->context, 0};
    (void)EMBED_CALL(cli_plugins_settings_of_table.of, CliPluginsSettingsOfCfg, .id = args->id,
                     .each = cli_palette_each, .context = &needle);
    return needle.found;
}

// each plugin with a palette passed on by its id
static void cli_palettes_each(const CliEachPluginCfg *args)
{
    const CliPluginsEachIdCfg *const asked = (const CliPluginsEachIdCfg *)args->context;
    if (cli_palette_object(args->plugin) != CLI_JSON_NONE)
    {
        asked->each(&(CliPluginIdCfg){.context = asked->context, .id = args->plugin->id});
    }
}

void cli_plugins_palettes_each(const CliPluginsEachIdCfg *args)
{
    EMBED_CALL(cli_plugins_each_table.each, CliPluginsEachCfg, .each = cli_palettes_each,
               .context = (void *)(embed_word)args);
}

// The plugin's tree written whole as plugin.json in a folder of its id, under the reader's plugins folder. The
// call lays the file out with cli_plugins_text_get. `written` is 1, or 0 where the call cannot make the folder or file
static void cli_palette_file_write(const CliEachPluginCfg *args)
{
    int *const written = (int *)args->context;
    char dir[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    if ((args->plugin->json == NULL) || !EMBED_CALL(cli_folder_internal.home_plugins, CliFolderCtx, .folder_out = dir))
    {
        return;
    }
    VerbaLinea line = {folder, sizeof(folder), 0ULL};
    mmgr_line_put(&line, dir);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->plugin->id);
    VerbaLinea named = {file, sizeof(file), 0ULL};
    if (mmgr_line_finish(&line) == 0ULL)
    {
        return;
    }
    mmgr_line_put(&named, folder);
    mmgr_line_put(&named, "/plugin.json");
    if ((mmgr_line_finish(&named) == 0ULL) ||
        !EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder))
    {
        return;
    }
    // the plugin arranged as its file positions it, each value on one line where it fits
    CliGrownLine out;
    cli_grown_begin(&out, CLI_PLUGIN_TEXT_CAPACITY, g_cli_plugins_owner);
    if (out.cell == NULL)
    {
        return;
    }
    EMBED_CALL(cli_plugins_json_out.text_get, CliPluginsJsonOutCfg, .json = args->plugin->json,
               .plugin = args->plugin->root, .out = &out.line);
    *written = mmgr_line_ok(&out.line) && EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx,
                                                     .path = file, .bytes = out.cell, .size = cli_grown_size(&out));
    cli_grown_end(&out);
}

int cli_plugins_palette_set(const CliPluginsIdNameValueCfg *args)
{
    // the color changes in the kept settings first, and the call then writes the reader's plugin.json from them
    if (!EMBED_CALL(cli_plugins_setting_set_table.set, CliPluginsSettingSetCfg, .id = args->id, .object = "palette",
                    .name = args->name, .value = args->value))
    {
        return 0;
    }
    int written = 0;
    (void)EMBED_CALL(cli_plugins_settings_of_table.of, CliPluginsSettingsOfCfg, .id = args->id,
                     .each = cli_palette_file_write, .context = &written);
    return written;
}
