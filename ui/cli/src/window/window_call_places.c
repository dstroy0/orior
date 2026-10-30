// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_places.c: the window's calls on the reader's own places: orior's folder, the language files by tag, the
// reader's stylesheet and menus, the places opened as the system opens them, and the templates projects begin from
#include "../plugins/templates.h"
#include "window_internal.h"

#include "../text/text.h"

// the most language files listed, the tags their list is begun with, doubling as they are found, and the most bytes of
// a file of the reader's places read
#define CLI_WINDOW_LANGUAGES 1024u
#define CLI_WINDOW_LANGUAGES_FIRST 16u
#define CLI_WINDOW_PLACE_FILE (1u << 22u)

CellOwner g_cli_window_places_cells = MMGR_CELL_OWNER("cli_window_places");

// the file at `path` written as a JSON string, the empty string where it cannot be read or is larger than
// CLI_WINDOW_PLACE_FILE
static void places_file_quoted(const char *path, int has_path, VerbaLinea *out)
{
    unsigned long long size = 0ull;
    const unsigned char *const bytes = has_path ? cli_file_read(path, &size) : NULL;
    const int read = (bytes != NULL) && (size <= CLI_WINDOW_PLACE_FILE);
    // the file's bytes read as the text they are
    cli_window_counted(out, read ? (const char *)bytes : "", read ? size : 0ull);
    cli_file_let_go(bytes, size);
}

// orior's own folder, where it stores what it stores and the reader's plugins are
static int places_home_folder(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    char folder[CLI_PATH_MAX_LEN];
    if (!cli_window_home(folder, err_msg))
    {
        return 0;
    }
    cli_window_quoted(out, folder);
    return 1;
}

// the text of the reader's language file for the language `tag`, or nothing where there is none
static int places_language_read(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const tag = cli_serve_string(args, "tag", err_msg);
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
    char folder[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ull};
    const int has_folder = plain && cli_home_languages(folder);
    if (has_folder)
    {
        mmgr_line_put(&line, folder);
        mmgr_line_ch(&line, '/');
        mmgr_line_put(&line, tag);
        mmgr_line_put(&line, ".json");
    }
    places_file_quoted(path, has_folder && (mmgr_line_finish(&line) != 0ull), out);
    return 1;
}

// The language files found: their tags, in order, in a cell of `room`
typedef struct
{
    char (*tag)[CLI_NAME_MAX_LEN];
    unsigned int count;
    unsigned int room;
} CliWindowTags;

// room made in the tags for one more, below CLI_WINDOW_LANGUAGES: the tags moved into a cell of twice their room,
// CLI_WINDOW_LANGUAGES_FIRST where they have none: 1, or 0 where there is none
static int places_tags_room(CliWindowTags *tags)
{
    if (tags->count < tags->room)
    {
        return 1;
    }
    const unsigned int doubled = (tags->room == 0u) ? CLI_WINDOW_LANGUAGES_FIRST : (tags->room * 2u);
    const unsigned int room = (doubled < CLI_WINDOW_LANGUAGES) ? doubled : CLI_WINDOW_LANGUAGES;
    char (*const grown)[CLI_NAME_MAX_LEN] =
        (tags->room < CLI_WINDOW_LANGUAGES)
            ? g_cli_cells->persistent_buf_alloc(room * sizeof(*grown), &g_cli_window_places_cells)
            : NULL;
    if (grown == NULL)
    {
        return 0;
    }
    if (tags->count != 0u)
    {
        memor.cpy(&(MemoriaCfg){.dst = grown, .src = tags->tag, .bytes = tags->count * sizeof(*grown)});
    }
    cli_cell_release(tags->tag, &g_cli_window_places_cells);
    tags->tag = grown;
    tags->room = room;
    return 1;
}

// a file of the languages' folder taken, its tag stored in order where it is a .json
static void places_tag_each(void *context, const char *name, int is_folder)
{
    CliWindowTags *const tags = context;
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    if (is_folder || (length < 5u) || (length - 5u >= CLI_NAME_MAX_LEN) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = name + length - 5u, .other = ".json", .cap = CLI_CELLS_POOL})) ||
        !places_tags_room(tags))
    {
        return;
    }
    char tag[CLI_NAME_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = tag, .src = name, .bytes = length - 5u});
    tag[length - 5u] = '\0';
    unsigned int place = tags->count;
    while ((place > 0u) && (cli_text_order(tags->tag[place - 1u], tag) > 0))
    {
        memor.cpy(&(MemoriaCfg){.dst = tags->tag[place], .src = tags->tag[place - 1u], .bytes = CLI_NAME_MAX_LEN});
        place -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = tags->tag[place], .src = tag, .bytes = length - 4u});
    tags->count += 1u;
}

