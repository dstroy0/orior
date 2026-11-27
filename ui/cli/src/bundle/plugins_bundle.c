// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugins_bundle.c: the program that builds into the command line what comes with orior: the plugins, each folder
// under ui/plugins that contains a plugin.json by the folder's name, written as plugins_bundled.c, the table
// g_cli_bundled of each one's id and bytes, and a script plugin's code read as the window reads a reader's, in the
// order of their names; the toolchains impero knows, the bytes of toolchains.json as g_cli_toolchains_json; and the
// menus and the command line's words, the bytes of commands.json as g_cli_commands_json; and the host that runs a
// script plugin's code, the bytes of scripts/host.js as g_cli_scripts_host. Run as `plugins_bundle <plugins>
// <toolchains.json> <commands.json> <host.js> <out.c>`. The file is written a piece at a time, each piece's text in a
// cell of the temporary tier sized to the bytes it encodes
#include "../command_line/callback_cfg.h"
#include "../command_line/cli_internal.h"
#include "../plugins/plugins.h"
#include "../text/json_tree.h"
#include "../text/text.h"

// the bundled table of the bundler itself, which runs before plugins_bundled.c exists: empty
const CliBundled g_cli_bundled[1] = {{NULL, NULL, 0ULL, NULL, 0ULL}};
const unsigned int g_cli_bundled_count = 0u;

// A plugin found under the plugins' folder: its id; whether its plugin.json read and was bundled; and, for a script
// plugin, whether its code was bundled and that code's bytes
typedef struct
{
    char id[CLI_NAME_MAX_LEN];
    unsigned long long code_size;
    int bundled;
    int coded;
} CliBundlePlugin;

// the plugins found, in a movable cell of the run grown as the folder's names fill it, in the order of their names
#define CLI_BUNDLE_FIRST (16ULL * sizeof(CliBundlePlugin))

static const CarcerMovableEntry *s_plugins_cell;
static unsigned long long s_plugins_bytes;
static unsigned int s_count;

// the bytes of each byte's text in an array, and of each line's lead over a plain byte's
#define CLI_BUNDLE_BYTE_TEXT 6ULL

#define CLI_BUNDLE_LINE_LEAD 4ULL

#define CLI_BUNDLE_LINE_BYTES 24ULL

// the bytes an array's text takes past its bytes' own and its name's: its head, its tail and its size line
#define CLI_BUNDLE_ARRAY_MORE 128ULL

typedef struct
{
    VerbaLinea *out;
    const char *prefix;
    unsigned int at;
    const unsigned char *bytes;
    unsigned long long size;
    const unsigned char *text;
    const char *plugins;
    const char *folder;
    const char *id;
    char *path;
    const char *path_read;
    const char *name;
    const char *written;
    int fresh;
    unsigned long long needed;
    CliBundlePlugin *plugin;
} PluginsBundleCtx;

// The folder a script plugin's code is read from and the bytes of its files, summed as they are each found
typedef struct
{
    const char *folder;
    unsigned long long bytes;
} CliBundleFolder;

// the plugin at `at` read out of the cell into `plugin`
// `plugin` written into the cell at `at`
// the plugins' cell grown to `needed` bytes where it is less, doubled from its size, its records copied over, the
// cell before given back: 1, or 0 where the cells cannot meet it
EMBED_INLINE int cli_cell_grown(const PluginsBundleCtx *args)
{
    if (args->needed <= s_plugins_bytes)
    {
        return 1;
    }
    const unsigned long long doubled = (s_plugins_bytes != 0ULL) ? (s_plugins_bytes * 2ULL) : CLI_BUNDLE_FIRST;
    const unsigned long long bytes = (doubled < args->needed) ? args->needed : doubled;
    const CarcerMovableEntry *const cell = g_platform_guard->movable_buf_alloc(bytes, g_cli_bundle_owner);
    if (cell == NULL)
    {
        return 0;
    }
    for (unsigned long long at = 0ULL; at < s_plugins_bytes; at += sizeof(CliBundlePlugin))
    {
        CliBundlePlugin record;
        (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = s_plugins_cell, .offset = (embed_word)at, .dst = &record, .bytes = sizeof(record)});
        (void)g_platform_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = cell, .offset = (embed_word)at, .src = &record, .bytes = sizeof(record)});
    }
    if (s_plugins_cell != NULL)
    {
        (void)g_platform_guard->movable_buf_release(s_plugins_cell, g_cli_bundle_owner);
    }
    s_plugins_cell = cell;
    s_plugins_bytes = bytes;
    return 1;
}

