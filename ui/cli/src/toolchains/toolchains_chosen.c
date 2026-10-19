// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// toolchains_chosen.c: the folders the reader gave orior, by tool, kept in toolchains.json in orior's own folder
#include "toolchains_internal.h"

// The folders the reader gave, by tool, read into a tree sized to them with room for one folder more, which
// cli_json_given gives back: the object of them
static unsigned int cli_chosen_read(CliJson *json, char *path)
{
    const int named = cli_toolchains_file("toolchains.json", path);
    return cli_toolchains_file_read(json, named ? path : "", 1u, CLI_PATH_LONGEST + CLI_NAME_LONGEST);
}

// the tree given back, and the lock with it
static void cli_chosen_released(CliJson *json)
{
    cli_json_given(json);
    cli_lock_give(&g_cli_toolchains_lock);
}

int cli_toolchains_chosen(const char *id, char *folder)
{
    CliJson json;
    char path[CLI_PATH_LONGEST];
    cli_lock_take(&g_cli_toolchains_lock);
    const unsigned int chosen = cli_chosen_read(&json, path);
    const char *const given = cli_json_string(&json, cli_json_member(&json, chosen, id), NULL);
    if (given != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = folder,
                                .src = given,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = given, .cap = CLI_CELLS_POOL}) + 1u});
    }
    cli_chosen_released(&json);
    return given != NULL;
}

void cli_toolchains_chosen_each(void (*each)(void *context, const char *id, const char *folder), void *context)
{
    static char s_ids[256][CLI_NAME_LONGEST];
    static char s_folders[256][CLI_PATH_LONGEST];
    CliJson json;
    char path[CLI_PATH_LONGEST];
    unsigned int count = 0u;
    cli_lock_take(&g_cli_toolchains_lock);
    const unsigned int chosen = cli_chosen_read(&json, path);
    cli_json_sorted(&json, chosen);
    for (unsigned int item = json.node[chosen].first; (item != CLI_JSON_NONE) && (count < 256u);
         item = json.node[item].next)
    {
        const char *const folder = cli_json_string(&json, item, NULL);
        if (folder != NULL)
        {
            memor.cpy(
                &(MemoriaCfg){.dst = s_ids[count], .bytes = CLI_NAME_LONGEST, .src = json.text + json.node[item].key});
            memor.cpy(
                &(MemoriaCfg){.dst = s_folders[count],
                              .src = folder,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1u});
            count += 1u;
        }
    }
    cli_chosen_released(&json);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        each(context, s_ids[at], s_folders[at]);
    }
}

int cli_toolchains_choose(const char *id, const char *folder, char *program, CliLine *said)
{
    CliJson manifest_json;
    char dir[CLI_PATH_LONGEST];
    cli_toolchains_trimmed(folder, dir, sizeof(dir));
    const unsigned int manifest = cli_toolchains_manifest(&manifest_json, 0u, 0ull);
    const unsigned int tool = cli_toolchains_tool(&manifest_json, manifest, id, said);
    if (tool == CLI_JSON_NONE)
    {
        cli_json_given(&manifest_json);
        return 0;
    }
    if (!cli_toolchains_tool_program_in(&manifest_json, tool, dir, program))
    {
        cli_line_text(said, dir);
        cli_line_text(said, " holds none of ");
        const unsigned int programs = cli_json_member(&manifest_json, tool, "programs");
        for (unsigned int item = manifest_json.node[programs].first; item != CLI_JSON_NONE;
             item = manifest_json.node[item].next)
        {
            cli_line_text(said, (item != manifest_json.node[programs].first) ? ", " : "");
            const char *const name = cli_json_string(&manifest_json, item, NULL);
            cli_line_text(said, (name != NULL) ? name : "");
        }
        cli_json_given(&manifest_json);
        return 0;
    }
    cli_json_given(&manifest_json);
    CliJson json;
    char path[CLI_PATH_LONGEST];
    cli_lock_take(&g_cli_toolchains_lock);
    const unsigned int chosen = cli_chosen_read(&json, path);
    cli_json_set(&json, chosen, id,
                 cli_json_string_made(&json, dir, cellul.len(&(CatenaFinitaCfg){.src = dir, .cap = CLI_CELLS_POOL})));
    const int kept = (path[0] != '\0') && cli_toolchains_file_kept(&json, chosen, path, 1, said);
    cli_chosen_released(&json);
    if ((path[0] == '\0') && !kept)
    {
        cli_line_text(said, "orior has no folder of its own to keep toolchains in");
    }
    return kept;
}

int cli_toolchains_forget(const char *id, CliLine *said)
{
    CliJson json;
    char path[CLI_PATH_LONGEST];
    cli_lock_take(&g_cli_toolchains_lock);
    const unsigned int chosen = cli_chosen_read(&json, path);
    int kept = 1;
    if (cli_json_member(&json, chosen, id) != CLI_JSON_NONE)
    {
        cli_json_remove(&json, chosen, id);
        kept = cli_toolchains_file_kept(&json, chosen, path, 1, said);
    }
    cli_chosen_released(&json);
    return kept;
}

int cli_toolchains_chosen_program(const char *id, char *path)
{
    CliJson manifest_json;
    char folder[CLI_PATH_LONGEST];
    if (!cli_toolchains_chosen(id, folder))
    {
        return 0;
    }
    char unsaid[CLI_PATH_LONGEST];
    CliLine said = {unsaid, sizeof(unsaid), 0ull};
    const unsigned int tool =
        cli_toolchains_tool(&manifest_json, cli_toolchains_manifest(&manifest_json, 0u, 0ull), id, &said);
    const int found = (tool != CLI_JSON_NONE) && cli_toolchains_tool_program_in(&manifest_json, tool, folder, path);
    cli_json_given(&manifest_json);
    return found;
}
