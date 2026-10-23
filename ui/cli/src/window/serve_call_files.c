// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_files.c: the calls of one file of the tree: made, a folder made, its path as the tree names it, read for
// the editor, read as a window of lines or a slice, read as bytes, written, written with the rights the system asks
// for, its snapshots listed and read; the bridge read; files from outside the window copied into a folder of the tree
// (serve_call_bring.c); a file brought from the window's machine as files bring it over, a name for it then its
// parts; and the tree's trust told and set, the servers starting again with the tree's code or without it as the
// answer has it
#include "../command_line/cells.h"
#include "../editor/bridge.h"
#include "../editor/files.h"
#include "../editor/history.h"
#include "../machine/raised.h"
#include "../machine/trust.h"
#include "../text/base64.h"
#include "serve_internal.h"

// the bytes a file's text read is begun in before it grows
#define CLI_SERVE_TEXT_FIRST (64ull << 10u)

// the cells a file's text and bytes are held in as they are read and written, each sized to them, and the lock a
// file's text is read, written over and kept in its history under, one write at a time
static CliCellsTally s_cells = CLI_CELLS_TALLY("cli_serve_files");
CliLock g_cli_serve_write_lock = CLI_LOCK_INIT;

// a growing line begun for a file's text: 1, or 0 with why said where the cells cannot meet it
static int text_line_begun(CliLine *text, CliLine *said)
{
    const int begun = cli_line_growing(text, CLI_SERVE_TEXT_FIRST, "a file's text");
    if (!begun)
    {
        cli_line_text(said, "a file's text could not be taken: the cells cannot meet it");
    }
    return begun;
}

static int file_create(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    const char *const path = cli_serve_string(args, "path", said);
    if ((path == NULL) || !cli_serve_tree(root, said) || !cli_files_create_file(root, path, said))
    {
        return 0;
    }
    cli_line_text(out, "null");
    return 1;
}

static int folder_create(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    const char *const path = cli_serve_string(args, "path", said);
    if ((path == NULL) || !cli_serve_tree(root, said) || !cli_files_create_folder(root, path, said))
    {
        return 0;
    }
    cli_line_text(out, "null");
    return 1;
}

static int tree_relative(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    const char *const path = cli_serve_string(args, "path", said);
    if (path == NULL)
    {
        return 0;
    }
    // a file under a folder mounted beside the tree goes by its mount's name as well
    if (!cli_serve_root_now(root) || !query_path_full(path, full, sizeof(full)))
    {
        cli_line_text(out, "null");
        return 1;
    }
    char shown[CLI_PATH_LONGEST];
    CliLine line = {shown, sizeof(shown), 0ull};
    cli_servers_tree_path(root, full, &line);
    const int inside =
        cli_line_fits(&line) && (!cellul.eq(&(CatenaFinitaCfg){.src = shown, .other = full, .cap = CLI_CELLS_POOL}));
    if (inside)
    {
        cli_json_quoted_lossy(out, (const unsigned char *)shown, line.at);
    }
    else
    {
        cli_line_text(out, "null");
    }
    return 1;
}

static int file_read(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    const char *const path = cli_serve_string(args, "path", said);
    return (path != NULL) && cli_serve_tree(root, said) && cli_files_read(root, path, out, said);
}

// whether git ignores the file of the argument `path`: true where it neither tracks it nor would
static int file_ignored(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    const char *const path = cli_serve_string(args, "path", said);
    if ((path == NULL) || !cli_serve_tree(root, said))
    {
        return 0;
    }
    cli_line_text(out, cli_files_tracked(root, path) ? "false" : "true");
    return 1;
}

static int bridge_read(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    char root[CLI_PATH_LONGEST];
    if (!cli_serve_tree(root, said))
    {
        return 0;
    }
    CliJson json;
    const unsigned int bridge = cli_bridge_read(&json, root);
    if (bridge != CLI_JSON_NONE)
    {
        cli_json_write(&json, bridge, out, 0u, 0);
    }
    else
    {
        cli_line_text(out, "null");
    }
    cli_json_given(&json);
    return 1;
}

