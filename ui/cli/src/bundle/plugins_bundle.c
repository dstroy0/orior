// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugins_bundle.c: the program that builds into the command line what comes with orior: the plugins, each folder
// under ui/plugins that contains a plugin.json by the folder's name, written as plugins_bundled.c, the table
// g_cli_bundled of each one's id and bytes, and a script plugin's code read as the window reads a reader's, in the
// order of their names; the toolchains orior_cli knows, the bytes of toolchains.json as g_cli_toolchains_json; and the
// menus and the command line's words, the bytes of commands.json as g_cli_commands_json. Run as
// `plugins_bundle <plugins> <toolchains.json> <commands.json> <out.c>`
#include "../command_line/callback_cfg.h"
#include "../command_line/cli_internal.h"
#include "../plugins/plugins.h"
#include "../text/json_tree.h"
#include "../text/text.h"

// the most plugins bundled, the most bytes of one, and the most bytes of the file written
#define CLI_BUNDLE_MAX 512u

#define CLI_BUNDLE_BYTES (1u << 22u)

#define CLI_BUNDLE_OUT (1u << 27u)

// the bundled table of the bundler itself, which runs before plugins_bundled.c exists: empty
const CliBundled g_cli_bundled[1] = {{NULL, NULL, 0ull, NULL, 0ull}};
const unsigned int g_cli_bundled_count = 0u;

static char s_ids[CLI_BUNDLE_MAX][CLI_NAME_MAX_LEN];
static unsigned int s_count;
static unsigned char s_bytes[CLI_BUNDLE_BYTES];
static char s_code[CLI_BUNDLE_BYTES];
static unsigned long long s_code_size[CLI_BUNDLE_MAX];
static int s_coded[CLI_BUNDLE_MAX];
static char s_out[CLI_BUNDLE_OUT];

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
} PluginsBundleCtx;
// `size` bytes written as the array `prefix` and `at`, ended by a 0 its size leaves out
EMBED_INLINE void cli_bytes_write(const PluginsBundleCtx *args)
{
    mmgr_line_put(args->out, "static const unsigned char ");
    mmgr_line_put(args->out, args->prefix);
    mmgr_line_decimal(args->out, args->at, 1u);
    mmgr_line_put(args->out, "[] = {");
    for (unsigned long long byte = 0ull; byte < args->size; byte += 1ull)
    {
        mmgr_line_put(args->out, ((byte % 24ull) == 0ull) ? "\n    0x" : " 0x");
        mmgr_line_hex(args->out, args->bytes[byte], 2u);
        mmgr_line_ch(args->out, ',');
    }
    mmgr_line_put(args->out, "\n    0x00};\n\n");
}

// whether the plugin.json `text`, `size` bytes, is a script plugin's
EMBED_INLINE int cli_script(const PluginsBundleCtx *args)
{
    const unsigned char *text = args->text;
    unsigned long long size = args->size;
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ull);
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const unsigned int root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg,
                                         .json = &json, .text = (const char *)text, .size = size, .nodes_more = 0u,
                                         .text_more = 0ull, .err_msg = &err_msg, .owner = &g_cli_bundle_cells);
    const char *const kind =
        (root != CLI_JSON_NONE)
            ? EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                         .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                            .object = root, .name = "kind"),
                         .length_pointer = NULL)
            : NULL;
    const int script =
        (kind != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "script", .cap = CLI_NAME_MAX_LEN});
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    return script;
}

// the code of the script plugin `id` under `plugins`, whose plugin.json is `size` bytes of `text`, written as the
// array s_code_ and `at`, and stored as written: 1, or 0 where it does not read
EMBED_INLINE int cli_code_bundle(const PluginsBundleCtx *args)
{
    VerbaLinea *out = args->out;
    unsigned long long size = args->size;
    char folder[CLI_PATH_MAX_LEN];
    VerbaLinea line = {folder, sizeof(folder), 0ull};
    mmgr_line_put(&line, args->plugins);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, s_ids[args->at]);
    VerbaLinea code = {s_code, sizeof(s_code), 0ull};
    char why[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if ((mmgr_line_finish(&line) == 0ull) ||
        !EMBED_CALL(cli_plugins_id.code_in, CliPluginsIdCfg, .id = s_ids[args->at], .folder_read = folder,
                    .text = (const char *)s_bytes, .size = size, .out = &code, .err_msg = &err_msg) ||
        !mmgr_line_ok(&code))
    {
        return 0;
    }
    EMBED_CALL(cli_bytes_write, PluginsBundleCtx, .out = out, .prefix = "s_code_", .at = args->at,
               .bytes = (const unsigned char *)s_code, .size = code.at);
    s_coded[args->at] = 1;
    s_code_size[args->at] = code.at;
    return 1;
}

// Each folder under the plugins' folder taken, its name stored in order
static void cli_folder_each(const CliEachNameIsFolderCfg *args)
{
    if (!args->is_folder || (s_count >= CLI_BUNDLE_MAX) ||
        (cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}) >= CLI_NAME_MAX_LEN))
    {
        return;
    }
    unsigned int place = s_count;
    while ((place > 0u) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = s_ids[place - 1u],
                                       .other_pointer = args->name) > 0))
    {
        memor.cpy(&(MemoriaCfg){
            .dst = s_ids[place],
            .src = s_ids[place - 1u],
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_ids[place - 1u], .cap = CLI_CELLS_POOL}) + 1u});
        place -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = s_ids[place],
                            .src = args->name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}) + 1u});
    s_count += 1u;
}