// the bytes the text of an array of `size` bytes named with `name` takes
EMBED_INLINE unsigned long long cli_array_text_bytes(const PluginsBundleCtx *args)
{
    const unsigned long long lines = (args->size / CLI_BUNDLE_LINE_BYTES) + 1ULL;
    return (args->size * CLI_BUNDLE_BYTE_TEXT) + (lines * CLI_BUNDLE_LINE_LEAD) +
           (2ULL * cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_NAME_MAX_LEN})) + CLI_BUNDLE_ARRAY_MORE;
}

// the text of `out` put on the end of the file at `written`, or as its start where `fresh`: 1, or 0 where the text
// overran its cell or the file refused it
EMBED_INLINE int cli_appended(const PluginsBundleCtx *args)
{
    return mmgr_line_ok(args->out) &&
           EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = args->written,
                      .bytes = args->out->out, .size = args->out->at, .fresh = args->fresh);
}

// `size` bytes written as the array `prefix` and `at`, ended by a 0 its size leaves out
EMBED_INLINE void cli_bytes_write(const PluginsBundleCtx *args)
{
    mmgr_line_put(args->out, "static const unsigned char ");
    mmgr_line_put(args->out, args->prefix);
    mmgr_line_decimal(args->out, args->at, 1u);
    mmgr_line_put(args->out, "[] = {");
    for (unsigned long long byte = 0ULL; byte < args->size; byte += 1ULL)
    {
        mmgr_line_put(args->out, ((byte % CLI_BUNDLE_LINE_BYTES) == 0ULL) ? "\n    0x" : " 0x");
        mmgr_line_hex(args->out, args->bytes[byte], 2u);
        mmgr_line_ch(args->out, ',');
    }
    mmgr_line_put(args->out, "\n    0x00};\n\n");
}

// `size` bytes written as the array `prefix` and `at` in a cell of their text's size, put on the end of the file at
// `written`: 1, or 0 where the cells or the file refuse
EMBED_INLINE int cli_array_appended(const PluginsBundleCtx *args)
{
    const unsigned long long bytes =
        EMBED_CALL(cli_array_text_bytes, PluginsBundleCtx, .size = args->size, .name = args->prefix);
    char *const text = g_platform_guard->temporary_buf_alloc(bytes, g_cli_bundle_owner);
    if (text == NULL)
    {
        return 0;
    }
    VerbaLinea out = {text, bytes, 0ULL};
    EMBED_CALL(cli_bytes_write, PluginsBundleCtx, .out = &out, .prefix = args->prefix, .at = args->at,
               .bytes = args->bytes, .size = args->size);
    const int appended = EMBED_CALL(cli_appended, PluginsBundleCtx, .out = &out, .written = args->written, .fresh = 0);
    g_platform_guard->persistent_buf_release(text, g_cli_bundle_owner);
    return appended;
}

// whether the plugin.json `text`, `size` bytes, is a script plugin's
EMBED_INLINE int cli_script(const PluginsBundleCtx *args)
{
    const unsigned char *text = args->text;
    unsigned long long size = args->size;
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const unsigned int root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg,
                                         .json = &json, .text = (const char *)text, .size = size, .nodes_more = 0u,
                                         .text_more = 0ULL, .err_msg = &err_msg, .owner = g_cli_bundle_owner);
    const char *const type =
        (root != CLI_JSON_NONE)
            ? EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                         .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                            .object = root, .name = "type"),
                         .length_pointer = NULL)
            : NULL;
    const int script =
        (type != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = type, .other = "script", .cap = CLI_NAME_MAX_LEN});
    g_platform_guard->persistent_buf_release(json.node, mmgr_owner_of_buf(platform_pool_series(), json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return script;
}

// Each file of a script plugin's folder: its bytes summed, which bound the code the folder's files make
static void cli_code_file_each(const CliEachNameIsFolderCfg *args)
{
    CliBundleFolder *const folder = args->context;
    if (args->is_folder)
    {
        return;
    }
    char file[CLI_PATH_MAX_LEN];
    VerbaLinea line = {file, sizeof(file), 0ULL};
    mmgr_line_put(&line, folder->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->name);
    unsigned long long size = 0ULL;
    if ((mmgr_line_finish(&line) != 0ULL) &&
        EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = file, .size_pointer = &size))
    {
        folder->bytes += size + 1ULL;
    }
}

