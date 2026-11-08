// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// home.c: orior's own folder, for what it stores between runs and what a reader adds to it: the settings, the reader's
// plugins under `plugins/`, the stylesheet `user.css` the window lays over its own, and `menus.json`, the reader's
// menus and toolbar. It is `orior` in the system's folder for settings, %APPDATA% on Windows and $XDG_CONFIG_HOME or
// ~/.config elsewhere, or the folder ORIOR_HOME names
#include "../command_line/cli_internal.h"
#include "../text/text.h"

// what menus.json contains when orior_cli makes it: a note on what each part is for, and nothing laid out
#define CLI_MENUS                                                                                                      \
    "{\n  \"about\": \"Each item is a command's name, as orior_cli <menu> <command> names it, or { \\\"command\\\": "  \
    "name, "                                                                                                           \
    "\\\"label\\\": text, \\\"icon\\\": name }, or \\\"-\\\" for a line; { \\\"label\\\": text, \\\"items\\\": [...] " \
    "} "                                                                                                               \
    "contains more. A menu named as one of orior's stands in its place, the others stand before Help, and hide takes " \
    "orior's away by name.\",\n  \"menus\": [],\n  \"hide\": [],\n  \"toolbar\": []\n}\n"

// what user.css contains when orior_cli makes it: a note on what it is for
#define CLI_USER_CSS                                                                                                   \
    "/* orior_cli lays this stylesheet over its own, last, and reads it again each time the window\n"                  \
    "   comes back to the front. Every color and font the app uses is a variable, named as\n"                          \
    "   Preferences lists them. This rule sets two of them for both schemes:\n\n"                                      \
    "   :root[data-scheme] { --fuse-fire: #ff7a1a; --code: \"Fira Code\", monospace; }\n\n"                            \
    "   Any other rule of CSS works here as well. */\n"

int cli_home_folder(const CliFolderCtx *args)
{
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ORIOR_HOME",
                   .value_out = args->folder_out, .capacity = CLI_PATH_MAX_LEN))
    {
        return 1;
    }
    char base[CLI_PATH_MAX_LEN];
#if defined(_WIN32)
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "APPDATA", .value_out = base,
                    .capacity = sizeof(base)))
    {
        return 0;
    }
#else
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "XDG_CONFIG_HOME",
                    .value_out = base, .capacity = sizeof(base)))
    {
        char home[CLI_PATH_MAX_LEN];
        if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "HOME",
                        .value_out = home, .capacity = sizeof(home)))
        {
            return 0;
        }
        VerbaLinea line = {base, sizeof(base), 0ULL};
        mmgr_line_put(&line, home);
        mmgr_line_put(&line, "/.config");
        if (mmgr_line_finish(&line) == 0ULL)
        {
            return 0;
        }
    }
#endif
    VerbaLinea line = {args->folder_out, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, base);
    mmgr_line_put(&line, "/orior");
    return mmgr_line_finish(&line) != 0ULL;
}

typedef struct
{
    const char *name;
    char *path;
    const char *path_read;
    const char *text;
    VerbaLinea *err_msg;
} HomeCtx;
// `name` in orior's own folder into `path`: 1, or 0 where orior_cli has no folder of its own
EMBED_INLINE int cli_home_by_name(const HomeCtx *args)
{
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder))
    {
        return 0;
    }
    VerbaLinea line = {args->path, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->name);
    return mmgr_line_finish(&line) != 0ULL;
}

int cli_home_plugins(const CliFolderCtx *args)
{
    return EMBED_CALL(cli_home_by_name, HomeCtx, .name = "plugins", .path = args->folder_out);
}

int cli_home_user_css(const CliPathErrMsgTypeCtx *args)
{
    return EMBED_CALL(cli_home_by_name, HomeCtx, .name = "user.css", .path = args->path_out);
}

int cli_home_menus(const CliPathErrMsgTypeCtx *args)
{
    return EMBED_CALL(cli_home_by_name, HomeCtx, .name = "menus.json", .path = args->path_out);
}

// the folder of the reader's language files, `<tag>.json` each, which give the window's texts in the reader's language
// (the window's language.js)
int cli_home_languages(const CliFolderCtx *args)
{
    return EMBED_CALL(cli_home_by_name, HomeCtx, .name = "languages", .path = args->folder_out);
}

// the file at `path` made with `text` where it is not there yet, its folder with it
EMBED_INLINE int cli_home_ensure(const HomeCtx *args)
{
    const char *path = args->path_read;
    const char *text = args->text;
    if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path))
    {
        return 1;
    }
    char folder[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){
        .dst = folder, .src = path, .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES}) + 1u});
    char *const slash =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = folder,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_POOL_BYTES}), .byte = '/');
    if (slash != NULL)
    {
        *slash = '\0';
        if (!EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder))
        {
            mmgr_line_put(args->err_msg, folder);
            mmgr_line_put(args->err_msg, ": it could not be made");
            return 0;
        }
    }
    if (!EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = text,
                    .size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES})))
    {
        mmgr_line_put(args->err_msg, path);
        mmgr_line_put(args->err_msg, ": it could not be written");
        return 0;
    }
    return 1;
}

int cli_home_ensure_menus(const CliPathErrMsgTypeCtx *args)
{
    if (!cli_path_err_msg_type_internal.home_menus(args))
    {
        mmgr_line_put(args->err_msg, "orior_cli has no folder of its own to store menus in");
        return 0;
    }
    return EMBED_CALL(cli_home_ensure, HomeCtx, .path_read = args->path_out, .text = CLI_MENUS,
                      .err_msg = args->err_msg);
}

int cli_settings_ensure(const CliPathErrMsgTypeCtx *args)
{
    return EMBED_CALL(cli_home_ensure, HomeCtx, .path_read = args->path_out, .text = "{\n}\n",
                      .err_msg = args->err_msg);
}

int cli_home_ensure_user_css(const CliPathErrMsgTypeCtx *args)
{
    if (!cli_path_err_msg_type_internal.home_user_css(args))
    {
        mmgr_line_put(args->err_msg, "orior_cli has no folder of its own to store a stylesheet in");
        return 0;
    }
    return EMBED_CALL(cli_home_ensure, HomeCtx, .path_read = args->path_out, .text = CLI_USER_CSS,
                      .err_msg = args->err_msg);
}

int cli_home_reveal(const CliPathErrMsgNameCtx *args)
{
#if defined(_WIN32)
    const char *const words[3] = {"explorer", args->path, NULL};
#elif defined(__APPLE__)
    const char *const words[3] = {"open", args->path, NULL};
#else
    const char *const words[3] = {"xdg-open", args->path, NULL};
#endif
    // the file manager or the program it opens in is the reader's, and stays as orior_cli ends
    unsigned int id = 0u;
    if (!EMBED_CALL(cli_words_id_internal.started_outside, CliWordsIdCtx, .words = words, .id = &id))
    {
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, ": it could not be opened");
        return 0;
    }
    return 1;
}
