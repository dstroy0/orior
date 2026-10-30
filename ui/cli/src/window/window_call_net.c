// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_net.c: the window's calls that reach past this machine or into the reader's files anywhere: orior_cli
// blocked from the network or let on and the addresses it reached, the plugin catalog read and installed from, a
// plugin's request sent, a picture read as a data address, a page opened in the browser, a toolchain's install page,
// and a repository cloned, its progress reported as it comes
#include "../code/git.h"
#include "../machine/net.h"
#include "../machine/report.h"
#include "../plugins/plugin_catalog.h"
#include "../requests/requests_internal.h"
#include "../text/base64.h"
#include "../toolchains/toolchains.h"
#include "window_internal.h"

#include "../text/text.h"

// the most bytes of a picture behind the editor
#define CLI_WINDOW_IMAGE_MOST (8ull << 20u)

// the bytes a catalog entry's line is begun with before it grows
#define CLI_WINDOW_ENTRY_FIRST 4096ull

static CliLock s_lock = CLI_LOCK_INIT;

// orior_cli blocked from the network, or let on, as the reader's setting `network` specifies
static int net_disable(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)out;
    int off = 0;
    if (!cli_serve_bool(args, "off", &off, err_msg))
    {
        return 0;
    }
    cli_net_disable(off);
    return 1;
}

// the addresses orior_cli has reached, the last first
static int net_reached(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    cli_net_reached(1000u, out);
    return 1;
}

// the plugins the catalog at `address` lists
static int net_catalog_read(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const address = cli_serve_string(args, "address", err_msg);
    return (address != NULL) && cli_plugin_catalog_read(address, out, err_msg);
}

// the plugin `entry` of the catalog at `address` installed: its folder
static int net_catalog_install(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const address = cli_serve_string(args, "address", err_msg);
    if (address == NULL)
    {
        return 0;
    }
    VerbaLinea entry;
    if (!cli_growing_line_temporary(&entry, CLI_WINDOW_ENTRY_FIRST, &g_cli_window_cells))
    {
        mmgr_line_put(err_msg, "the catalog's entry could not be stored");
        return 0;
    }
    cli_window_arg_written(args, "entry", &entry);
    char folder_text[CLI_PATH_MAX_LEN];
    VerbaLinea folder = {folder_text, sizeof(folder_text), 0ull};
    const int installed =
        mmgr_line_ok(&entry) && cli_plugin_catalog_install(address, entry.out, entry.at, &folder, err_msg);
    if (installed)
    {
        cli_window_counted(out, folder_text, mmgr_line_ok(&folder) ? folder.at : 0ull);
    }
    g_cli_cells->persistent_buf_release(entry.out, &g_cli_window_cells);
    return installed;
}

