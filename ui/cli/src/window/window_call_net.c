// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_net.c: the window's calls that reach past this machine or into the reader's files anywhere: orior_cli
// blocked from the network or let on and the addresses it reached, the plugin catalog read and installed from, a
// plugin's request sent, a picture read as a data address, a page opened in the browser, a toolchain's install page,
// and a repository cloned, its progress reported as it comes
#include "../code/git.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
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

// the bytes a catalog entry's line is taken with

static CliLock s_lock = CLI_LOCK_INIT;

// orior_cli blocked from the network, or let on, as the reader's setting `network` specifies
static int net_disable(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    int off = 0;
    if (!EMBED_CALL(cli_serve_err_msg_internal.serve_bool, CliServeErrMsgCtx, .request = arguments, .key = "off",
                    .value_i32s = &off, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_net.disable, CliNetCfg, .off = off);
    return 1;
}

// the addresses orior_cli has reached, the last first
static int net_reach(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    EMBED_CALL(cli_net_most.reach, CliNetMostCfg, .most = 1000u, .out = out);
    return 1;
}

// the plugins the catalog at `address` lists
static int net_catalog_read(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const address = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                           .key = "address", .err_msg = err_msg);
    return (address != NULL) && EMBED_CALL(cli_plugin_catalog_address.read, CliPluginCatalogAddressCfg,
                                           .address = address, .out = out, .err_msg = err_msg);
}

// the plugin `entry` of the catalog at `address` installed: its folder
static int net_catalog_install(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const address = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                           .key = "address", .err_msg = err_msg);
    if (address == NULL)
    {
        return 0;
    }
    const unsigned long long entry_room =
        EMBED_CALL(cli_json_tree_node_out.json_write_room, CliJsonTreeNodeOutCfg, .json_read = arguments->json);
    char *const entry_cell = g_cli_cells->temporary_buf_alloc(entry_room, &g_cli_window_cells);
    VerbaLinea entry = mmgr_line_over(entry_cell, entry_room);
    if (entry.out == NULL)
    {
        mmgr_line_put(err_msg, "the catalog's entry could not be stored");
        return 0;
    }
    EMBED_CALL(cli_window_out_request_internal.arg_write, CliWindowOutRequestCtx, .request = arguments, .key = "entry",
               .out = &entry);
    char folder_text[CLI_PATH_MAX_LEN];
    VerbaLinea folder = {folder_text, sizeof(folder_text), 0ull};
    const int installed = mmgr_line_ok(&entry) &&
                          EMBED_CALL(cli_plugin_catalog_address.install, CliPluginCatalogAddressCfg, .address = address,
                                     .entry = entry.out, .size = entry.at, .out = &folder, .err_msg = err_msg);
    if (installed)
    {
        EMBED_CALL(cli_window_out_text_internal.count, CliWindowOutTextCtx, .out = out, .text = folder_text,
                   .size = mmgr_line_ok(&folder) ? folder.at : 0ull);
    }
    g_cli_cells->persistent_buf_release(entry.out, &g_cli_window_cells);
    return installed;
}