// the file at `folder`/`id`/plugin.json into `path`, sized for CLI_PATH_MAX_LEN
EMBED_INLINE int cli_plugin_path(const PluginsBundleCtx *args)
{
    VerbaLinea line = {args->path, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, args->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->id);
    mmgr_line_put(&line, "/plugin.json");
    return mmgr_line_finish(&line) != 0ull;
}

// the bytes of the file at `path` written as the array `name`, ended by a 0 its size leaves out, and its size as
// `name`_size: 1, or 0 where the file could not be read
EMBED_INLINE int cli_file_bundle(const PluginsBundleCtx *args)
{
    const unsigned long long size = qry_member_file_read(args->path_read, s_bytes, sizeof(s_bytes));
    if (size == QRY_MEMBER_UNREAD)
    {
        return 0;
    }
    mmgr_line_put(args->out, "const unsigned char ");
    mmgr_line_put(args->out, args->name);
    mmgr_line_put(args->out, "[] = {");
    for (unsigned long long byte = 0ull; byte < size; byte += 1ull)
    {
        mmgr_line_put(args->out, ((byte % 24ull) == 0ull) ? "\n    0x" : " 0x");
        mmgr_line_hex(args->out, s_bytes[byte], 2u);
        mmgr_line_ch(args->out, ',');
    }
    mmgr_line_put(args->out, "\n    0x00};\n\nconst unsigned long long ");
    mmgr_line_put(args->out, args->name);
    mmgr_line_put(args->out, "_size = ");
    mmgr_line_decimal(args->out, size, 1u);
    mmgr_line_put(args->out, "ull;\n");
    return 1;
}

int main(int count, char **words)
{
    (void)EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL);
    if (count != 5)
    {
        return 2;
    }
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = words[1], .each = cli_folder_each,
               .context_bytess = NULL);
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    mmgr_line_put(&out,
                  "// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational\n"
                  "// plugins_bundled.c: the plugins that come with orior, each one's id and the bytes of its "
                  "plugin.json, and the bytes\n// of toolchains.json and commands.json, written by "
                  "bundle/plugins_bundle.c as the program is built\n"
                  "#include \"command_line/commands.h\"\n#include \"plugins/plugins.h\"\n#include "
                  "\"toolchains/toolchains.h\"\n\n");
    unsigned int bundled = 0u;
    char path[CLI_PATH_MAX_LEN];
    for (unsigned int at = 0u; at < s_count; at += 1u)
    {
        const unsigned long long size =
            EMBED_CALL(cli_plugin_path, PluginsBundleCtx, .folder = words[1], .id = s_ids[at], .path = path)
                ? qry_member_file_read(path, s_bytes, sizeof(s_bytes))
                : QRY_MEMBER_UNREAD;
        if (size == QRY_MEMBER_UNREAD)
        {
            s_ids[at][0] = '\0';
            continue;
        }
        EMBED_CALL(cli_bytes_write, PluginsBundleCtx, .out = &out, .prefix = "s_plugin_", .at = at, .bytes = s_bytes,
                   .size = size);
        // a script plugin whose code does not read is not built
        if (EMBED_CALL(cli_script, PluginsBundleCtx, .text = s_bytes, .size = size) &&
            !EMBED_CALL(cli_code_bundle, PluginsBundleCtx, .out = &out, .plugins = words[1], .at = at, .size = size))
        {
            return 1;
        }
        bundled += 1u;
    }
    mmgr_line_put(&out, "const CliBundled g_cli_bundled[] = {\n");
    for (unsigned int at = 0u; at < s_count; at += 1u)
    {
        if (s_ids[at][0] == '\0')
        {
            continue;
        }
        EMBED_CALL(cli_plugin_path, PluginsBundleCtx, .folder = words[1], .id = s_ids[at], .path = path);
        unsigned long long size = 0ull;
        EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = path, .size_pointer = &size);
        mmgr_line_put(&out, "    {\"");
        mmgr_line_put(&out, s_ids[at]);
        mmgr_line_put(&out, "\", s_plugin_");
        mmgr_line_decimal(&out, at, 1u);
        mmgr_line_put(&out, ", ");
        mmgr_line_decimal(&out, size, 1u);
        if (s_coded[at])
        {
            mmgr_line_put(&out, "ull, s_code_");
            mmgr_line_decimal(&out, at, 1u);
            mmgr_line_put(&out, ", ");
            mmgr_line_decimal(&out, s_code_size[at], 1u);
            mmgr_line_put(&out, "ull},\n");
        }
        else
        {
            mmgr_line_put(&out, "ull, NULL, 0ull},\n");
        }
    }
    mmgr_line_put(&out, (bundled == 0u) ? "    {\"\", NULL, 0ull, NULL, 0ull}};\n\n" : "};\n\n");
    mmgr_line_put(&out, "const unsigned int g_cli_bundled_count = ");
    mmgr_line_decimal(&out, bundled, 1u);
    mmgr_line_put(&out, "u;\n\n");
    if (!EMBED_CALL(cli_file_bundle, PluginsBundleCtx, .out = &out, .path_read = words[2],
                    .name = "g_cli_toolchains_json"))
    {
        return 1;
    }
    mmgr_line_ch(&out, '\n');
    if (!EMBED_CALL(cli_file_bundle, PluginsBundleCtx, .out = &out, .path_read = words[3],
                    .name = "g_cli_commands_json") ||
        !mmgr_line_ok(&out))
    {
        return 1;
    }
    return EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = words[4],
                      .bytes = s_out, .size = out.at)
               ? 0
               : 1;
}
