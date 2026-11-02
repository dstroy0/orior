// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_places.c: the window's calls on the reader's own places: orior's folder, the language files by tag, the
// reader's stylesheet and menus, the places opened as the system opens them, and the templates projects begin from
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../plugins/templates.h"
#include "window_internal.h"

#include "../text/text.h"

// the most language files listed, the tags their list is taken with, and the most bytes of a file of the reader's
// places read
#define CLI_WINDOW_LANGUAGES 1024u
#define CLI_WINDOW_PLACE_FILE (1u << 22u)

typedef struct
{
    const char *path;
    int has_path;
    VerbaLinea *out;
    const char *folder;
    VerbaLinea *err_msg;
} WindowCallPlacesCtx;
// the file at `path` written as a JSON string, the empty string where it cannot be read or is larger than
// CLI_WINDOW_PLACE_FILE
EMBED_INLINE void places_file_quote(const WindowCallPlacesCtx *args)
{
    const char *path = args->path;
    VerbaLinea *out = args->out;
    unsigned long long size = 0ull;
    const unsigned char *const bytes = args->has_path
                                           ? EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = path,
                                                        .size_pointer = &size, .owner = &g_cli_window_places_cells)
                                           : NULL;
    const int read = (bytes != NULL) && (size <= CLI_WINDOW_PLACE_FILE);
    // the file's bytes read as the text they are
    EMBED_CALL(cli_window_out_text_internal.count, CliWindowOutTextCtx, .out = out,
               .text = read ? (const char *)bytes : "", .size = read ? size : 0ull);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes,
               .owner = &g_cli_window_places_cells);
}

// orior's own folder, where it stores what it stores and the reader's plugins are
static int places_home_folder(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_window_internal.home, CliWindowCtx, .folder = folder, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out, .text = folder);
    return 1;
}

// the text of the reader's language file for the language `tag`, or nothing where there is none
static int places_language_read(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const tag = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "tag", .err_msg = err_msg);
    if (tag == NULL)
    {
        return 0;
    }
    int plain = (tag[0] != '\0');
    plain = plain && (EMBED_CALL(cli_text_at_size_classes.past_class, CliTextAtSizeClassesCfg, .text = tag, .at = 0ull,
                                 .size = cellul.len(&(CatenaFinitaCfg){.src = tag, .cap = CLI_CELLS_POOL}),
                                 .classes = CLI_TEXT_LETTERS | CLI_TEXT_DIGITS,
                                 .set = "-") == cellul.len(&(CatenaFinitaCfg){.src = tag, .cap = CLI_CELLS_POOL}));
    char folder[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ull};
    const int has_folder = plain && EMBED_CALL(cli_folder_internal.home_languages, CliFolderCtx, .folder_out = folder);
    if (has_folder)
    {
        mmgr_line_put(&line, folder);
        mmgr_line_ch(&line, '/');
        mmgr_line_put(&line, tag);
        mmgr_line_put(&line, ".json");
    }
    EMBED_CALL(places_file_quote, WindowCallPlacesCtx, .path = path,
               .has_path = has_folder && (mmgr_line_finish(&line) != 0ull), .out = out);
    return 1;
}

// The language files found: their tags, in order, in a cell of `room`
typedef struct
{
    char (*tag)[CLI_NAME_MAX_LEN];
    unsigned int count;
    unsigned int room;
} CliWindowTags;

// a file of the languages' folder taken, its tag stored in order where it is a .json
static void places_tag_each(const CliEachNameIsFolderCfg *args)
{
    CliWindowTags *const tags = args->context;
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL});
    if (args->is_folder || (length < 5u) || (length - 5u >= CLI_NAME_MAX_LEN) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = args->name + length - 5u, .other = ".json", .cap = CLI_CELLS_POOL})) ||
        (tags->count >= tags->room))
    {
        return;
    }
    char tag[CLI_NAME_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = tag, .src = args->name, .bytes = length - 5u});
    tag[length - 5u] = '\0';
    unsigned int place = tags->count;
    while ((place > 0u) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = tags->tag[place - 1u],
                                       .other_pointer = tag) > 0))
    {
        memor.cpy(&(MemoriaCfg){.dst = tags->tag[place], .src = tags->tag[place - 1u], .bytes = CLI_NAME_MAX_LEN});
        place -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = tags->tag[place], .src = tag, .bytes = length - 4u});
    tags->count += 1u;
}

// the languages the reader has files for, by tag, in order
static int places_languages_list(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    char (*const taken)[CLI_NAME_MAX_LEN] = g_cli_cells->temporary_buf_alloc(
        (unsigned long long)CLI_WINDOW_LANGUAGES * CLI_NAME_MAX_LEN, &g_cli_window_places_cells);
    CliWindowTags tags = {taken, 0u, (taken != NULL) ? CLI_WINDOW_LANGUAGES : 0u};
    char folder[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_folder_internal.home_languages, CliFolderCtx, .folder_out = folder))
    {
        (void)EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = folder,
                         .each = places_tag_each, .context_bytess = &tags);
    }
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < tags.count; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? "," : "");
        EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out, .text = tags.tag[at]);
    }
    mmgr_line_ch(out, ']');
    g_cli_cells->persistent_buf_release(tags.tag, &g_cli_window_places_cells);
    return 1;
}

// the reader's stylesheet, or nothing where there is none
static int places_user_css_read(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(places_file_quote, WindowCallPlacesCtx, .path = path,
               .has_path =
                   EMBED_CALL(cli_path_err_msg_kind_internal.home_user_css, CliPathErrMsgKindCtx, .path_out = path),
               .out = out);
    return 1;
}