// a request the plugin `id` sends to a host it requested to reach, as the window lets it: its answer's status, headers
// and body, the body in base64 where it is not text
static int net_plugin_request(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    static CliRequestsSent s_sent;
    const char *const id = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                      .key = "id", .err_msg = err_msg);
    const char *const method = (id != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                         .request = arguments, .key = "method", .err_msg = err_msg)
                                            : NULL;
    const char *const url = (method != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                          .request = arguments, .key = "url", .err_msg = err_msg)
                                             : NULL;
    const char *const body = (url != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                        .request = arguments, .key = "body", .err_msg = err_msg)
                                           : NULL;
    if (body == NULL)
    {
        return 0;
    }
    char purpose[CLI_NAME_MAX_LEN + 16u];
    VerbaLinea named = {purpose, sizeof(purpose), 0ull};
    mmgr_line_put(&named, "the plugin ");
    mmgr_line_put(&named, id);
    (void)mmgr_line_finish(&named);
    if (!EMBED_CALL(cli_net_address.reach, CliNetAddressCfg, .address = url, .purpose = purpose, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    memor.set(&(MemoriaCfg){.dst = &s_sent, .val = 0, .bytes = sizeof(s_sent)});
    memor.cpy(&(MemoriaCfg){.dst = s_sent.method,
                            .src = method,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = method, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = s_sent.url,
                            .src = url,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = url, .cap = CLI_CELLS_POOL}) + 1u});
    const unsigned int headers =
        EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "headers");
    const unsigned int first = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                          .json_read = arguments->json, .node = headers, .kind = CFG_JSON_ARRAY)
                                   ? arguments->json->node[headers].first
                                   : CLI_JSON_NONE;
    for (unsigned int pair = first; (pair != CLI_JSON_NONE) && (s_sent.headers < CLI_REQUESTS_HEADERS);
         pair = arguments->json->node[pair].next)
    {
        const char *const name =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = arguments->json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = arguments->json, .array = pair, .index = 0u),
                       .length_pointer = NULL);
        const char *const value =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = arguments->json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = arguments->json, .array = pair, .index = 1u),
                       .length_pointer = NULL);
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
    CliRequestsRoom *const room = cli_requests_room_take();
    if (room == NULL)
    {
        mmgr_line_put(err_msg, "the request's room could not be taken");
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
        return 0;
    }
    EMBED_CALL(cli_requests_sent_internal.send, CliRequestsSentCtx, .sent = &s_sent, .verify = 1, .room = room);
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
            EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out,
                       .text = EMBED_CALL(cli_requests_pool_internal.text, CliRequestsPoolCtx, .pool = answer->head,
                                          .text_requests_text = answer->header_name[at]));
            mmgr_line_ch(out, ',');
            EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out,
                       .text = EMBED_CALL(cli_requests_pool_internal.text, CliRequestsPoolCtx, .pool = answer->head,
                                          .text_requests_text = answer->header_value[at]));
            mmgr_line_ch(out, ']');
        }
        mmgr_line_put(out, "],\"body\":");
        if (answer->binary)
        {
            mmgr_line_ch(out, '"');
            EMBED_CALL(cli_base64_table.base64, CliBase64Cfg, .out = out, .bytes = answer->body, .size = answer->size);
            mmgr_line_ch(out, '"');
        }
        else
        {
            EMBED_CALL(cli_window_out_text_internal.count, CliWindowOutTextCtx, .out = out,
                       .text = (const char *)answer->body, .size = answer->size);
        }
        mmgr_line_put(out, answer->binary ? ",\"binary\":true}" : ",\"binary\":false}");
    }
    EMBED_CALL(cli_requests_size_err_msg_internal.room_free, CliRequestsSizeErrMsgCtx, .room = room);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    return answered;
}

// the picture at `path` as the data address a theme stores for the image behind the editor: PNG, JPEG, GIF, WebP, SVG
// or BMP, of 8 MB at most
static int net_image_data(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    static const char *const s_kinds[7][2] = {{"png", "image/png"}, {"jpg", "image/jpeg"},  {"jpeg", "image/jpeg"},
                                              {"gif", "image/gif"}, {"webp", "image/webp"}, {"svg", "image/svg+xml"},
                                              {"bmp", "image/bmp"}};
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    if (path == NULL)
    {
        return 0;
    }
    const char *const dot =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = path,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), .byte = '.');
    const char *const slash =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = path,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), .byte = '/');
    const char *const back =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = path,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), .byte = '\\');
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
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = path,
                                                  .size_pointer = &size, .owner = &g_cli_window_cells);
    if (bytes == NULL)
    {
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, ": it could not be read");
        return 0;
    }
    if (size > CLI_WINDOW_IMAGE_MOST)
    {
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_window_cells);
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, " is over 8 MB, the most an image behind the editor is");
        return 0;
    }
    mmgr_line_put(out, "\"data:");
    mmgr_line_put(out, kind);
    mmgr_line_put(out, ";base64,");
    EMBED_CALL(cli_base64_table.base64, CliBase64Cfg, .out = out, .bytes = bytes, .size = size);
    mmgr_line_ch(out, '"');
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_window_cells);
    return 1;
}

// an https page a hover or a diagnostic links to opened in the browser
static int net_link_open(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    const char *const url = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "url", .err_msg = err_msg);
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
    return EMBED_CALL(cli_report_err_msg.open_url, CliReportErrMsgCfg, .url = url, .err_msg = err_msg);
}

