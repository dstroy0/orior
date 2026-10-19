// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugins_bundle.c: the program that builds into the command line what comes with orior: the plugins, each folder
// under ui/plugins that holds a plugin.json by the folder's name, written as plugins_bundled.c, the table
// g_cli_bundled of each one's id and bytes, in the order of their names; the toolchains orior knows, the bytes of
// toolchains.json as g_cli_toolchains_json; and the menus and the command line's words, the bytes of commands.json as
// g_cli_commands_json. Run as `plugins_bundle <plugins> <toolchains.json> <commands.json> <out.c>`
#include "../command_line/cli_internal.h"
#include "../text/text.h"

// the most plugins bundled, the most bytes of one, and the most bytes of the file written
#define CLI_BUNDLE_MOST 512u

#define CLI_BUNDLE_BYTES (1u << 22u)

#define CLI_BUNDLE_OUT (1u << 27u)

static char s_ids[CLI_BUNDLE_MOST][CLI_NAME_LONGEST];
static unsigned int s_count;
static unsigned char s_bytes[CLI_BUNDLE_BYTES];
static char s_out[CLI_BUNDLE_OUT];

// Each folder under the plugins' folder taken, its name kept in order
static void cli_folder_each(void *context, const char *name, int is_folder)
{
    (void)context;
    if (!is_folder || (s_count >= CLI_BUNDLE_MOST) ||
        (cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) >= CLI_NAME_LONGEST))
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

// the file at `folder`/`id`/plugin.json into `path`, which holds CLI_PATH_LONGEST
static int cli_plugin_path(const char *folder, const char *id, char *path)
{
    CliLine line = {path, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, folder);
    cli_line_character(&line, '/');
    cli_line_text(&line, id);
    cli_line_text(&line, "/plugin.json");
    return cli_line_closed(&line) != 0ull;
}

// the bytes of the file at `path` written as the array `name`, ended by a 0 its size leaves out, and its size as
// `name`_size: 1, or 0 where the file could not be read
static int cli_file_bundled(CliLine *out, const char *path, const char *name)
{
    const unsigned long long size = qry_member_file_read(path, s_bytes, sizeof(s_bytes));
    if (size == QRY_MEMBER_UNREAD)
    {
        return 0;
    }
    cli_line_text(out, "const unsigned char ");
    cli_line_text(out, name);
    cli_line_text(out, "[] = {");
    for (unsigned long long byte = 0ull; byte < size; byte += 1ull)
    {
        cli_line_text(out, ((byte % 24ull) == 0ull) ? "\n    0x" : " 0x");
        cli_line_hex(out, s_bytes[byte], 2u);
        cli_line_character(out, ',');
    }
    cli_line_text(out, "\n    0x00};\n\nconst unsigned long long ");
    cli_line_text(out, name);
    cli_line_text(out, "_size = ");
    cli_line_decimal(out, size, 1u);
    cli_line_text(out, "ull;\n");
    return 1;
}

int main(int count, char **words)
{
    if (count != 5)
    {
        return 2;
    }
    query_folder_each(words[1], cli_folder_each, NULL);
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_line_text(&out,
                  "// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational\n"
                  "// plugins_bundled.c: the plugins that come with orior, each one's id and the bytes of its "
                  "plugin.json, and the bytes\n// of toolchains.json and commands.json, written by "
                  "bundle/plugins_bundle.c as the program is built\n"
                  "#include \"command_line/commands.h\"\n#include \"plugins/plugins.h\"\n#include "
                  "\"toolchains/toolchains.h\"\n\n");
    unsigned int bundled = 0u;
    char path[CLI_PATH_LONGEST];
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
        cli_line_text(&out, "static const unsigned char s_plugin_");
        cli_line_decimal(&out, at, 1u);
        cli_line_text(&out, "[] = {");
        for (unsigned long long byte = 0ull; byte < size; byte += 1ull)
        {
            cli_line_text(&out, ((byte % 24ull) == 0ull) ? "\n    0x" : " 0x");
            cli_line_hex(&out, s_bytes[byte], 2u);
            cli_line_character(&out, ',');
        }
        // a plugin of no bytes still holds one, which its size leaves out
        cli_line_text(&out, (size == 0ull) ? "0x00};\n\n" : "};\n\n");
        bundled += 1u;
    }
    cli_line_text(&out, "const CliBundled g_cli_bundled[] = {\n");
    for (unsigned int at = 0u; at < s_count; at += 1u)
    {
        if (s_ids[at][0] == '\0')
        {
            continue;
        }
        cli_plugin_path(words[1], s_ids[at], path);
        unsigned long long size = 0ull;
        cli_file_size(path, &size);
        cli_line_text(&out, "    {\"");
        cli_line_text(&out, s_ids[at]);
        cli_line_text(&out, "\", s_plugin_");
        cli_line_decimal(&out, at, 1u);
        cli_line_text(&out, ", ");
        cli_line_decimal(&out, size, 1u);
        cli_line_text(&out, "ull},\n");
    }
    cli_line_text(&out, (bundled == 0u) ? "    {\"\", NULL, 0ull}};\n\n" : "};\n\n");
    cli_line_text(&out, "const unsigned int g_cli_bundled_count = ");
    cli_line_decimal(&out, bundled, 1u);
    cli_line_text(&out, "u;\n\n");
    if (!cli_file_bundled(&out, words[2], "g_cli_toolchains_json"))
    {
        return 1;
    }
    cli_line_character(&out, '\n');
    if (!cli_file_bundled(&out, words[3], "g_cli_commands_json") || !cli_line_fits(&out))
    {
        return 1;
    }
    return query_file_written(words[4], s_out, out.at) ? 0 : 1;
}
