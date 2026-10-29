// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_view.c: a page the tree holds opened in a window of its own: the window's label, its title and the
// address it loads from in the window's `view` scheme, which Windows reaches as an http host and the other platforms by
// the scheme's own name; and each file a page window asks for served from under the tree and nothing outside it, read
// where it lies, or over the link where the tree is on another machine
#include "../comms/link.h"
#include "../text/base64.h"
#include "window_internal.h"

// where the `view` scheme's addresses begin
#if defined(_WIN32)
#define CLI_VIEW_BASE "http://view.localhost/"
#else
#define CLI_VIEW_BASE "view://localhost/"
#endif

// the bytes a file's bytes as base64 are read over the link in, begun at and doubling as they fill
#define CLI_VIEW_LINKED_FIRST (64ull << 10u)

// the media type of a page's own text, of an error's, and of anything a file's name does not say
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

// the media type the name `path` ends in gives it
static const char *view_media_type(const char *path)
{
    const char *ending = path;
    for (const char *at = path; *at != '\0'; at += 1)
    {
        ending = (*at == '.') ? (at + 1) : ending;
    }
    for (unsigned int at = 0u; (ending != path) && (at < sizeof(s_types) / sizeof(s_types[0])); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = ending, .other = s_types[at].ending, .cap = CLI_CELLS_POOL}))
        {
            return s_types[at].media_type;
        }
    }
    return CLI_VIEW_BYTES;
}

// `path` written into an address's path, each byte a path's segment does not keep written as `%XX`
static void view_path_written(VerbaLinea *out, const char *path)
{
    static const char s_digits[] = "0123456789ABCDEF";
    for (const unsigned char *at = (const unsigned char *)path; *at != 0u; at += 1)
    {
        const int kept = ((*at >= 'a') && (*at <= 'z')) || ((*at >= 'A') && (*at <= 'Z')) ||
                         ((*at >= '0') && (*at <= '9')) || (*at == '-') || (*at == '_') || (*at == '.') ||
                         (*at == '~') || (*at == '/');
        if (kept)
        {
            // a byte an address keeps is an ASCII letter
            mmgr_line_ch(out, (char)*at);
        }
        else
        {
            mmgr_line_ch(out, '%');
            mmgr_line_ch(out, s_digits[*at >> 4u]);
            mmgr_line_ch(out, s_digits[*at & 0x0fu]);
        }
    }
}

// the value of the hexadecimal digit `digit`, or 16 where it is none
static unsigned int view_hex_of(char digit)
{
    // a digit's distance from the first of its run is below 16 where the digit is one
    return ((digit >= '0') && (digit <= '9'))   ? (unsigned int)(digit - '0')
           : ((digit >= 'a') && (digit <= 'f')) ? (unsigned int)(digit - 'a') + 10u
           : ((digit >= 'A') && (digit <= 'F')) ? (unsigned int)(digit - 'A') + 10u
                                                : 16u;
}

// a page under the tree opened in a window of its own: `{label, url, title}`, the file refused where it is none of the
// tree's on this machine
static int view_window(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    const char *const path = cli_serve_string(args, "path", err_msg);
    CliLinkAddress address;
    const int linked = (path != NULL) && cli_link_address(&address);
    if ((path == NULL) || (!linked && (!cli_serve_root(root, err_msg) || !cli_inside(root, path, full, err_msg))))
    {
        return 0;
    }
    if (!linked && !query_is_file(full))
    {
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, " is not a file");
        return 0;
    }
    cli_lock_take(&s_lock);
    s_next_view += 1ull;
    const unsigned long long view = s_next_view;
    cli_lock_give(&s_lock);
    mmgr_line_put(out, "{\"label\":\"view-");
    mmgr_line_decimal(out, view, 1u);
    mmgr_line_put(out, "\",\"url\":\"" CLI_VIEW_BASE);
    view_path_written(out, path);
    mmgr_line_put(out, "\",\"title\":");
    cli_window_quoted(out, path);
    mmgr_line_ch(out, '}');
    return 1;
}

// why a file is not served handed on as its answer's body
static void view_refused(CliWindowServed served, void *context, unsigned int status, const VerbaLinea *why)
{
    served(context, status, CLI_VIEW_TEXT, (const unsigned char *)why->out, mmgr_line_ok(why) ? why->at : 0ull);
}

