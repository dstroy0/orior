// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugins_bundle.c: the program that builds into the command line what comes with orior: the plugins, each folder
// under ui/plugins that contains a plugin.json by the folder's name, written as plugins_bundled.c, the table
// g_cli_bundled of each one's id and bytes, and a script plugin's code read as the window reads a reader's, in the
// order of their names; the toolchains orior_cli knows, the bytes of toolchains.json as g_cli_toolchains_json; and the
// menus and the command line's words, the bytes of commands.json as g_cli_commands_json. Run as
// `plugins_bundle <plugins> <toolchains.json> <commands.json> <out.c>`
#include "../command_line/cli_internal.h"
#include "../plugins/plugins.h"
#include "../text/json_tree.h"
#include "../text/text.h"

// the most plugins bundled, the most bytes of one, and the most bytes of the file written
#define CLI_BUNDLE_MOST 512u

#define CLI_BUNDLE_BYTES (1u << 22u)

#define CLI_BUNDLE_OUT (1u << 27u)

static char s_ids[CLI_BUNDLE_MOST][CLI_NAME_MAX_LEN];
static unsigned int s_count;
static unsigned char s_bytes[CLI_BUNDLE_BYTES];
static char s_code[CLI_BUNDLE_BYTES];
static unsigned long long s_code_size[CLI_BUNDLE_MOST];
static int s_coded[CLI_BUNDLE_MOST];
static char s_out[CLI_BUNDLE_OUT];

// `size` bytes written as the array `prefix` and `at`, ended by a 0 its size leaves out
static void cli_bytes_written(VerbaLinea *out, const char *prefix, unsigned int at, const unsigned char *bytes,
                              unsigned long long size)
{
    mmgr_line_put(out, "static const unsigned char ");
    mmgr_line_put(out, prefix);
    mmgr_line_decimal(out, at, 1u);
    mmgr_line_put(out, "[] = {");
    for (unsigned long long byte = 0ull; byte < size; byte += 1ull)
    {
        mmgr_line_put(out, ((byte % 24ull) == 0ull) ? "\n    0x" : " 0x");
        mmgr_line_hex(out, bytes[byte], 2u);
        mmgr_line_ch(out, ',');
    }
    mmgr_line_put(out, "\n    0x00};\n\n");
}

// whether the plugin.json `text`, `size` bytes, is a script plugin's
static int cli_script(const unsigned char *text, unsigned long long size)
{
    CliJson json;
    cli_json_begun(&json, NULL, 0u, NULL, 0ull);
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const unsigned int root = cli_json_taken(&json, (const char *)text, size, 0u, 0ull, &err_msg);
    const char *const kind =
        (root != CLI_JSON_NONE) ? cli_json_string(&json, cli_json_member(&json, root, "kind"), NULL) : NULL;
    const int script =
        (kind != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "script", .cap = CLI_NAME_MAX_LEN});
    cli_json_given(&json);
    return script;
}

// the code of the script plugin `id` under `plugins`, whose plugin.json is `size` bytes of `text`, written as the
// array s_code_ and `at`, and stored as written: 1, or 0 where it does not read
static int cli_code_bundled(VerbaLinea *out, const char *plugins, unsigned int at, unsigned long long size)
{
    char folder[CLI_PATH_MAX_LEN];
    VerbaLinea line = {folder, sizeof(folder), 0ull};
    mmgr_line_put(&line, plugins);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, s_ids[at]);
    VerbaLinea code = {s_code, sizeof(s_code), 0ull};
    char why[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if ((mmgr_line_finish(&line) == 0ull) ||
        !cli_plugins_code_in(s_ids[at], folder, (const char *)s_bytes, size, &code, &err_msg) || !mmgr_line_ok(&code))
    {
        return 0;
    }
    cli_bytes_written(out, "s_code_", at, (const unsigned char *)s_code, code.at);
    s_coded[at] = 1;
    s_code_size[at] = code.at;
    return 1;
}

// Each folder under the plugins' folder taken, its name stored in order
static void cli_folder_each(void *context, const char *name, int is_folder)
{
    (void)context;
    if (!is_folder || (s_count >= CLI_BUNDLE_MOST) ||
        (cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) >= CLI_NAME_MAX_LEN))
    {
        return;
    }
    unsigned int place = s_count;
    while ((place > 0u) && (cli_text_order(s_ids[place - 1u], name) > 0))
    {
        memor.cpy(&(MemoriaCfg){
            .dst = s_ids[place],
            .src = s_ids[place - 1u],
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_ids[place - 1u], .cap = CLI_CELLS_POOL}) + 1u});
        place -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = s_ids[place],
                            .src = name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
    s_count += 1u;
}

// the file at `folder`/`id`/plugin.json into `path`, sized for CLI_PATH_MAX_LEN
static int cli_plugin_path(const char *folder, const char *id, char *path)
{
    VerbaLinea line = {path, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, id);
    mmgr_line_put(&line, "/plugin.json");
    return mmgr_line_finish(&line) != 0ull;
}

// the bytes of the file at `path` written as the array `name`, ended by a 0 its size leaves out, and its size as
// `name`_size: 1, or 0 where the file could not be read
static int cli_file_bundled(VerbaLinea *out, const char *path, const char *name)
{
    const unsigned long long size = qry_member_file_read(path, s_bytes, sizeof(s_bytes));
    if (size == QRY_MEMBER_UNREAD)
    {
        return 0;
    }
    mmgr_line_put(out, "const unsigned char ");
    mmgr_line_put(out, name);
    mmgr_line_put(out, "[] = {");
    for (unsigned long long byte = 0ull; byte < size; byte += 1ull)
    {
        mmgr_line_put(out, ((byte % 24ull) == 0ull) ? "\n    0x" : " 0x");
        mmgr_line_hex(out, s_bytes[byte], 2u);
        mmgr_line_ch(out, ',');
    }
    mmgr_line_put(out, "\n    0x00};\n\nconst unsigned long long ");
    mmgr_line_put(out, name);
    mmgr_line_put(out, "_size = ");
    mmgr_line_decimal(out, size, 1u);
    mmgr_line_put(out, "ull;\n");
    return 1;
}

int main(int count, char **words)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    if (count != 5)
    {
        return 2;
    }
    query_folder_each(words[1], cli_folder_each, NULL);
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
        const unsigned long long size = cli_plugin_path(words[1], s_ids[at], path)
                                            ? qry_member_file_read(path, s_bytes, sizeof(s_bytes))
                                            : QRY_MEMBER_UNREAD;
        if (size == QRY_MEMBER_UNREAD)
        {
            s_ids[at][0] = '\0';
            continue;
        }
        cli_bytes_written(&out, "s_plugin_", at, s_bytes, size);
        // a script plugin whose code does not read is not built
        if (cli_script(s_bytes, size) && !cli_code_bundled(&out, words[1], at, size))
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
        cli_plugin_path(words[1], s_ids[at], path);
        unsigned long long size = 0ull;
        cli_file_size(path, &size);
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
    if (!cli_file_bundled(&out, words[2], "g_cli_toolchains_json"))
    {
        return 1;
    }
    mmgr_line_ch(&out, '\n');
    if (!cli_file_bundled(&out, words[3], "g_cli_commands_json") || !mmgr_line_ok(&out))
    {
        return 1;
    }
    return query_file_written(words[4], s_out, out.at) ? 0 : 1;
}