static int file_slice(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    unsigned long long start = 0ull;
    unsigned long long end = 0ull;
    const char *const path = cli_serve_string(args, "path", said);
    if ((path == NULL) || !cli_serve_whole(args, "start", &start, said) || !cli_serve_whole(args, "end", &end, said) ||
        !cli_serve_tree(root, said))
    {
        return 0;
    }
    CliLine slice;
    if (!text_line_begun(&slice, said))
    {
        return 0;
    }
    const int done = cli_files_slice(root, path, start, end, &slice, said);
    if (done)
    {
        // the page takes the slice's start, end, size and text
        static const char *const s_kept[4] = {"start", "end", "size", "text"};
        cli_line_character(out, '{');
        for (unsigned int at = 0u; at < 4u; at += 1u)
        {
            unsigned long long from = 0ull;
            unsigned long long length = 0ull;
            cli_line_text(out, (at > 0u) ? ",\"" : "\"");
            cli_line_text(out, s_kept[at]);
            cli_line_text(out, "\":");
            if (cli_lsp_member(slice.out, slice.at, s_kept[at], &from, &length))
            {
                cli_line_counted(out, slice.out + from, length);
            }
            else
            {
                cli_line_text(out, "null");
            }
        }
        cli_line_character(out, '}');
    }
    cli_cell_given(slice.out);
    return done;
}

static int file_bytes(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    const char *const path = cli_serve_string(args, "path", said);
    if ((path == NULL) || !cli_serve_tree(root, said) || !cli_inside(root, path, full, said))
    {
        return 0;
    }
    if (!query_is_file(full))
    {
        cli_line_text(said, path);
        cli_line_text(said, " is not a file");
        return 0;
    }
    unsigned long long size = 0ull;
    const unsigned char *const mapped = cli_file_held(full, &size);
    if ((mapped == NULL) && (!cli_file_size(full, &size) || (size > 0ull)))
    {
        cli_line_text(said, full);
        cli_line_text(said, ": it could not be read");
        return 0;
    }
    cli_line_character(out, '"');
    cli_base64(out, (mapped != NULL) ? mapped : (const unsigned char *)"", size);
    cli_line_character(out, '"');
    if (mapped != NULL)
    {
        cli_file_let_go(mapped, size);
    }
    return 1;
}

static int file_write_raised(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    unsigned long long length = 0ull;
    const char *const path = cli_serve_string(args, "path", said);
    const char *const text = (path != NULL) ? cli_serve_string(args, "text", said) : NULL;
    if ((text == NULL) || !cli_serve_tree(root, said) || !cli_inside(root, path, full, said))
    {
        return 0;
    }
    (void)cli_json_string(args->json, cli_serve_arg(args, "text"), &length);
    if (!cli_raised_write(full, text, length, said))
    {
        return 0;
    }
    cli_line_text(out, "null");
    return 1;
}

int cli_serve_file_write(const char *path, const char *text, unsigned long long length, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    if (!cli_serve_tree(root, said))
    {
        return 0;
    }
    cli_full(root, path, full);
    // the text the file held is copied out before it is written over, as the write puts a new file in its place
    cli_lock_take(&g_cli_serve_write_lock);
    unsigned long long held = 0ull;
    char *const was = cli_file_size(full, &held)
                          ? cli_cell_taken_by(&s_cells, held + 1ull, "a file's text before it is written")
                          : NULL;
    const unsigned long long before =
        (was != NULL) ? qry_member_file_read(full, (unsigned char *)was, held + 1ull) : QRY_MEMBER_UNREAD;
    const int had = (before != QRY_MEMBER_UNREAD) && cli_utf8_whole(was, before);
    if (!cli_files_write(root, path, text, length, said))
    {
        cli_cell_given_by(&s_cells, was);
        cli_lock_give(&g_cli_serve_write_lock);
        return 0;
    }
    char why[1024];
    CliLine kept = {why, sizeof(why), 0ull};
    if (!cli_history_keep(root, path, had ? was : NULL, had ? before : 0ull, text, length, &kept))
    {
        char told[2048];
        CliLine line = {told, sizeof(told), 0ull};
        cli_line_text(&line, "orior: local history of ");
        cli_line_text(&line, path);
        cli_line_text(&line, ": ");
        cli_line_counted(&line, why, cli_line_fits(&kept) ? kept.at : 0ull);
        cli_line_character(&line, '\n');
        query_terminal_written(1, told, cli_line_fits(&line) ? line.at : 0ull);
    }
    cli_cell_given_by(&s_cells, was);
    cli_lock_give(&g_cli_serve_write_lock);
    return 1;
}