// the file `path` of the tree on another machine read over the link, its bytes as base64 made bytes again, and handed
// to `served`
static void view_linked_served(const char *path, const char *media_type, CliWindowServed served, void *context)
{
    char why[CLI_PATH_MAX_LEN + 64u];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    char args_text[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea args = {args_text, sizeof(args_text), 0ull};
    mmgr_line_put(&args, "{\"path\":");
    cli_window_quoted(&args, path);
    mmgr_line_ch(&args, '}');
    VerbaLinea read;
    if (!mmgr_line_ok(&args) || !cli_growing_line_made(&read, CLI_VIEW_LINKED_FIRST))
    {
        mmgr_line_put(&err_msg, "the file's cells could not be taken");
        view_refused(served, context, 404u, &err_msg);
        return;
    }
    const int answered = cli_window_tree_call("file_bytes", args.out, args.at, &read, &err_msg) &&
                         mmgr_line_ok(&read) && (read.at >= 2ull);
    unsigned char *const bytes =
        answered ? g_cli_cells->persistent_buf_alloc(((read.at / 4ull) * 3ull) + 3ull, NULL) : NULL;
    unsigned long long size = 0ull;
    // the answer is a JSON string of base64, whose letters need no escape, its quotes about it
    const int decoded =
        (bytes != NULL) && cli_unbase64(read.out + 1, read.at - 2ull, bytes, ((read.at / 4ull) * 3ull) + 3ull, &size);
    if (decoded)
    {
        served(context, 200u, media_type, bytes, size);
    }
    else
    {
        if (answered)
        {
            mmgr_line_put(&err_msg, path);
            mmgr_line_put(&err_msg, " is not a file");
        }
        view_refused(served, context, 404u, &err_msg);
    }
    g_cli_cells->persistent_buf_release(bytes, NULL);
    g_cli_cells->persistent_buf_release(read.out, NULL);
}

void cli_window_view_served(const char *path, unsigned long long size, CliWindowServed served, void *context)
{
    char decoded[CLI_PATH_MAX_LEN];
    VerbaLinea named = {decoded, sizeof(decoded), 0ull};
    for (unsigned long long at = ((size > 0ull) && (path[0] == '/')) ? 1ull : 0ull; at < size; at += 1ull)
    {
        const int escaped = (path[at] == '%') && (at + 2ull < size) && (view_hex_of(path[at + 1ull]) < 16u) &&
                            (view_hex_of(path[at + 2ull]) < 16u);
        // two hexadecimal digits make a byte, below 256
        mmgr_line_ch(&named,
                     escaped ? (char)((view_hex_of(path[at + 1ull]) << 4u) | view_hex_of(path[at + 2ull])) : path[at]);
        at += escaped ? 2ull : 0ull;
    }
    char why[CLI_PATH_MAX_LEN + 64u];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (mmgr_line_finish(&named) == 0ull)
    {
        mmgr_line_put(&err_msg, "the path is longer than a file's");
        view_refused(served, context, 404u, &err_msg);
        return;
    }
    const char *const media_type = view_media_type(decoded);
    CliLinkAddress address;
    if (cli_link_address(&address))
    {
        view_linked_served(decoded, media_type, served, context);
        return;
    }
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    if (!cli_serve_root(root, &err_msg))
    {
        view_refused(served, context, 404u, &err_msg);
        return;
    }
    if (!cli_inside(root, decoded, full, &err_msg))
    {
        view_refused(served, context, 403u, &err_msg);
        return;
    }
    unsigned long long file_size = 0ull;
    const unsigned char *const mapped = query_is_file(full) ? cli_file_read(full, &file_size) : NULL;
    if ((mapped == NULL) && (!query_is_file(full) || !cli_file_size(full, &file_size) || (file_size > 0ull)))
    {
        mmgr_line_put(&err_msg, decoded);
        mmgr_line_put(&err_msg, " is not a file");
        view_refused(served, context, 404u, &err_msg);
        return;
    }
    served(context, 200u, media_type, (mapped != NULL) ? mapped : (const unsigned char *)"", file_size);
    if (mapped != NULL)
    {
        cli_file_let_go(mapped, file_size);
    }
}

const CliServeCall g_cli_window_view_calls[] = {
    {"view_window", view_window},
};
const unsigned int g_cli_window_view_count = sizeof(g_cli_window_view_calls) / sizeof(g_cli_window_view_calls[0]);
