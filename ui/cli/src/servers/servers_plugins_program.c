// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_plugins_program.c: what a plugin's server starts as: the program its toolchain finds, and the arguments and
// options it starts with, `{folder}` in each the folder the program was found in
#include "../toolchains/toolchains.h"
#include "servers_internal.h"

// the program a plugin's server starts as, found by its toolchain where `server` names one, into `program`, and the
// folder it was found in into `folder`, each sized for CLI_PATH_MAX_LEN: 1, or 0 with why given
int cli_servers_plugin_program(const CliJson *json, unsigned int server, char *program, char *folder,
                               VerbaLinea *err_msg)
{
    const char *const name = cli_json_string(json, cli_json_member(json, server, "program"), NULL);
    const char *const toolchain = cli_json_string(json, cli_json_member(json, server, "toolchain"), NULL);
    folder[0] = '\0';
    if (name == NULL)
    {
        mmgr_line_put(err_msg, "the plugin's server names no program");
        return 0;
    }
    if (toolchain == NULL)
    {
        VerbaLinea whole = {program, CLI_PATH_MAX_LEN, 0ull};
        mmgr_line_put(&whole, name);
        return mmgr_line_finish(&whole) != 0ull;
    }
    CliJson manifest_json;
    const unsigned int tool =
        cli_toolchains_tool(&manifest_json, cli_toolchains_manifest(&manifest_json, 0u, 0ull), toolchain, err_msg);
    CliFound found = {"missing", {0}, {0}, 0};
    if (tool != CLI_JSON_NONE)
    {
        unsigned int count = 0u;
        char (*const folders)[CLI_PATH_MAX_LEN] = cli_toolchains_path_taken(&count, &g_cli_servers_cells);
        cli_toolchains_find(&manifest_json, tool, (const char (*)[CLI_PATH_MAX_LEN])folders, count, &found);
        cli_cell_release(folders, &g_cli_servers_cells);
    }
    cli_json_given(&manifest_json);
    const char *const names[1] = {name};
    if ((tool == CLI_JSON_NONE) || (found.folder[0] == '\0') ||
        !cli_toolchains_program_in(found.folder, names, 1u, program))
    {
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, " is not found: File, Toolchains opens ");
        mmgr_line_put(err_msg, toolchain);
        mmgr_line_put(err_msg, "'s install page or takes its folder");
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = folder, .src = found.folder, .bytes = CLI_PATH_MAX_LEN});
    return 1;
}

// the `length` bytes of `text` written into `written`, each `{folder}` in them written as `folder`, escaped for a JSON
// string where `escaped`
void cli_servers_plugin_folder_put(VerbaLinea *written, const char *text, unsigned long long length, const char *folder,
                                   int escaped)
{
    const unsigned long long folder_length = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL});
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        const int named =
            ((length - at) >= 8ull) &&
            cellul.starts(&(CatenaFinitaCfg){.src = text + at, .other = "{folder}", .cap = CLI_CELLS_POOL});
        if (named && escaped)
        {
            cli_json_escaped_lossy(written, (const unsigned char *)folder, folder_length);
        }
        else if (named)
        {
            mmgr_line_put_n(written, folder, folder_length);
        }
        else
        {
            mmgr_line_ch(written, text[at]);
        }
        at += named ? 7ull : 0ull;
    }
}

// the arguments of `server` put into `words`, each `{folder}` in one written as `folder`
void cli_servers_plugin_args(const CliJson *json, unsigned int server, const char *folder, CliWords *words)
{
    const unsigned int args = cli_json_member(json, server, "args");
    for (unsigned int item = cli_json_is(json, args, CFG_JSON_ARRAY) ? json->node[args].first : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        const char *const arg = cli_json_string(json, item, NULL);
        const char *const text = (arg != NULL) ? arg : "";
        char word[CLI_PATH_MAX_LEN * 2u];
        VerbaLinea written = {word, sizeof(word), 0ull};
        cli_servers_plugin_folder_put(&written, text,
                                      cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), folder, 0);
        if (mmgr_line_finish(&written) != 0ull)
        {
            cli_words_put(words, word);
        }
    }
}
