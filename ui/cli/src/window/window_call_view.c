// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_view.c: a page the tree holds opened in a window of its own: the window's label, its title and the
// address it loads from in the window's `view` scheme, which Windows reaches as an http host and the other platforms by
// the scheme's own name; and each file a page window asks for served from under the tree and nothing outside it, read
// where it lies, or over the link where the tree is on another machine
#include "../command_line/cells.h"
#include "../comms/link.h"
#include "../text/base64.h"
#include "../text/text.h"
#include "window_internal.h"

// where the `view` scheme's addresses begin
#if defined(_WIN32)
#define CLI_VIEW_BASE "http://view.localhost/"
#else
#define CLI_VIEW_BASE "view://localhost/"
#endif

// the bytes a file's bytes as base64 are read over the link in, begun at and
#define CLI_VIEW_LINKED_ROOM (64ull << 20u)

// the media type of a page's text, of an error's, and of anything a file's name does not say
#define CLI_VIEW_TEXT "text/plain; charset=utf-8"
#define CLI_VIEW_BYTES "application/octet-stream"

static unsigned long long s_next_view;
static CliLock s_lock = CLI_LOCK_INIT;

// The media type each name a file ends in gives it, as the page windows read it
typedef struct
{
    const char *ending;
    const char *media_type;
} CliViewType;

static const CliViewType s_types[] = {
    {"html", "text/html; charset=utf-8"},
    {"htm", "text/html; charset=utf-8"},
    {"js", "text/javascript; charset=utf-8"},
    {"mjs", "text/javascript; charset=utf-8"},
    {"css", "text/css; charset=utf-8"},
    {"json", "application/json"},
    {"svg", "image/svg+xml"},
    {"png", "image/png"},
    {"jpg", "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"csv", CLI_VIEW_TEXT},
    {"tsv", CLI_VIEW_TEXT},
    {"txt", CLI_VIEW_TEXT},
    {"wav", "audio/wav"},
};

typedef struct
{
    const char *path;
    VerbaLinea *out;
    char digit;
    CliWindowServed served;
    void *context;
    unsigned int status;
    const VerbaLinea *why;
    const char *media_type;
} WindowCallViewCtx;
// the media type the name `path` ends in gives it
EMBED_INLINE const char *view_media_type(const WindowCallViewCtx *args)
{
    const char *ending = args->path;
    const unsigned long long ending_past =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}), .set = ".");
    ending = (ending_past != 0ull) ? (args->path + ending_past) : args->path;
    for (unsigned int at = 0u; (ending != args->path) && (at < sizeof(s_types) / sizeof(s_types[0])); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = ending, .other = s_types[at].ending, .cap = CLI_CELLS_POOL}))
        {
            return s_types[at].media_type;
        }
    }
    return CLI_VIEW_BYTES;
}

// `path` written into an address's path, each byte a path's segment does not save written as `%XX`
EMBED_INLINE void view_path_write(const WindowCallViewCtx *args)
{
    static const char s_digits[] = "0123456789ABCDEF";
    for (const unsigned char *at = (const unsigned char *)args->path; *at != 0u; at += 1)
    {
        const int saved = ((*at >= 'a') && (*at <= 'z')) || ((*at >= 'A') && (*at <= 'Z')) ||
                          ((*at >= '0') && (*at <= '9')) || (*at == '-') || (*at == '_') || (*at == '.') ||
                          (*at == '~') || (*at == '/');
        if (saved)
        {
            // a byte an address saves is an ASCII letter
            mmgr_line_ch(args->out, (char)*at);
        }
        else
        {
            mmgr_line_ch(args->out, '%');
            mmgr_line_ch(args->out, s_digits[*at >> 4u]);
            mmgr_line_ch(args->out, s_digits[*at & 0x0fu]);
        }
    }
}

// the value of the hexadecimal digit `digit`, or 16 where it is none
EMBED_INLINE unsigned int view_hex_get(const WindowCallViewCtx *args)
{
    // a digit's distance from the first of its run is below 16 where the digit is one
    return ((args->digit >= '0') && (args->digit <= '9'))   ? (unsigned int)(args->digit - '0')
           : ((args->digit >= 'a') && (args->digit <= 'f')) ? (unsigned int)(args->digit - 'a') + 10u
           : ((args->digit >= 'A') && (args->digit <= 'F')) ? (unsigned int)(args->digit - 'A') + 10u
                                                            : 16u;
}

// a page under the tree opened in a window of its own: `{label, url, title}`, the file refused where it is none of the
// tree's on this machine
static int view_window(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    CliLinkAddress address;
    const int linked = (path != NULL) && EMBED_CALL(cli_link_text.address, CliLinkTextCfg, .address_out = &address);
    if ((path == NULL) || (!linked && (!EMBED_CALL(cli_serve_err_msg_root.root, CliServeErrMsgRootCfg, .root_out = root,
                                                   .err_msg = err_msg) ||
                                       !EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = root, .relative = path,
                                                   .joined = full, .err_msg = err_msg))))
    {
        return 0;
    }
    if (!linked && !EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = full))
    {
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, " is not a file");
        return 0;
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    s_next_view += 1ull;
    const unsigned long long view = s_next_view;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    mmgr_line_put(out, "{\"label\":\"view-");
    mmgr_line_decimal(out, view, 1u);
    mmgr_line_put(out, "\",\"url\":\"" CLI_VIEW_BASE);
    EMBED_CALL(view_path_write, WindowCallViewCtx, .out = out, .path = path);
    mmgr_line_put(out, "\",\"title\":");
    EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out, .text = path);
    mmgr_line_ch(out, '}');
    return 1;
}

