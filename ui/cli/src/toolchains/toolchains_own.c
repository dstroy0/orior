// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// toolchains_own.c: orior's own folder and whether it is on the PATH, and a folder added to the reader's own PATH
#include "toolchains_internal.h"

int cli_toolchains_own(char *folder, int *on_path, CliLine *said)
{
    char program[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    if (!cli_program_own(program, sizeof(program)))
    {
        cli_line_text(said, "orior's program could not be found");
        return 0;
    }
    if (!query_path_full(program, full, sizeof(full)))
    {
        memor.cpy(&(MemoriaCfg){.dst = full,
                                .src = program,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = program, .cap = CLI_CELLS_POOL}) + 1u});
    }
    char *cut = full + cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_CELLS_POOL});
    while ((cut > full) && (cut[-1] != '/') && (cut[-1] != '\\'))
    {
        cut -= 1;
    }
    if (cut == full)
    {
        cli_line_text(said, "orior's program has no folder");
        return 0;
    }
    cut[-1] = '\0';
    memor.cpy(&(MemoriaCfg){
        .dst = folder, .src = full, .bytes = cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_CELLS_POOL}) + 1u});
    char key[CLI_PATH_LONGEST];
    cli_toolchains_key_of(folder, key);
    cli_lock_take(&g_cli_toolchains_find_lock);
    const unsigned int count = cli_toolchains_find_path();
    *on_path = 0;
    for (unsigned int at = 0u; !*on_path && (at < count); at += 1u)
    {
        char held[CLI_PATH_LONGEST];
        cli_toolchains_key_of(g_cli_toolchains_path[at], held);
        *on_path = cellul.eq(&(CatenaFinitaCfg){.src = held, .other = key, .cap = CLI_CELLS_POOL});
    }
    cli_toolchains_find_released(NULL);
    return 1;
}

// A folder added to the reader's own PATH: on Windows the reader's Path in the registry, kept as text whose variables
// expand, and every open program told it changed; elsewhere a line at the end of ~/.profile, and of ~/.zprofile on
// macOS, and of ~/.bash_profile and ~/.bash_login where they are there
static int cli_add_folder(const char *folder, CliLine *said)
{
#if defined(_WIN32)
    static char s_now[CLI_PATH_TEXT];
    static char s_next[CLI_PATH_TEXT];
    if (!cli_registry_user_value("Path", s_now, sizeof(s_now)))
    {
        s_now[0] = '\0';
    }
    if (cli_toolchains_joined(s_now, folder, s_next, sizeof(s_next)))
    {
        if (!cli_registry_set_user_value("Path", s_next, said))
        {
            return 0;
        }
        cli_registry_announce();
    }
    return 1;
#else
    static char s_file[1u << 20u];
    char home[CLI_PATH_LONGEST];
    if (!query_environment_read("HOME", home, sizeof(home)))
    {
        cli_line_text(said, "HOME is not set");
        return 0;
    }
    char line[CLI_PATH_LONGEST * 2u];
    CliLine made = {line, sizeof(line), 0ull};
    cli_line_text(&made, "export PATH=\"$PATH:");
    for (const char *at = folder; at[0] != '\0'; at += 1)
    {
        cli_line_text(&made, (at[0] == '"') ? "\\\"" : "");
        if (at[0] != '"')
        {
            cli_line_character(&made, at[0]);
        }
    }
    cli_line_character(&made, '"');
    const unsigned long long line_length = cli_line_closed(&made);
    static const char *const s_names[4] = {"/.profile", "/.zprofile", "/.bash_profile", "/.bash_login"};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        char path[CLI_PATH_LONGEST];
        CliLine named = {path, sizeof(path), 0ull};
        cli_line_text(&named, home);
        cli_line_text(&named, s_names[at]);
        cli_line_closed(&named);
#if !defined(__APPLE__)
        if (at == 1u)
        {
            continue;
        }
#endif
        if ((at >= 2u) && !query_is_file(path))
        {
            continue;
        }
        unsigned long long size =
            qry_member_file_read(path, (unsigned char *)s_file, sizeof(s_file) - line_length - 4u);
        size = (size == QRY_MEMBER_UNREAD) ? 0ull : size;
        // a line already there, trimmed, is kept as it is
        int there = 0;
        const char *from = s_file;
        for (const char *place = s_file; !there && (place <= (s_file + size)); place += 1)
        {
            if ((place == (s_file + size)) || (place[0] == '\n'))
            {
                const char *start = from;
                const char *end = place;
                while ((start < end) && ((start[0] == ' ') || ((start[0] >= '\t') && (start[0] <= '\r'))))
                {
                    start += 1;
                }
                while ((end > start) && ((end[-1] == ' ') || ((end[-1] >= '\t') && (end[-1] <= '\r'))))
                {
                    end -= 1;
                }
                there = ((unsigned long long)(end - start) == line_length) &&
                        (memor.cmp(&(MemoriaCfg){.src = start, .other = line, .bytes = line_length}) == 0);
                from = place + 1;
            }
        }
        if (there)
        {
            continue;
        }
        CliLine text = {s_file, sizeof(s_file), size};
        if ((size != 0ull) && (s_file[size - 1u] != '\n'))
        {
            cli_line_character(&text, '\n');
        }
        cli_line_counted(&text, line, line_length);
        cli_line_character(&text, '\n');
        if (!cli_line_fits(&text) || !query_file_written(path, s_file, text.at))
        {
            cli_line_text(said, path);
            cli_line_text(said, ": it could not be written");
            return 0;
        }
    }
    return 1;
#endif
}

int cli_toolchains_add_to_path(const char *what, char *folder, CliLine *said)
{
    if (cellul.eq(&(CatenaFinitaCfg){.src = what, .other = "orior", .cap = CLI_CELLS_POOL}))
    {
        int on_path = 0;
        if (!cli_toolchains_own(folder, &on_path, said))
        {
            return 0;
        }
    }
    else
    {
        CliJson json;
        CliFound found;
        cli_lock_take(&g_cli_toolchains_find_lock);
        const unsigned int tool = cli_toolchains_tool(&json, cli_toolchains_manifest(&json, 0u, 0ull), what, said);
        if (tool == CLI_JSON_NONE)
        {
            cli_toolchains_find_released(&json);
            return 0;
        }
        const unsigned int count = cli_toolchains_find_path();
        cli_toolchains_find(&json, tool, (const char (*)[CLI_PATH_LONGEST])g_cli_toolchains_path, count, &found);
        const char *const name = cli_json_string(&json, cli_json_member(&json, tool, "name"), NULL);
        if (!found.has_program)
        {
            cli_line_text(said, (name != NULL) ? name : what);
            cli_line_text(said, " is not found: install it or give orior its folder first");
            cli_toolchains_find_released(&json);
            return 0;
        }
        memor.cpy(
            &(MemoriaCfg){.dst = folder,
                          .src = found.folder,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = found.folder, .cap = CLI_CELLS_POOL}) + 1u});
        cli_toolchains_find_released(&json);
    }
    return cli_add_folder(folder, said);
}