// the code of the script plugin at `at`, `plugin`, under `plugins`, whose plugin.json is `size` bytes of `text`, read
// into a cell of its folder's files' size, written as the array s_code_ and `at` on the end of the file at `written`,
// and recorded in `plugin` as written: 1, or 0 where it does not read
EMBED_INLINE int cli_code_bundle(const PluginsBundleCtx *args)
{
    CliBundleFolder folder = {NULL, 0ULL};
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ULL};
    mmgr_line_put(&line, args->plugins);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->plugin->id);
    if (mmgr_line_finish(&line) == 0ULL)
    {
        return 0;
    }
    folder.folder = path;
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = path, .each = cli_code_file_each,
               .context_bytess = &folder);
    char *const code_text = g_platform_guard->temporary_buf_alloc(folder.bytes + 1ULL, g_cli_bundle_owner);
    if (code_text == NULL)
    {
        return 0;
    }
    VerbaLinea code = {code_text, folder.bytes + 1ULL, 0ULL};
    char why[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const int read =
        EMBED_CALL(cli_plugins_id.code_in, CliPluginsIdCfg, .id = args->plugin->id, .folder_read = path,
                   .text = (const char *)args->text, .size = args->size, .out = &code, .err_msg = &err_msg) &&
        mmgr_line_ok(&code) &&
        EMBED_CALL(cli_array_appended, PluginsBundleCtx, .prefix = "s_code_", .at = args->at,
                   .bytes = (const unsigned char *)code_text, .size = code.at, .written = args->written);
    g_platform_guard->persistent_buf_release(code_text, g_cli_bundle_owner);
    args->plugin->coded = read;
    args->plugin->code_size = read ? code.at : 0ULL;
    return read;
}

// Each folder under the plugins' folder taken, recorded in the order of its name, the cell grown to hold it
static void cli_folder_each(const CliEachNameIsFolderCfg *args)
{
    if (!args->is_folder ||
        (cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES}) >= CLI_NAME_MAX_LEN) ||
        !EMBED_CALL(cli_cell_grown, PluginsBundleCtx,
                    .needed = ((unsigned long long)s_count + 1ULL) * sizeof(CliBundlePlugin)))
    {
        return;
    }
    unsigned int place = s_count;
    CliBundlePlugin before;
    while (place > 0u)
    {
        (void)g_platform_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = s_plugins_cell,
                                .offset = (embed_word)(place - 1u) * sizeof(CliBundlePlugin),
                                .dst = &before,
                                .bytes = sizeof(CliBundlePlugin)});
        if (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = before.id, .other_pointer = args->name) <= 0)
        {
            break;
        }
        (void)g_platform_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = s_plugins_cell,
                                .offset = (embed_word)place * sizeof(CliBundlePlugin),
                                .src = &before,
                                .bytes = sizeof(CliBundlePlugin)});
        place -= 1u;
    }
    CliBundlePlugin found = {{0}, 0ULL, 0, 0};
    VerbaLinea id = {found.id, sizeof(found.id), 0ULL};
    mmgr_line_put(&id, args->name);
    (void)mmgr_line_finish(&id);
    (void)g_platform_guard->movable_buf_write(&(CarcerMovableCfg){.cell = s_plugins_cell,
                                                                  .offset = (embed_word)place * sizeof(CliBundlePlugin),
                                                                  .src = &found,
                                                                  .bytes = sizeof(CliBundlePlugin)});
    s_count += 1u;
}

// the file at `folder`/`id`/plugin.json into `path`, sized for CLI_PATH_MAX_LEN
EMBED_INLINE int cli_plugin_path(const PluginsBundleCtx *args)
{
    VerbaLinea line = {args->path, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->id);
    mmgr_line_put(&line, "/plugin.json");
    return mmgr_line_finish(&line) != 0ULL;
}