// a toolchain's install page opened in the browser, and named
static int net_toolchain_install(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const id = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                      .key = "id", .err_msg = err_msg);
    static char s_url[CLI_PATH_MAX_LEN * 4u];
    VerbaLinea url = {s_url, sizeof(s_url), 0ull};
    if ((id == NULL) || !EMBED_CALL(cli_toolchains_id_err_msg.open_install, CliToolchainsIdErrMsgCfg, .id = id,
                                    .out = &url, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_window_out_text_internal.count, CliWindowOutTextCtx, .out = out, .text = s_url,
               .size = mmgr_line_ok(&url) ? url.at : 0ull);
    return 1;
}

// what File, Clone Repository starts from: orior's own repository and the reader's home
static int net_clone_start(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    char parent[CLI_PATH_MAX_LEN] = "";
    (void)EMBED_CALL(cli_git_err_msg_folder.clone_parent, CliGitErrMsgFolderCfg, .folder = parent);
    mmgr_line_put(out, "{\"url\":");
    EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out, .text = CLI_GIT_ORIOR);
    mmgr_line_put(out, ",\"parent\":");
    EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out, .text = parent);
    mmgr_line_ch(out, '}');
    return 1;
}

// a line of git's progress sent to the page as clone-progress
static void net_clone_line(const CliEachLineEndedCfg *args)
{
    char msg_buf[4096];
    VerbaLinea body = {msg_buf, sizeof(msg_buf), 0ull};
    EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = &body, .text = args->line);
    EMBED_CALL(cli_serve_event_internal.emit, CliServeEventCtx, .event = "clone-progress", .body = msg_buf,
               .size = mmgr_line_ok(&body) ? body.at : 0ull);
}

// `url` cloned into a new folder under `parent`, a full path, each line of git's progress reported as it comes: the
// folder
static int net_repo_clone(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const url = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "url", .err_msg = err_msg);
    const char *const parent = (url != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                          .request = arguments, .key = "parent", .err_msg = err_msg)
                                             : NULL;
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
    if (!EMBED_CALL(cli_git_err_msg_folder.clone_folder, CliGitErrMsgFolderCfg, .url = url, .parent = parent,
                    .folder = target, .err_msg = err_msg) ||
        !EMBED_CALL(cli_git_err_msg.clone, CliGitErrMsgCfg, .url = url, .target = target,
                    .progress_line = net_clone_line, .context = NULL, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out, .text = target);
    return 1;
}

// the folder File, Open, Repository clones into, made where it is not there yet
static int net_repos_folder(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_git_err_msg_folder.opened_parent, CliGitErrMsgFolderCfg, .folder = folder))
    {
        mmgr_line_put(args->err_msg, "orior_cli has no folder of its own here");
        return 0;
    }
    if (!EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder))
    {
        mmgr_line_put(args->err_msg, folder);
        mmgr_line_put(args->err_msg, ": it could not be made");
        return 0;
    }
    EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out, .text = folder);
    return 1;
}

// the folder File, Open, Repository cloned `url` into before, or null where it has not
static int net_repo_open(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const url = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "url", .err_msg = err_msg);
    if (url == NULL)
    {
        return 0;
    }
    char parent[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    char git[CLI_PATH_MAX_LEN];
    char quiet[256];
    VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
    if (!EMBED_CALL(cli_git_err_msg_folder.opened_parent, CliGitErrMsgFolderCfg, .folder = parent) ||
        !EMBED_CALL(cli_git_err_msg_folder.clone_folder, CliGitErrMsgFolderCfg, .url = url, .parent = parent,
                    .folder = folder, .err_msg = &ignored))
    {
        mmgr_line_put(out, "null");
        return 1;
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = ".git", .joined = git);
    if (!EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = git) &&
        !EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = git))
    {
        mmgr_line_put(out, "null");
        return 1;
    }
    EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out, .text = folder);
    return 1;
}

const CliServeCall g_cli_window_net_calls[] = {
    {"network_disable", net_disable},
    {"network_reached", net_reach},
    {"plugins_catalog_read", net_catalog_read},
    {"plugins_catalog_install", net_catalog_install},
    {"plugin_request", net_plugin_request},
    {"image_data", net_image_data},
    {"link_open", net_link_open},
    {"toolchain_install", net_toolchain_install},
    {"clone_start", net_clone_start},
    {"repo_clone", net_repo_clone},
    {"repos_folder", net_repos_folder},
    {"repo_opened", net_repo_open},
};
const unsigned int g_cli_window_net_count = sizeof(g_cli_window_net_calls) / sizeof(g_cli_window_net_calls[0]);