// the reader's menus and toolbar, as menus.json stores them, or nothing where there is none
static int places_menus_read(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(places_file_quote, WindowCallPlacesCtx, .path = path,
               .has_path =
                   EMBED_CALL(cli_path_err_msg_kind_internal.home_menus, CliPathErrMsgKindCtx, .path_out = path),
               .out = out);
    return 1;
}

// `folder` made where it is not there: 1, or 0 with why given
EMBED_INLINE int places_folder_init(const WindowCallPlacesCtx *args)
{
    const char *folder = args->folder;
    if (!EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder))
    {
        mmgr_line_put(args->err_msg, folder);
        mmgr_line_put(args->err_msg, ": it could not be made");
        return 0;
    }
    return 1;
}

// one of orior's own places opened as the system opens it: "user-css", "menus", "settings" and "plugins", each made
// first where it is not there, or the folder of one of the reader's plugins
static int places_home_reveal(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    const char *const what = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "what", .err_msg = err_msg);
    char plugins[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    if (what == NULL)
    {
        return 0;
    }
    if (!EMBED_CALL(cli_folder_internal.home_plugins, CliFolderCtx, .folder_out = plugins))
    {
        mmgr_line_put(err_msg, "orior_cli has no folder of its own");
        return 0;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = what, .other = "user-css", .cap = CLI_CELLS_POOL}))
    {
        if (!EMBED_CALL(cli_path_err_msg_kind_internal.home_ensure_user_css, CliPathErrMsgKindCtx, .path_out = path,
                        .err_msg = err_msg))
        {
            return 0;
        }
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = what, .other = "menus", .cap = CLI_CELLS_POOL}))
    {
        if (!EMBED_CALL(cli_path_err_msg_kind_internal.home_ensure_menus, CliPathErrMsgKindCtx, .path_out = path,
                        .err_msg = err_msg))
        {
            return 0;
        }
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = what, .other = "settings", .cap = CLI_CELLS_POOL}))
    {
        char home[CLI_PATH_MAX_LEN];
        if (!EMBED_CALL(cli_window_internal.home, CliWindowCtx, .folder = home, .err_msg = err_msg))
        {
            return 0;
        }
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = home, .relative = "settings.json", .joined = path);
        if (!EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path))
        {
            if (!EMBED_CALL(places_folder_init, WindowCallPlacesCtx, .folder = home, .err_msg = err_msg))
            {
                return 0;
            }
            if (!EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path,
                            .bytes = "{}\n", .size = 3ull))
            {
                mmgr_line_put(err_msg, path);
                mmgr_line_put(err_msg, ": it could not be written");
                return 0;
            }
        }
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = what, .other = "plugins", .cap = CLI_CELLS_POOL}))
    {
        if (!EMBED_CALL(places_folder_init, WindowCallPlacesCtx, .folder = plugins, .err_msg = err_msg))
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
        if (!EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = what, .other_pointer = plugins,
                        .size = length) ||
            ((what[length] != '/') && (what[length] != '\\')) ||
            !EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = what))
        {
            mmgr_line_put(err_msg, what);
            mmgr_line_put(err_msg, " is not a plugin of the reader's");
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = path,
                                .src = what,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = what, .cap = CLI_CELLS_POOL}) + 1u});
    }
    return EMBED_CALL(cli_path_err_msg_name_internal.home_reveal, CliPathErrMsgNameCtx, .path = path,
                      .err_msg = err_msg);
}

// the reader's templates, and the folder that contains them
static int places_templates_list(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    char folder[CLI_PATH_MAX_LEN];
    mmgr_line_ch(out, '[');
    EMBED_CALL(cli_templates_out.list, CliTemplatesOutCfg, .out = out);
    mmgr_line_ch(out, ',');
    EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out,
               .text = EMBED_CALL(cli_templates_table.folder, CliTemplatesCfg, .folder = folder) ? folder : "");
    mmgr_line_ch(out, ']');
    return 1;
}

// a project named `name` written in `parent` from the template `template`: its folder
static int places_project_create(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const template_name = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                 .request = arguments, .key = "template", .err_msg = err_msg);
    const char *const parent = (template_name != NULL)
                                   ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                .request = arguments, .key = "parent", .err_msg = err_msg)
                                   : NULL;
    const char *const name = (parent != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                           .request = arguments, .key = "name", .err_msg = err_msg)
                                              : NULL;
    char made[CLI_PATH_MAX_LEN];
    if ((name == NULL) ||
        !EMBED_CALL(cli_templates_template_name.create, CliTemplatesTemplateNameCfg, .template_name = template_name,
                    .parent = parent, .name = name, .made = made, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out, .text = made);
    return 1;
}

// the folder that contains the reader's templates opened, made where it is not there yet
static int places_templates_reveal(const CliServeAnswerCfg *args)
{
    VerbaLinea *err_msg = args->err_msg;
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_templates_table.folder, CliTemplatesCfg, .folder = folder))
    {
        mmgr_line_put(err_msg, "orior_cli has no folder of its own to store templates in");
        return 0;
    }
    return EMBED_CALL(places_folder_init, WindowCallPlacesCtx, .folder = folder, .err_msg = err_msg) &&
           EMBED_CALL(cli_path_err_msg_name_internal.home_reveal, CliPathErrMsgNameCtx, .path = folder,
                      .err_msg = err_msg);
}

const CliServeCall g_cli_window_places_calls[] = {
    {"home_folder", places_home_folder},           {"language_read", places_language_read},
    {"languages_list", places_languages_list},     {"user_css_read", places_user_css_read},
    {"reader_menus_read", places_menus_read},      {"home_reveal", places_home_reveal},
    {"templates_list", places_templates_list},     {"project_create", places_project_create},
    {"templates_reveal", places_templates_reveal},
};
const unsigned int g_cli_window_places_count = sizeof(g_cli_window_places_calls) / sizeof(g_cli_window_places_calls[0]);