// a request the plugin `id` sends to a host it requested to reach, as the window lets it: its answer's status, headers
// and body, the body in base64 where it is not text
static int net_plugin_request(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    static CliRequestsSent s_sent;
    const char *const id = cli_serve_string(args, "id", err_msg);
    const char *const method = (id != NULL) ? cli_serve_string(args, "method", err_msg) : NULL;
    const char *const url = (method != NULL) ? cli_serve_string(args, "url", err_msg) : NULL;
    const char *const body = (url != NULL) ? cli_serve_string(args, "body", err_msg) : NULL;
    if (body == NULL)
    {
        return 0;
    }
    char purpose[CLI_NAME_MAX_LEN + 16u];
    VerbaLinea named = {purpose, sizeof(purpose), 0ull};
    mmgr_line_put(&named, "the plugin ");
    mmgr_line_put(&named, id);
    (void)mmgr_line_finish(&named);
    if (!cli_net_reach(url, purpose, err_msg))
    {
        return 0;
    }
    cli_lock_take(&s_lock);
    memor.set(&(MemoriaCfg){.dst = &s_sent, .val = 0, .bytes = sizeof(s_sent)});
    memor.cpy(&(MemoriaCfg){.dst = s_sent.method,
                            .src = method,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = method, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = s_sent.url,
                            .src = url,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = url, .cap = CLI_CELLS_POOL}) + 1u});
    const unsigned int headers = cli_serve_arg(args, "headers");
    const unsigned int first =
        cli_json_is(args->json, headers, CFG_JSON_ARRAY) ? args->json->node[headers].first : CLI_JSON_NONE;
    for (unsigned int pair = first; (pair != CLI_JSON_NONE) && (s_sent.headers < CLI_REQUESTS_HEADERS);
         pair = args->json->node[pair].next)
    {
        const char *const name = cli_json_string(args->json, cli_json_item(args->json, pair, 0u), NULL);
        const char *const value = cli_json_string(args->json, cli_json_item(args->json, pair, 1u), NULL);
        if ((name != NULL) && (value != NULL))
        {
            memor.cpy(
                &(MemoriaCfg){.dst = s_sent.header_name[s_sent.headers], .bytes = CLI_REQUESTS_NAME, .src = name});
            memor.cpy(
                &(MemoriaCfg){.dst = s_sent.header_value[s_sent.headers], .bytes = CLI_REQUESTS_VALUE, .src = value});
            s_sent.headers += 1u;
        }
    }
    s_sent.body = body;
    s_sent.body_size = cellul.len(&(CatenaFinitaCfg){.src = body, .cap = CLI_CELLS_POOL});
    s_sent.redirect = 0;
    CliRequestsRoom *const room = cli_requests_room_taken();
    if (room == NULL)
    {
        mmgr_line_put(err_msg, "the request's room could not be taken");
        cli_lock_give(&s_lock);
        return 0;
    }
    cli_requests_send(&s_sent, 1, room);
    const CliRequestsAnswer *const answer = &room->answer;
    const int answered = !answer->has_failed;
    if (!answered)
    {
        mmgr_line_put(err_msg, answer->failed);
    }
    else
    {
        mmgr_line_put(out, "{\"status\":");
        mmgr_line_decimal(out, answer->status, 1u);
        mmgr_line_put(out, ",\"headers\":[");
        for (unsigned int at = 0u; at < answer->headers; at += 1u)
        {
            mmgr_line_put(out, (at > 0u) ? ",[" : "[");
            cli_window_quoted(out, cli_requests_text(answer->head, answer->header_name[at]));
            mmgr_line_ch(out, ',');
            cli_window_quoted(out, cli_requests_text(answer->head, answer->header_value[at]));
            mmgr_line_ch(out, ']');
        }
        mmgr_line_put(out, "],\"body\":");
        if (answer->binary)
        {
            mmgr_line_ch(out, '"');
            cli_base64(out, answer->body, answer->size);
            mmgr_line_ch(out, '"');
        }
        else
        {
            cli_window_counted(out, (const char *)answer->body, answer->size);
        }
        mmgr_line_put(out, answer->binary ? ",\"binary\":true}" : ",\"binary\":false}");
    }
    cli_requests_room_given(room);
    cli_lock_give(&s_lock);
    return answered;
}

// the picture at `path` as the data address a theme stores for the image behind the editor: PNG, JPEG, GIF, WebP, SVG
// or BMP, of 8 MB at most
static int net_image_data(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    static const char *const s_kinds[7][2] = {{"png", "image/png"}, {"jpg", "image/jpeg"},  {"jpeg", "image/jpeg"},
                                              {"gif", "image/gif"}, {"webp", "image/webp"}, {"svg", "image/svg+xml"},
                                              {"bmp", "image/bmp"}};
    const char *const path = cli_serve_string(args, "path", err_msg);
    if (path == NULL)
    {
        return 0;
    }
    const char *const dot =
        cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '.');
    const char *const slash =
        cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '/');
    const char *const back =
        cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '\\');
    const char *kind = NULL;
    if ((dot != NULL) && (dot > slash) && (dot > back))
    {
        char end[8] = "";
        const size_t length = cellul.len(&(CatenaFinitaCfg){.src = dot + 1, .cap = CLI_CELLS_POOL});
        for (size_t at = 0u; (at < length) && (at + 1u < sizeof(end)); at += 1u)
        {
            const char letter = dot[1u + at];
            end[at] = ((letter >= 'A') && (letter <= 'Z')) ? (char)(letter - 'A' + 'a') : letter;
            end[at + 1u] = '\0';
        }
        for (unsigned int at = 0u; (at < 7u) && (length < sizeof(end)); at += 1u)
        {
            kind = (cellul.eq(&(CatenaFinitaCfg){.src = end, .other = s_kinds[at][0], .cap = CLI_CELLS_POOL}))
                       ? s_kinds[at][1]
                       : kind;
        }
    }
    if (kind == NULL)
    {
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, " is not a picture of a kind a theme takes: PNG, JPEG, GIF, WebP, SVG or BMP");
        return 0;
    }
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_read(path, &size);
    if (bytes == NULL)
    {
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, ": it could not be read");
        return 0;
    }
    if (size > CLI_WINDOW_IMAGE_MOST)
    {
        cli_file_let_go(bytes, size);
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, " is over 8 MB, the most an image behind the editor is");
        return 0;
    }
    mmgr_line_put(out, "\"data:");
    mmgr_line_put(out, kind);
    mmgr_line_put(out, ";base64,");
    cli_base64(out, bytes, size);
    mmgr_line_ch(out, '"');
    cli_file_let_go(bytes, size);
    return 1;
}