// the languages the reader has files for, by tag, in order
static int places_languages_list(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    CliWindowTags tags = {NULL, 0u, 0u};
    char folder[CLI_PATH_MAX_LEN];
    if (cli_home_languages(folder))
    {
        (void)query_folder_each(folder, places_tag_each, &tags);
    }
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < tags.count; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? "," : "");
        cli_window_quoted(out, tags.tag[at]);
    }
    mmgr_line_ch(out, ']');
    cli_cell_release(tags.tag, &g_cli_window_places_cells);
    return 1;
}

// the reader's stylesheet, or nothing where there is none
static int places_user_css_read(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    char path[CLI_PATH_MAX_LEN];
    places_file_quoted(path, cli_home_user_css(path), out);
    return 1;
}

// the reader's menus and toolbar, as menus.json stores them, or nothing where there is none
static int places_menus_read(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    char path[CLI_PATH_MAX_LEN];
    places_file_quoted(path, cli_home_menus(path), out);
    return 1;
}

// `folder` made where it is not there: 1, or 0 with why given
static int places_folder_made(const char *folder, VerbaLinea *err_msg)
{
    if (!query_folder_made(folder))
    {
        mmgr_line_put(err_msg, folder);
        mmgr_line_put(err_msg, ": it could not be made");
        return 0;
    }
    return 1;
}

// one of orior's own places opened as the system opens it: "user-css", "menus", "settings" and "plugins", each made
// first where it is not there, or the folder of one of the reader's plugins
static int places_home_reveal(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)out;
    const char *const what = cli_serve_string(args, "what", err_msg);
    char plugins[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    if (what == NULL)
    {
        return 0;
    }
    if (!cli_home_plugins(plugins))
    {
        mmgr_line_put(err_msg, "orior_cli has no folder of its own");
        return 0;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = what, .other = "user-css", .cap = CLI_CELLS_POOL}))
    {
        if (!cli_home_ensure_user_css(path, err_msg))
        {
            return 0;
        }
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = what, .other = "menus", .cap = CLI_CELLS_POOL}))
    {
        if (!cli_home_ensure_menus(path, err_msg))
        {
            return 0;
        }
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = what, .other = "settings", .cap = CLI_CELLS_POOL}))
    {
        char home[CLI_PATH_MAX_LEN];
        if (!cli_window_home(home, err_msg))
        {
            return 0;
        }
        cli_full(home, "settings.json", path);
        if (!query_is_file(path))
        {
            if (!places_folder_made(home, err_msg))
            {
                return 0;
            }
            if (!query_file_written(path, "{}\n", 3ull))
            {
                mmgr_line_put(err_msg, path);
                mmgr_line_put(err_msg, ": it could not be written");
                return 0;
            }
        }
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = what, .other = "plugins", .cap = CLI_CELLS_POOL}))
    {
        if (!places_folder_made(plugins, err_msg))
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
            mmgr_line_put(err_msg, what);
            mmgr_line_put(err_msg, " is not a plugin of the reader's");
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = path,
                                .src = what,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = what, .cap = CLI_CELLS_POOL}) + 1u});
    }
    return cli_home_reveal(path, err_msg);
}

// the reader's templates, and the folder that contains them
static int places_templates_list(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    char folder[CLI_PATH_MAX_LEN];
    mmgr_line_ch(out, '[');
    cli_templates_list(out);
    mmgr_line_ch(out, ',');
    cli_window_quoted(out, cli_templates_folder(folder) ? folder : "");
    mmgr_line_ch(out, ']');
    return 1;
}

// a project named `name` begun in `parent` from the template `template`: its folder
static int places_project_create(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const template_name = cli_serve_string(args, "template", err_msg);
    const char *const parent = (template_name != NULL) ? cli_serve_string(args, "parent", err_msg) : NULL;
    const char *const name = (parent != NULL) ? cli_serve_string(args, "name", err_msg) : NULL;
    char made[CLI_PATH_MAX_LEN];
    if ((name == NULL) || !cli_templates_create(template_name, parent, name, made, err_msg))
    {
        return 0;
    }
    cli_window_quoted(out, made);
    return 1;
}

// the folder that contains the reader's templates opened, made where it is not there yet
static int places_templates_reveal(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)out;
    char folder[CLI_PATH_MAX_LEN];
    if (!cli_templates_folder(folder))
    {
        mmgr_line_put(err_msg, "orior_cli has no folder of its own to store templates in");
        return 0;
    }
    return places_folder_made(folder, err_msg) && cli_home_reveal(folder, err_msg);
}

const CliServeCall g_cli_window_places_calls[] = {
    {"home_folder", places_home_folder},           {"language_read", places_language_read},
    {"languages_list", places_languages_list},     {"user_css_read", places_user_css_read},
    {"reader_menus_read", places_menus_read},      {"home_reveal", places_home_reveal},
    {"templates_list", places_templates_list},     {"project_create", places_project_create},
    {"templates_reveal", places_templates_reveal},
};
const unsigned int g_cli_window_places_count = sizeof(g_cli_window_places_calls) / sizeof(g_cli_window_places_calls[0]);