static int file_write(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned long long length = 0ull;
    const char *const path = cli_serve_string(args, "path", said);
    const char *const text = (path != NULL) ? cli_serve_string(args, "text", said) : NULL;
    if (text == NULL)
    {
        return 0;
    }
    (void)cli_json_string(args->json, cli_serve_arg(args, "text"), &length);
    if (!cli_serve_file_write(path, text, length, said))
    {
        return 0;
    }
    cli_line_text(out, "null");
    return 1;
}

static int file_part(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    unsigned long long at = 0ull;
    int last = 0;
    unsigned long long length = 0ull;
    const char *const path = cli_serve_string(args, "path", said);
    const char *const data = (path != NULL) ? cli_serve_string(args, "data", said) : NULL;
    if ((data == NULL) || !cli_serve_whole(args, "at", &at, said) || !cli_serve_bool(args, "last", &last, said) ||
        !cli_serve_tree(root, said))
    {
        return 0;
    }
    (void)cli_json_string(args->json, cli_serve_arg(args, "data"), &length);
    // base64 holds three bytes in each four letters
    const unsigned long long room = ((length / 4ull) + 1ull) * 3ull;
    char *const bytes = cli_cell_taken_by(&s_cells, room, "a file's part");
    if (bytes == NULL)
    {
        cli_line_text(said, "a file's part could not be taken: the cells cannot meet it");
        return 0;
    }
    unsigned long long size = 0ull;
    int done = cli_unbase64(data, length, (unsigned char *)bytes, room, &size);
    if (!done)
    {
        cli_line_text(said, "a part not in base64");
    }
    done = done && cli_files_write_part(root, path, at, bytes, size, last, said);
    cli_cell_given_by(&s_cells, bytes);
    cli_line_text(out, done ? "null" : "");
    return done;
}

static int trust_read(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    char root[CLI_PATH_LONGEST];
    if (!cli_serve_tree(root, said))
    {
        return 0;
    }
    cli_trust_told(root, out);
    return 1;
}

static int trust_set(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    int trusted = 0;
    const char *const folder = cli_serve_string(args, "folder", said);
    if ((folder == NULL) || !cli_serve_bool(args, "trusted", &trusted, said) || !cli_trust_set(folder, trusted, said))
    {
        return 0;
    }
    // the servers start again, with the tree's code or without it as the answer has it
    cli_servers_let_go();
    cli_line_text(out, "null");
    return 1;
}

static int history_list(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    const char *const path = cli_serve_string(args, "path", said);
    if ((path == NULL) || !cli_serve_tree(root, said))
    {
        return 0;
    }
    cli_history_list(root, path, out);
    return 1;
}

static int history_read(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    unsigned long long at = 0ull;
    const char *const path = cli_serve_string(args, "path", said);
    if ((path == NULL) || !cli_serve_whole(args, "at", &at, said) || !cli_serve_tree(root, said))
    {
        return 0;
    }
    CliLine text;
    if (!text_line_begun(&text, said))
    {
        return 0;
    }
    const int done = cli_history_read(root, path, at, &text, said);
    if (done)
    {
        cli_json_quoted_lossy(out, (const unsigned char *)text.out, cli_line_fits(&text) ? text.at : 0ull);
    }
    cli_cell_given(text.out);
    return done;
}

const CliServeCall g_cli_serve_files_calls[] = {
    {"file_create", file_create},         {"folder_create", folder_create},
    {"tree_relative", tree_relative},     {"file_read", file_read},
    {"file_ignored", file_ignored},       {"bridge_read", bridge_read},
    {"file_slice", file_slice},
    {"file_bytes", file_bytes},           {"file_write_raised", file_write_raised},
    {"file_write", file_write},           {"files_bring", cli_serve_files_bring},
    {"bring_name", cli_serve_bring_name}, {"file_part", file_part},
    {"trust_read", trust_read},           {"trust_set", trust_set},
    {"history_list", history_list},       {"history_read", history_read},
};
const unsigned int g_cli_serve_files_count = sizeof(g_cli_serve_files_calls) / sizeof(g_cli_serve_files_calls[0]);