// an https page a hover or a diagnostic links to opened in the browser
static int net_link_open(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)out;
    const char *const url = cli_serve_string(args, "url", err_msg);
    if (url == NULL)
    {
        return 0;
    }
    if (!cellul.starts(&(CatenaFinitaCfg){.src = url, .other = "https://", .cap = CLI_CELLS_POOL}))
    {
        mmgr_line_put(err_msg, url);
        mmgr_line_put(err_msg, " is not an https page");
        return 0;
    }
    return cli_report_open_url(url, err_msg);
}

// a toolchain's install page opened in the browser, and named
static int net_toolchain_install(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const id = cli_serve_string(args, "id", err_msg);
    static char s_url[CLI_PATH_MAX_LEN * 4u];
    VerbaLinea url = {s_url, sizeof(s_url), 0ull};
    if ((id == NULL) || !cli_toolchains_open_install(id, &url, err_msg))
    {
        return 0;
    }
    cli_window_counted(out, s_url, mmgr_line_ok(&url) ? url.at : 0ull);
    return 1;
}

// what File, Clone Repository starts from: orior's own repository and the reader's home
static int net_clone_start(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    char parent[CLI_PATH_MAX_LEN] = "";
    (void)cli_git_clone_parent(parent);
    mmgr_line_put(out, "{\"url\":");
    cli_window_quoted(out, CLI_GIT_ORIOR);
    mmgr_line_put(out, ",\"parent\":");
    cli_window_quoted(out, parent);
    mmgr_line_ch(out, '}');
    return 1;
}

// a line of git's progress sent to the page as clone-progress
static void net_clone_line(void *context, const char *line, int ended)
{
    (void)context;
    (void)ended;
    char msg_buf[4096];
    VerbaLinea body = {msg_buf, sizeof(msg_buf), 0ull};
    cli_window_quoted(&body, line);
    cli_serve_emitted("clone-progress", msg_buf, mmgr_line_ok(&body) ? body.at : 0ull);
}

// `url` cloned into a new folder under `parent`, a full path, each line of git's progress reported as it comes: the
// folder
static int net_repo_clone(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const url = cli_serve_string(args, "url", err_msg);
    const char *const parent = (url != NULL) ? cli_serve_string(args, "parent", err_msg) : NULL;
    if (parent == NULL)
    {
        return 0;
    }
    const int full = (parent[0] == '/') || (parent[0] == '\\') ||
                     ((parent[0] != '\0') && (parent[1] == ':') && ((parent[2] == '/') || (parent[2] == '\\')));
    if (!full)
    {
        mmgr_line_put(err_msg, parent);
        mmgr_line_put(err_msg, " is not a full path, such as one Choose Folder gives");
        return 0;
    }
    char target[CLI_PATH_MAX_LEN];
    if (!cli_git_clone_folder(url, parent, target, err_msg) ||
        !cli_git_clone(url, target, net_clone_line, NULL, err_msg))
    {
        return 0;
    }
    cli_window_quoted(out, target);
    return 1;
}

// the folder File, Open, Repository clones into, made where it is not there yet
static int net_repos_folder(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    char folder[CLI_PATH_MAX_LEN];
    if (!cli_git_opened_parent(folder))
    {
        mmgr_line_put(err_msg, "orior_cli has no folder of its own here");
        return 0;
    }
    if (!query_folder_made(folder))
    {
        mmgr_line_put(err_msg, folder);
        mmgr_line_put(err_msg, ": it could not be made");
        return 0;
    }
    cli_window_quoted(out, folder);
    return 1;
}

// the folder File, Open, Repository cloned `url` into before, or null where it has not
static int net_repo_opened(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const url = cli_serve_string(args, "url", err_msg);
    if (url == NULL)
    {
        return 0;
    }
    char parent[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    char git[CLI_PATH_MAX_LEN];
    char quiet[256];
    VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
    if (!cli_git_opened_parent(parent) || !cli_git_clone_folder(url, parent, folder, &ignored))
    {
        mmgr_line_put(out, "null");
        return 1;
    }
    cli_full(folder, ".git", git);
    if (!query_is_file(git) && !query_is_folder(git))
    {
        mmgr_line_put(out, "null");
        return 1;
    }
    cli_window_quoted(out, folder);
    return 1;
}

const CliServeCall g_cli_window_net_calls[] = {
    {"network_disable", net_disable},
    {"network_reached", net_reached},
    {"plugins_catalog_read", net_catalog_read},
    {"plugins_catalog_install", net_catalog_install},
    {"plugin_request", net_plugin_request},
    {"image_data", net_image_data},
    {"link_open", net_link_open},
    {"toolchain_install", net_toolchain_install},
    {"clone_start", net_clone_start},
    {"repo_clone", net_repo_clone},
    {"repos_folder", net_repos_folder},
    {"repo_opened", net_repo_opened},
};
const unsigned int g_cli_window_net_count = sizeof(g_cli_window_net_calls) / sizeof(g_cli_window_net_calls[0]);