// the bytes of the file at `path_read` written as the array `name`, ended by a 0 its size leaves out, and its size as
// `name`_size, on the end of the file at `written`: 1, or 0 where the file could not be read or the text refused
EMBED_INLINE int cli_file_bundle(const PluginsBundleCtx *args)
{
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = args->path_read,
                                                  .size_pointer = &size, .owner = g_cli_bundle_owner);
    if (bytes == NULL)
    {
        return 0;
    }
    const unsigned long long text_bytes =
        EMBED_CALL(cli_array_text_bytes, PluginsBundleCtx, .size = size, .name = args->name);
    char *const text = g_platform_guard->temporary_buf_alloc(text_bytes, g_cli_bundle_owner);
    if (text == NULL)
    {
        g_platform_guard->persistent_buf_release((void *)(embed_word)bytes, g_cli_bundle_owner);
        return 0;
    }
    VerbaLinea out = {text, text_bytes, 0ULL};
    mmgr_line_put(&out, "const unsigned char ");
    mmgr_line_put(&out, args->name);
    mmgr_line_put(&out, "[] = {");
    for (unsigned long long byte = 0ULL; byte < size; byte += 1ULL)
    {
        mmgr_line_put(&out, ((byte % CLI_BUNDLE_LINE_BYTES) == 0ULL) ? "\n    0x" : " 0x");
        mmgr_line_hex(&out, bytes[byte], 2u);
        mmgr_line_ch(&out, ',');
    }
    mmgr_line_put(&out, "\n    0x00};\n\nconst unsigned long long ");
    mmgr_line_put(&out, args->name);
    mmgr_line_put(&out, "_size = ");
    mmgr_line_decimal(&out, size, 1u);
    mmgr_line_put(&out, "ull;\n");
    const int appended = EMBED_CALL(cli_appended, PluginsBundleCtx, .out = &out, .written = args->written, .fresh = 0);
    g_platform_guard->persistent_buf_release(text, g_cli_bundle_owner);
    g_platform_guard->persistent_buf_release((void *)(embed_word)bytes, g_cli_bundle_owner);
    return appended;
}

// the plugin at `at` bundled: its plugin.json's bytes as the array s_plugin_ and `at`, then its code where it is a
// script plugin's, on the end of the file at `written`: 1, or 0 where a script's code does not read, which stops the
// build; a plugin.json that does not read is recorded as not bundled
EMBED_INLINE int cli_plugin_bundle(const PluginsBundleCtx *args)
{
    CliBundlePlugin plugin;
    (void)g_platform_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = s_plugins_cell,
                            .offset = (embed_word)args->at * sizeof(CliBundlePlugin),
                            .dst = &plugin,
                            .bytes = sizeof(CliBundlePlugin)});
    char path[CLI_PATH_MAX_LEN];
    unsigned long long size = 0ULL;
    const unsigned char *const bytes =
        EMBED_CALL(cli_plugin_path, PluginsBundleCtx, .folder = args->plugins, .id = plugin.id, .path = path)
            ? EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path, .size_pointer = &size,
                         .owner = g_cli_bundle_owner)
            : NULL;
    if (bytes == NULL)
    {
        plugin.bundled = 0;
        (void)g_platform_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = s_plugins_cell,
                                .offset = (embed_word)args->at * sizeof(CliBundlePlugin),
                                .src = &plugin,
                                .bytes = sizeof(CliBundlePlugin)});
        return 1;
    }
    int done = EMBED_CALL(cli_array_appended, PluginsBundleCtx, .prefix = "s_plugin_", .at = args->at, .bytes = bytes,
                          .size = size, .written = args->written);
    // a script plugin whose code does not read is not built
    if (done && EMBED_CALL(cli_script, PluginsBundleCtx, .text = bytes, .size = size))
    {
        done = EMBED_CALL(cli_code_bundle, PluginsBundleCtx, .plugins = args->plugins, .at = args->at,
                          .plugin = &plugin, .text = bytes, .size = size, .written = args->written);
    }
    g_platform_guard->persistent_buf_release((void *)(embed_word)bytes, g_cli_bundle_owner);
    plugin.bundled = done;
    (void)g_platform_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = s_plugins_cell,
                            .offset = (embed_word)args->at * sizeof(CliBundlePlugin),
                            .src = &plugin,
                            .bytes = sizeof(CliBundlePlugin)});
    return done;
}

