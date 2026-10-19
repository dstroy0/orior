// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// home.c: orior's own folder, for what it keeps between runs and what a reader adds to it: the settings, the reader's
// plugins under `plugins/`, the stylesheet `user.css` the window lays over its own, and `menus.json`, the reader's
// menus and toolbar. It is `orior` in the system's folder for settings, %APPDATA% on Windows and $XDG_CONFIG_HOME or
// ~/.config elsewhere, or the folder ORIOR_HOME names
#include "../command_line/cli_internal.h"
#include "../text/text.h"

// what menus.json holds when orior makes it: a note on what each part is for, and nothing laid out
#define CLI_MENUS                                                                                                      \
    "{\n  \"about\": \"Each item is a command's name, as orior <menu> <command> names it, or { \\\"command\\\": "      \
    "name, "                                                                                                           \
    "\\\"label\\\": text, \\\"icon\\\": name }, or \\\"-\\\" for a line; { \\\"label\\\": text, \\\"items\\\": [...] " \
    "} "                                                                                                               \
    "holds more. A menu named as one of orior's stands in its place, the others stand before Help, and hide takes "    \
    "orior's away by name.\",\n  \"menus\": [],\n  \"hide\": [],\n  \"toolbar\": []\n}\n"

// what user.css holds when orior makes it: a note on what it is for
#define CLI_USER_CSS                                                                                                   \
    "/* orior lays this stylesheet over its own, last, and reads it again each time the window\n"                      \
    "   comes back to the front. Every color and font the app uses is a variable, named as\n"                          \
    "   Preferences lists them. This rule sets two of them for both schemes:\n\n"                                      \
    "   :root[data-scheme] { --fuse-fire: #ff7a1a; --code: \"Fira Code\", monospace; }\n\n"                            \
    "   Any other rule of CSS works here as well. */\n"

int cli_home_folder(char *folder)
{
    if (query_environment_read("ORIOR_HOME", folder, CLI_PATH_LONGEST))
    {
        return 1;
    }
    char base[CLI_PATH_LONGEST];
#if defined(_WIN32)
    if (!query_environment_read("APPDATA", base, sizeof(base)))
    {
        return 0;
    }
#else
    if (!query_environment_read("XDG_CONFIG_HOME", base, sizeof(base)))
    {
        char home[CLI_PATH_LONGEST];
        if (!query_environment_read("HOME", home, sizeof(home)))
        {
            return 0;
        }
        CliLine line = {base, sizeof(base), 0ull};
        cli_line_text(&line, home);
        cli_line_text(&line, "/.config");
        if (cli_line_closed(&line) == 0ull)
        {
            return 0;
        }
    }
#endif
    CliLine line = {folder, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, base);
    cli_line_text(&line, "/orior");
    return cli_line_closed(&line) != 0ull;
}

// `name` in orior's own folder into `path`: 1, or 0 where orior has no folder of its own
static int cli_home_named(const char *name, char *path)
{
    char folder[CLI_PATH_LONGEST];
    if (!cli_home_folder(folder))
    {
        return 0;
    }
    CliLine line = {path, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, folder);
    cli_line_character(&line, '/');
    cli_line_text(&line, name);
    return cli_line_closed(&line) != 0ull;
}

int cli_home_plugins(char *folder)
{
    return cli_home_named("plugins", folder);
}

int cli_home_user_css(char *path)
{
    return cli_home_named("user.css", path);
}

int cli_home_menus(char *path)
{
    return cli_home_named("menus.json", path);
}

// the folder of the reader's language files, `<tag>.json` each, which give the window's texts in the reader's language
// (the window's language.js)
int cli_home_languages(char *folder)
{
    return cli_home_named("languages", folder);
}

// the file at `path` made with `text` where it is not there yet, its folder with it
static int cli_home_ensured(const char *path, const char *text, CliLine *said)
{
    if (query_is_file(path))
    {
        return 1;
    }
    char folder[CLI_PATH_LONGEST];
    memor.cpy(&(MemoriaCfg){
        .dst = folder, .src = path, .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    char *const slash =
        cli_text_last(folder, cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}), '/');
    if (slash != NULL)
    {
        *slash = '\0';
        if (!query_folder_made(folder))
        {
            cli_line_text(said, folder);
            cli_line_text(said, ": it could not be made");
            return 0;
        }
    }
    if (!query_file_written(path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL})))
    {
        cli_line_text(said, path);
        cli_line_text(said, ": it could not be written");
        return 0;
    }
    return 1;
}

int cli_home_ensure_menus(char *path, CliLine *said)
{
    if (!cli_home_menus(path))
    {
        cli_line_text(said, "orior has no folder of its own to keep menus in");
        return 0;
    }
    return cli_home_ensured(path, CLI_MENUS, said);
}

int cli_home_ensure_user_css(char *path, CliLine *said)
{
    if (!cli_home_user_css(path))
    {
        cli_line_text(said, "orior has no folder of its own to keep a stylesheet in");
        return 0;
    }
    return cli_home_ensured(path, CLI_USER_CSS, said);
}

int cli_home_reveal(const char *path, CliLine *said)
{
#if defined(_WIN32)
    const char *const words[3] = {"explorer", path, NULL};
#elif defined(__APPLE__)
    const char *const words[3] = {"open", path, NULL};
#else
    const char *const words[3] = {"xdg-open", path, NULL};
#endif
    // the file manager or the program it opens in is the reader's, and stays as orior ends
    unsigned int id = 0u;
    if (!cli_started_outside(words, &id))
    {
        cli_line_text(said, path);
        cli_line_text(said, ": it could not be opened");
        return 0;
    }
    return 1;
}