// why a file is not served handed on as its answer's body
EMBED_INLINE void view_reject(const WindowCallViewCtx *args)
{
    args->served(args->context, args->status, CLI_VIEW_TEXT, (const unsigned char *)args->why->out,
                 mmgr_line_ok(args->why) ? args->why->at : 0ull);
}

// the file `path` of the tree on another machine read over the link, its bytes as base64 made bytes again, and handed
// to `served`
EMBED_INLINE void view_linked_serve(const WindowCallViewCtx *args)
{
    char why[CLI_PATH_MAX_LEN + 64u];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    char args_text[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea arguments = {args_text, sizeof(args_text), 0ull};
    mmgr_line_put(&arguments, "{\"path\":");
    EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = &arguments, .text = args->path);
    mmgr_line_ch(&arguments, '}');
    char *const read_cell =
        mmgr_line_ok(&arguments) ? g_cli_cells->temporary_buf_alloc(CLI_VIEW_LINKED_ROOM, &g_cli_window_cells) : NULL;
    VerbaLinea read = mmgr_line_over(read_cell, CLI_VIEW_LINKED_ROOM);
    if (read_cell == NULL)
    {
        mmgr_line_put(&err_msg, "the file's cells could not be taken");
        EMBED_CALL(view_reject, WindowCallViewCtx, .served = args->served, .context = args->context, .status = 404u,
                   .why = &err_msg);
        return;
    }
    const int answered =
        EMBED_CALL(cli_window_out_size_internal.tree_call, CliWindowOutSizeCtx, .name = "file_bytes",
                   .args_text = arguments.out, .size = arguments.at, .out = &read, .err_msg = &err_msg) &&
        mmgr_line_ok(&read) && (read.at >= 2ull);
    unsigned char *const bytes =
        answered ? g_cli_cells->temporary_buf_alloc(((read.at / 4ull) * 3ull) + 3ull, &g_cli_window_cells) : NULL;
    unsigned long long size = 0ull;
    // the answer is a JSON string of base64, whose letters need no escape, its quotes about it
    const int decoded = (bytes != NULL) && EMBED_CALL(cli_base64_text.unbase64, CliBase64TextCfg, .text = read.out + 1,
                                                      .length = read.at - 2ull, .out_bytes = bytes,
                                                      .room = ((read.at / 4ull) * 3ull) + 3ull, .size_pointer = &size);
    if (decoded)
    {
        args->served(args->context, 200u, args->media_type, bytes, size);
    }
    else
    {
        if (answered)
        {
            mmgr_line_put(&err_msg, args->path);
            mmgr_line_put(&err_msg, " is not a file");
        }
        EMBED_CALL(view_reject, WindowCallViewCtx, .served = args->served, .context = args->context, .status = 404u,
                   .why = &err_msg);
    }
    g_cli_cells->persistent_buf_release(bytes, &g_cli_window_cells);
    g_cli_cells->persistent_buf_release(read.out, &g_cli_window_cells);
}

void cli_window_view_served(const char *path, unsigned long long size, CliWindowServed served, void *context)
{
    char decoded[CLI_PATH_MAX_LEN];
    VerbaLinea named = {decoded, sizeof(decoded), 0ull};
    const unsigned long long lead = ((size > 0ull) && (path[0] == '/')) ? 1ull : 0ull;
    EMBED_CALL(cli_text_size_set.put_unescaped, CliTextSizeSetCfg, .out = &named, .text = path + lead,
               .size = size - lead);
    char why[CLI_PATH_MAX_LEN + 64u];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (mmgr_line_finish(&named) == 0ull)
    {
        mmgr_line_put(&err_msg, "the path is longer than a file's");
        EMBED_CALL(view_reject, WindowCallViewCtx, .served = served, .context = context, .status = 404u,
                   .why = &err_msg);
        return;
    }
    const char *const media_type = EMBED_CALL(view_media_type, WindowCallViewCtx, .path = decoded);
    CliLinkAddress address;
    if (EMBED_CALL(cli_link_text.address, CliLinkTextCfg, .address_out = &address))
    {
        EMBED_CALL(view_linked_serve, WindowCallViewCtx, .path = decoded, .media_type = media_type, .served = served,
                   .context = context);
        return;
    }
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_root.root, CliServeErrMsgRootCfg, .root_out = root, .err_msg = &err_msg))
    {
        EMBED_CALL(view_reject, WindowCallViewCtx, .served = served, .context = context, .status = 404u,
                   .why = &err_msg);
        return;
    }
    if (!EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = root, .relative = decoded, .joined = full,
                    .err_msg = &err_msg))
    {
        EMBED_CALL(view_reject, WindowCallViewCtx, .served = served, .context = context, .status = 403u,
                   .why = &err_msg);
        return;
    }
    unsigned long long file_size = 0ull;
    const unsigned char *const mapped =
        EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = full)
            ? EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = full, .size_pointer = &file_size,
                         .owner = &g_cli_window_cells)
            : NULL;
    if ((mapped == NULL) &&
        (!EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = full) ||
         !EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = full, .size_pointer = &file_size) ||
         (file_size > 0ull)))
    {
        mmgr_line_put(&err_msg, decoded);
        mmgr_line_put(&err_msg, " is not a file");
        EMBED_CALL(view_reject, WindowCallViewCtx, .served = served, .context = context, .status = 404u,
                   .why = &err_msg);
        return;
    }
    served(context, 200u, media_type, (mapped != NULL) ? mapped : (const unsigned char *)"", file_size);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = mapped, .owner = &g_cli_window_cells);
}

const CliServeCall g_cli_window_view_calls[] = {
    {"view_window", view_window},
};
const unsigned int g_cli_window_view_count = sizeof(g_cli_window_view_calls) / sizeof(g_cli_window_view_calls[0]);