// the table g_cli_bundled, a line for each plugin bundled, and its count, in a cell sized to the plugins' count, on the
// end of the file at `written`: 1, or 0 where the cells or the file refuse
EMBED_INLINE int cli_table_appended(const PluginsBundleCtx *args)
{
    const unsigned long long bytes =
        (((unsigned long long)s_count + 1ULL) * (CLI_NAME_MAX_LEN + CLI_BUNDLE_ARRAY_MORE)) + CLI_BUNDLE_ARRAY_MORE;
    char *const text = g_platform_guard->temporary_buf_alloc(bytes, g_cli_bundle_owner);
    if (text == NULL)
    {
        return 0;
    }
    VerbaLinea out = {text, bytes, 0ULL};
    mmgr_line_put(&out, "const CliBundled g_cli_bundled[] = {\n");
    unsigned int bundled = 0u;
    for (unsigned int at = 0u; at < s_count; at += 1u)
    {
        CliBundlePlugin plugin;
        (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_plugins_cell,
                                                                     .offset = (embed_word)at * sizeof(CliBundlePlugin),
                                                                     .dst = &plugin,
                                                                     .bytes = sizeof(CliBundlePlugin)});
        if (!plugin.bundled)
        {
            continue;
        }
        char path[CLI_PATH_MAX_LEN];
        unsigned long long size = 0ULL;
        EMBED_CALL(cli_plugin_path, PluginsBundleCtx, .folder = args->plugins, .id = plugin.id, .path = path);
        EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = path, .size_pointer = &size);
        mmgr_line_put(&out, "    {\"");
        mmgr_line_put(&out, plugin.id);
        mmgr_line_put(&out, "\", s_plugin_");
        mmgr_line_decimal(&out, at, 1u);
        mmgr_line_put(&out, ", ");
        mmgr_line_decimal(&out, size, 1u);
        if (plugin.coded)
        {
            mmgr_line_put(&out, "ull, s_code_");
            mmgr_line_decimal(&out, at, 1u);
            mmgr_line_put(&out, ", ");
            mmgr_line_decimal(&out, plugin.code_size, 1u);
            mmgr_line_put(&out, "ull},\n");
        }
        else
        {
            mmgr_line_put(&out, "ull, NULL, 0ULL},\n");
        }
        bundled += 1u;
    }
    mmgr_line_put(&out, (bundled == 0u) ? "    {\"\", NULL, 0ULL, NULL, 0ULL}};\n\n" : "};\n\n");
    mmgr_line_put(&out, "const unsigned int g_cli_bundled_count = ");
    mmgr_line_decimal(&out, bundled, 1u);
    mmgr_line_put(&out, "u;\n\n");
    const int appended = EMBED_CALL(cli_appended, PluginsBundleCtx, .out = &out, .written = args->written, .fresh = 0);
    g_platform_guard->persistent_buf_release(text, g_cli_bundle_owner);
    return appended;
}

int main(int count, char **words)
{
    static const char s_head[] =
        "// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational\n"
        "// plugins_bundled.c: the plugins that come with orior, each one's id and the bytes of its "
        "plugin.json, and the bytes\n// of toolchains.json, commands.json and scripts/host.js, written by "
        "bundle/plugins_bundle.c as the program is built\n"
        "#include \"command_line/commands.h\"\n#include \"plugins/plugins.h\"\n#include "
        "\"scripts/scripts.h\"\n#include \"toolchains/toolchains.h\"\n\n";
    (void)cli_pool_made(CLI_POOL_BYTES);
    if (count != 6)
    {
        return 2;
    }
    const char *const written = words[5];
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = words[1], .each = cli_folder_each,
               .context_bytess = NULL);
    if (!EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = written, .bytes = s_head,
                    .size = sizeof(s_head) - 1ULL, .fresh = 1))
    {
        return 1;
    }
    for (unsigned int at = 0u; at < s_count; at += 1u)
    {
        if (!EMBED_CALL(cli_plugin_bundle, PluginsBundleCtx, .plugins = words[1], .at = at, .written = written))
        {
            return 1;
        }
    }
    if (!EMBED_CALL(cli_table_appended, PluginsBundleCtx, .plugins = words[1], .written = written) ||
        !EMBED_CALL(cli_file_bundle, PluginsBundleCtx, .path_read = words[2], .name = "g_cli_toolchains_json",
                    .written = written) ||
        !EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = written, .bytes = "\n", .size = 1ULL,
                    .fresh = 0) ||
        !EMBED_CALL(cli_file_bundle, PluginsBundleCtx, .path_read = words[3], .name = "g_cli_commands_json",
                    .written = written) ||
        !EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = written, .bytes = "\n", .size = 1ULL,
                    .fresh = 0) ||
        !EMBED_CALL(cli_file_bundle, PluginsBundleCtx, .path_read = words[4], .name = "g_cli_scripts_host",
                    .written = written))
    {
        return 1;
    }
    if (s_plugins_cell != NULL)
    {
        (void)g_platform_guard->movable_buf_release(s_plugins_cell, g_cli_bundle_owner);
    }
    return 0;
}
