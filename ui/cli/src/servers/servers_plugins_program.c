// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_plugins_program.c: what a plugin's server starts as: the program its toolchain finds, and the arguments and
// options it starts with, `{folder}` in each the folder the program was found in
#include "../toolchains/toolchains.h"
#include "servers_internal.h"

// the program a plugin's server starts as, found by its toolchain where `server` names one, into `program`, and the
// folder it was found in into `folder`, each of which holds CLI_PATH_LONGEST: 1, or 0 with why said
int cli_servers_plugin_program(const CliJson *json, unsigned int server, char *program, char *folder, CliLine *said)
{
    const char *const name = cli_json_string(json, cli_json_member(json, server, "program"), NULL);
    const char *const toolchain = cli_json_string(json, cli_json_member(json, server, "toolchain"), NULL);
    folder[0] = '\0';
    if (name == NULL)
    {
        cli_line_text(said, "the plugin's server names no program");
        return 0;
    }
    if (toolchain == NULL)
    {
        CliLine whole = {program, CLI_PATH_LONGEST, 0ull};
        cli_line_text(&whole, name);
        return cli_line_closed(&whole) != 0ull;
    }
    CliJson manifest_json;
    const unsigned int tool =
        cli_toolchains_tool(&manifest_json, cli_toolchains_manifest(&manifest_json, 0u, 0ull), toolchain, said);
    CliFound found = {"missing", {0}, {0}, 0};
    if (tool != CLI_JSON_NONE)
    {
        unsigned int count = 0u;
        char (*const folders)[CLI_PATH_LONGEST] = cli_toolchains_path_taken(&count);
        cli_toolchains_find(&manifest_json, tool, (const char (*)[CLI_PATH_LONGEST])folders, count, &found);
        cli_cell_given(folders);
    }
    cli_json_given(&manifest_json);
    const char *const names[1] = {name};
    if ((tool == CLI_JSON_NONE) || (found.folder[0] == '\0') ||
        !cli_toolchains_program_in(found.folder, names, 1u, program))
    {
        cli_line_text(said, name);
        cli_line_text(said, " is not found: File, Toolchains opens ");
        cli_line_text(said, toolchain);
        cli_line_text(said, "'s install page or takes its folder");
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = folder, .src = found.folder, .bytes = CLI_PATH_LONGEST});
    return 1;
}

// the `length` bytes of `text` written into `written`, each `{folder}` in them written as `folder`, escaped for a JSON
// string where `escaped`
void cli_servers_plugin_folder_put(CliLine *written, const char *text, unsigned long long length, const char *folder,
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
            cli_line_counted(written, folder, folder_length);
        }
        else
        {
            cli_line_character(written, text[at]);
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
        char word[CLI_PATH_LONGEST * 2u];
        CliLine written = {word, sizeof(word), 0ull};
        cli_servers_plugin_folder_put(&written, text,
                                      cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), folder, 0);
        if (cli_line_closed(&written) != 0ull)
        {
            cli_words_put(words, word);
        }
    }
}
