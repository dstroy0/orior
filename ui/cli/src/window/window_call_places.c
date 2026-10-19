// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_places.c: the window's calls on the reader's own places: orior's folder, the language files by tag, the
// reader's stylesheet and menus, the places opened as the system opens them, and the templates projects begin from
#include "../plugins/templates.h"
#include "window_internal.h"

#include "../text/text.h"

// the most language files listed
#define CLI_WINDOW_LANGUAGES 1024u

static CliLock s_lock = CLI_LOCK_INIT;
static char s_text[1u << 22u];

// the file at `path` written as a JSON string, the empty string where it cannot be read
static void places_file_quoted(const char *path, int has_path, CliLine *out)
{
    cli_lock_take(&s_lock);
    const unsigned long long size =
        has_path ? qry_member_file_read(path, (unsigned char *)s_text, sizeof(s_text)) : QRY_MEMBER_UNREAD;
    cli_window_counted(out, s_text, (size == QRY_MEMBER_UNREAD) ? 0ull : size);
    cli_lock_give(&s_lock);
}

// orior's own folder, where it keeps what it keeps and the reader's plugins are
static int places_home_folder(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    char folder[CLI_PATH_LONGEST];
    if (!cli_window_home(folder, said))
    {
        return 0;
    }
    cli_window_quoted(out, folder);
    return 1;
}

// the text of the reader's language file for the language `tag`, or nothing where there is none
static int places_language_read(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const tag = cli_serve_string(args, "tag", said);
    if (tag == NULL)
    {
        return 0;
    }
    int plain = (tag[0] != '\0');
    for (const char *at = tag; *at != '\0'; at += 1)
    {
        plain &= ((*at >= 'a') && (*at <= 'z')) || ((*at >= 'A') && (*at <= 'Z')) || ((*at >= '0') && (*at <= '9')) ||
                 (*at == '-');
    }
    char folder[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    CliLine line = {path, sizeof(path), 0ull};
    const int has_folder = plain && cli_home_languages(folder);
    if (has_folder)
    {
        cli_line_text(&line, folder);
        cli_line_character(&line, '/');
        cli_line_text(&line, tag);
        cli_line_text(&line, ".json");
    }
    places_file_quoted(path, has_folder && (cli_line_closed(&line) != 0ull), out);
    return 1;
}

// The language files found: their tags, in order
typedef struct
{
    char tag[CLI_WINDOW_LANGUAGES][CLI_NAME_LONGEST];
    unsigned int count;
} CliWindowTags;

// a file of the languages' folder taken, its tag kept in order where it is a .json
static void places_tag_each(void *context, const char *name, int is_folder)
{
    CliWindowTags *const tags = context;
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    if (is_folder || (length < 5u) || (length - 5u >= CLI_NAME_LONGEST) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = name + length - 5u, .other = ".json", .cap = CLI_CELLS_POOL})) ||
        (tags->count >= CLI_WINDOW_LANGUAGES))
    {
        return;
    }
    char tag[CLI_NAME_LONGEST];
    memor.cpy(&(MemoriaCfg){.dst = tag, .src = name, .bytes = length - 5u});
    tag[length - 5u] = '\0';
    unsigned int place = tags->count;
    while ((place > 0u) && (cli_text_order(tags->tag[place - 1u], tag) > 0))
    {
        memor.cpy(&(MemoriaCfg){.dst = tags->tag[place], .src = tags->tag[place - 1u], .bytes = CLI_NAME_LONGEST});
        place -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = tags->tag[place], .src = tag, .bytes = length - 4u});
    tags->count += 1u;
}

// the languages the reader has files for, by tag, in order
static int places_languages_list(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    static CliWindowTags s_tags;
    char folder[CLI_PATH_LONGEST];
    cli_lock_take(&s_lock);
    s_tags.count = 0u;
    if (cli_home_languages(folder))
    {
        (void)query_folder_each(folder, places_tag_each, &s_tags);
    }
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < s_tags.count; at += 1u)
    {
        cli_line_text(out, (at > 0u) ? "," : "");
        cli_window_quoted(out, s_tags.tag[at]);
    }
    cli_line_character(out, ']');
    cli_lock_give(&s_lock);
    return 1;
}

// the reader's stylesheet, or nothing where there is none
static int places_user_css_read(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    char path[CLI_PATH_LONGEST];
    places_file_quoted(path, cli_home_user_css(path), out);
    return 1;
}

// the reader's menus and toolbar, as menus.json holds them, or nothing where there is none
static int places_menus_read(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    char path[CLI_PATH_LONGEST];
    places_file_quoted(path, cli_home_menus(path), out);
    return 1;
}

// `folder` made where it is not there: 1, or 0 with why said
static int places_folder_made(const char *folder, CliLine *said)
{
    if (!query_folder_made(folder))
    {
        cli_line_text(said, folder);
        cli_line_text(said, ": it could not be made");
        return 0;
    }
    return 1;
}

// one of orior's own places opened as the system opens it: "user-css", "menus", "settings" and "plugins", each made
// first where it is not there, or the folder of one of the reader's plugins
static int places_home_reveal(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)out;
    const char *const what = cli_serve_string(args, "what", said);
    char plugins[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    if (what == NULL)
    {
        return 0;
    }
    if (!cli_home_plugins(plugins))
    {
        cli_line_text(said, "orior has no folder of its own");
        return 0;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = what, .other = "user-css", .cap = CLI_CELLS_POOL}))
    {
        if (!cli_home_ensure_user_css(path, said))
        {
            return 0;
        }
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = what, .other = "menus", .cap = CLI_CELLS_POOL}))
    {
        if (!cli_home_ensure_menus(path, said))
        {
            return 0;
        }
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = what, .other = "settings", .cap = CLI_CELLS_POOL}))
    {
        char home[CLI_PATH_LONGEST];
        if (!cli_window_home(home, said))
        {
            return 0;
        }
        cli_full(home, "settings.json", path);
        if (!query_is_file(path))
        {
            if (!places_folder_made(home, said))
            {
                return 0;
            }
            if (!query_file_written(path, "{}\n", 3ull))
            {
                cli_line_text(said, path);
                cli_line_text(said, ": it could not be written");
                return 0;
            }
        }
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = what, .other = "plugins", .cap = CLI_CELLS_POOL}))
    {
        if (!places_folder_made(plugins, said))
        {
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = path,
                                .src = plugins,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = plugins, .cap = CLI_CELLS_POOL}) + 1u});
    }
    else
    {
        // a folder of one of the reader's plugins, under their plugins folder
        const size_t length = cellul.len(&(CatenaFinitaCfg){.src = plugins, .cap = CLI_CELLS_POOL});
        if (!cli_text_same(what, plugins, length) || ((what[length] != '/') && (what[length] != '\\')) ||
            !query_is_folder(what))
        {
            cli_line_text(said, what);
            cli_line_text(said, " is not a plugin of the reader's");
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = path,
                                .src = what,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = what, .cap = CLI_CELLS_POOL}) + 1u});
    }
    return cli_home_reveal(path, said);
}

// the reader's templates, and the folder that holds them
static int places_templates_list(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    char folder[CLI_PATH_LONGEST];
    cli_line_character(out, '[');
    cli_templates_list(out);
    cli_line_character(out, ',');
    cli_window_quoted(out, cli_templates_folder(folder) ? folder : "");
    cli_line_character(out, ']');
    return 1;
}

// a project named `name` begun in `parent` from the template `template`: its folder
static int places_project_create(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const template_name = cli_serve_string(args, "template", said);
    const char *const parent = (template_name != NULL) ? cli_serve_string(args, "parent", said) : NULL;
    const char *const name = (parent != NULL) ? cli_serve_string(args, "name", said) : NULL;
    char made[CLI_PATH_LONGEST];
    if ((name == NULL) || !cli_templates_create(template_name, parent, name, made, said))
    {
        return 0;
    }
    cli_window_quoted(out, made);
    return 1;
}

// the folder that holds the reader's templates opened, made where it is not there yet
static int places_templates_reveal(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)out;
    char folder[CLI_PATH_LONGEST];
    if (!cli_templates_folder(folder))
    {
        cli_line_text(said, "orior has no folder of its own to keep templates in");
        return 0;
    }
    return places_folder_made(folder, said) && cli_home_reveal(folder, said);
}

const CliServeCall g_cli_window_places_calls[] = {
    {"home_folder", places_home_folder},           {"language_read", places_language_read},
    {"languages_list", places_languages_list},     {"user_css_read", places_user_css_read},
    {"reader_menus_read", places_menus_read},      {"home_reveal", places_home_reveal},
    {"templates_list", places_templates_list},     {"project_create", places_project_create},
    {"templates_reveal", places_templates_reveal},
};
const unsigned int g_cli_window_places_count = sizeof(g_cli_window_places_calls) / sizeof(g_cli_window_places_calls[0]);
