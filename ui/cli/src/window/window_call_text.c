// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_text.c: the window's calls on a text stored as a branch opened it (window_call_branch.c): its changes
// written as the page makes them, the text let go, its gutter's marks against the last commit and find's matches in
// it; two texts compared by structure; and a file anywhere on the machine read or written as the reader chose it
#include "../code/structure.h"
#include "../command_line/cells.h"
#include "../deploy/devcontainer.h"
#include "../editor/document.h"
#include "window_internal.h"

// the bytes the tree's answer is begun in before it grows
#define CLI_WINDOW_TEXT_ANSWER (64ull << 10u)

// a text's shape written as the page reads it
static void text_shape_written(const CliShape *shape, VerbaLinea *out)
{
    mmgr_line_put(out, "{\"lines\":");
    mmgr_line_decimal(out, shape->lines, 1u);
    mmgr_line_put(out, ",\"units\":");
    mmgr_line_decimal(out, shape->units, 1u);
    mmgr_line_ch(out, '}');
}

unsigned int cli_window_tree_called(CliJson *json, const char *name, const char *path, const char *more,
                                    VerbaLinea *err_msg)
{
    cli_json_begun(json, NULL, 0u, NULL, 0ull);
    char args[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea line = {args, sizeof(args), 0ull};
    mmgr_line_put(&line, "{\"path\":");
    cli_window_quoted(&line, path);
    mmgr_line_put(&line, more);
    mmgr_line_ch(&line, '}');
    VerbaLinea answer;
    if (!cli_growing_line_temporary(&answer, CLI_WINDOW_TEXT_ANSWER, &g_cli_window_text_cells))
    {
        mmgr_line_put(err_msg, "the tree's answer on a text could not be taken");
        return CLI_JSON_NONE;
    }
    unsigned int read = CLI_JSON_NONE;
    if (mmgr_line_ok(&line) && cli_window_tree_call(name, args, line.at, &answer, err_msg))
    {
        read = cli_json_taken(json, answer.out, mmgr_line_ok(&answer) ? answer.at : 0ull, 0u, 0ull, err_msg,
                              &g_cli_window_text_cells);
    }
    g_cli_cells->persistent_buf_release(answer.out, &g_cli_window_text_cells);
    return read;
}

// a place argument `{line, col}` of an edit read
static CliPlace text_place(const CliJson *json, unsigned int edit, const char *name)
{
    const unsigned int place = cli_json_member(json, edit, name);
    CliPlace read = {0ull, 0ull};
    const char *const keys[2] = {"line", "col"};
    unsigned long long *const into[2] = {&read.line, &read.col};
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        const unsigned int number = cli_json_member(json, place, keys[at]);
        if (cli_json_is(json, number, CFG_JSON_NUMBER))
        {
            for (unsigned long long digit = 0ull; digit < json->node[number].length; digit += 1ull)
            {
                const char letter = json->text[json->node[number].text + digit];
                if ((letter < '0') || (letter > '9'))
                {
                    break;
                }
                *into[at] = (*into[at] * 10ull) + (unsigned long long)(letter - '0');
            }
        }
    }
    return read;
}

// the items of the array `node`, 0 where it is not one
static unsigned int text_items(const CliJson *json, unsigned int node)
{
    unsigned int count = 0u;
    const unsigned int first = cli_json_is(json, node, CFG_JSON_ARRAY) ? json->node[node].first : CLI_JSON_NONE;
    for (unsigned int item = first; item != CLI_JSON_NONE; item = json->node[item].next)
    {
        count += 1u;
    }
    return count;
}

// the changes the page made to the text stored under `key` written, in order: its shape
static int text_document_edit(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const key = cli_serve_string(args, "key", err_msg);
    const unsigned int changes = cli_serve_arg(args, "changes");
    if (key == NULL)
    {
        return 0;
    }
    if (!cli_json_is(args->json, changes, CFG_JSON_ARRAY))
    {
        cli_serve_refused(args, changes, "changes", "a sequence", err_msg);
        return 0;
    }
    const unsigned int changes_count = text_items(args->json, changes);
    unsigned int edits_count = 0u;
    for (unsigned int change = args->json->node[changes].first; change != CLI_JSON_NONE;
         change = args->json->node[change].next)
    {
        edits_count += text_items(args->json, change);
    }
    // a cell is taken for one of each where the page sent none: a taken cell is never of no bytes
    CliEdit *const edit_of =
        g_cli_cells->temporary_buf_alloc((edits_count + 1ull) * sizeof(*edit_of), &g_cli_window_text_cells);
    const CliEdit **const change_of =
        g_cli_cells->temporary_buf_alloc((changes_count + 1ull) * sizeof(*change_of), &g_cli_window_text_cells);
    unsigned int *const edits_of =
        g_cli_cells->temporary_buf_alloc((changes_count + 1ull) * sizeof(*edits_of), &g_cli_window_text_cells);
    int edited = 0;
    if ((edit_of == NULL) || (change_of == NULL) || (edits_of == NULL))
    {
        mmgr_line_put(err_msg, "the changes of a writing could not be given cells");
    }
    else
    {
        unsigned int count = 0u;
        unsigned int used = 0u;
        for (unsigned int change = args->json->node[changes].first; change != CLI_JSON_NONE;
             change = args->json->node[change].next)
        {
            change_of[count] = &edit_of[used];
            edits_of[count] = 0u;
            const unsigned int first =
                cli_json_is(args->json, change, CFG_JSON_ARRAY) ? args->json->node[change].first : CLI_JSON_NONE;
            for (unsigned int edit = first; edit != CLI_JSON_NONE; edit = args->json->node[edit].next)
            {
                unsigned long long length = 0ull;
                const char *const text =
                    cli_json_string(args->json, cli_json_member(args->json, edit, "text"), &length);
                edit_of[used].from = text_place(args->json, edit, "from");
                edit_of[used].to = text_place(args->json, edit, "to");
                edit_of[used].text = (text != NULL) ? text : "";
                edit_of[used].length = (text != NULL) ? length : 0ull;
                used += 1u;
                edits_of[count] += 1u;
            }
            count += 1u;
        }
        CliShape shape;
        edited = cli_document_edit(key, change_of, edits_of, count, &shape, err_msg);
        if (edited)
        {
            text_shape_written(&shape, out);
        }
    }
    g_cli_cells->persistent_buf_release(edits_of, &g_cli_window_text_cells);
    g_cli_cells->persistent_buf_release(change_of, &g_cli_window_text_cells);
    g_cli_cells->persistent_buf_release(edit_of, &g_cli_window_text_cells);
    return edited;
}

// the text stored under `key` let go
static int text_document_release(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)out;
    const char *const key = cli_serve_string(args, "key", err_msg);
    if (key == NULL)
    {
        return 0;
    }
    cli_document_release(key);
    return 1;
}

// the gutter's marks for the text stored under `key` against the file at `path` as the last commit stores it, read from
// the tree where it is not stored beside the text yet or `fresh` requests; null where no text is stored under `key`
static int text_document_changes(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const key = cli_serve_string(args, "key", err_msg);
    const char *const path = (key != NULL) ? cli_serve_string(args, "path", err_msg) : NULL;
    int fresh = 0;
    if ((path == NULL) || !cli_serve_bool(args, "fresh", &fresh, err_msg))
    {
        return 0;
    }
    const unsigned long long start = out->at;
    if (!fresh && cli_document_has_head(key))
    {
        if (!cli_document_marks(key, 0, NULL, 0ull, out))
        {
            out->at = start;
            mmgr_line_put(out, "null");
        }
        return 1;
    }
    CliJson json;
    const unsigned int head = cli_window_tree_called(&json, "file_head", path, "", err_msg);
    unsigned long long length = 0ull;
    const char *const text = (head != CLI_JSON_NONE) ? cli_json_string(&json, head, &length) : NULL;
    const int read = (head != CLI_JSON_NONE);
    if (read && !cli_document_marks(key, 1, text, length, out))
    {
        out->at = start;
        mmgr_line_put(out, "null");
    }
    cli_json_given(&json);
    return read;
}

// find's matches in the text stored under `key`, each [line, from col, to col], at most `most`
static int text_document_find(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const key = cli_serve_string(args, "key", err_msg);
    const unsigned int sought = cli_serve_arg(args, "sought");
    unsigned long long most = 0ull;
    if ((key == NULL) || !cli_serve_whole(args, "most", &most, err_msg))
    {
        return 0;
    }
    const char *const query = cli_json_string(args->json, cli_json_member(args->json, sought, "query"), NULL);
    if (query == NULL)
    {
        cli_serve_refused(args, sought, "sought", "struct Sought", err_msg);
        return 0;
    }
    const CliDocSought read = {query,
                               cli_json_is(args->json, cli_json_member(args->json, sought, "regex"), CFG_JSON_TRUE),
                               cli_json_is(args->json, cli_json_member(args->json, sought, "case"), CFG_JSON_TRUE),
                               cli_json_is(args->json, cli_json_member(args->json, sought, "word"), CFG_JSON_TRUE)};
    return cli_document_find(key, &read, most, out, err_msg);
}

// two texts compared by structure, the older first, or null where they are too far apart
static int text_structure_compare(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const then = cli_serve_string_or(args, "then", "");
    const char *const now = cli_serve_string(args, "now", err_msg);
    if (now == NULL)
    {
        return 0;
    }
    cli_structure_compare(then, cellul.len(&(CatenaFinitaCfg){.src = then, .cap = CLI_CELLS_POOL}), now,
                          cellul.len(&(CatenaFinitaCfg){.src = now, .cap = CLI_CELLS_POOL}), out);
    return 1;
}

// the text of the file at `path`, anywhere on the machine, as the reader chose it
static int text_file_read_any(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const path = cli_serve_string(args, "path", err_msg);
    if (path == NULL)
    {
        return 0;
    }
    unsigned long long size = 0ull;
    const unsigned char *const mapped = cli_file_read(path, &size);
    const int read = (mapped != NULL) && cli_utf8_whole((const char *)mapped, size);
    if (read)
    {
        cli_json_quoted(out, (const char *)mapped, size);
    }
    cli_file_let_go(mapped, size);
    if (!read)
    {
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, ": it could not be read");
    }
    return read;
}

// `text` written to the file at `path`, anywhere on the machine, the folders it stands in made where they are not
static int text_file_write_any(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)out;
    const char *const path = cli_serve_string(args, "path", err_msg);
    unsigned long long length = 0ull;
    const char *const text = (path != NULL) ? cli_json_string(args->json, cli_serve_arg(args, "text"), &length) : NULL;
    if ((text == NULL) && (path != NULL))
    {
        (void)cli_serve_string(args, "text", err_msg);
    }
    if (text == NULL)
    {
        return 0;
    }
    char folder[CLI_PATH_MAX_LEN];
    if (cli_path_parent(path, folder) && (folder[0] != '\0') && !query_folder_made(folder))
    {
        mmgr_line_put(err_msg, folder);
        mmgr_line_put(err_msg, ": it could not be made");
        return 0;
    }
    if (!query_file_written(path, text, length))
    {
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, ": it could not be written");
        return 0;
    }
    return 1;
}

// the folders a .code-workspace file names, each made whole against the file's own folder, as JSON with comments and
// trailing commas reads them
static int text_workspace_read(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const path = cli_serve_string(args, "path", err_msg);
    if (path == NULL)
    {
        return 0;
    }
    unsigned long long size = 0ull;
    const unsigned char *const mapped = cli_file_read(path, &size);
    // a text made plain is never longer than the text, as it only leaves comments and commas out
    char *const plain_buf =
        (mapped != NULL) ? g_cli_cells->temporary_buf_alloc(size + 1ull, &g_cli_window_text_cells) : NULL;
    VerbaLinea plain = {plain_buf, (plain_buf != NULL) ? (size + 1ull) : 0ull, 0ull};
    if (plain_buf != NULL)
    {
        cli_plain_json((const char *)mapped, size, &plain);
    }
    cli_file_let_go(mapped, size);
    if (plain_buf == NULL)
    {
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, ": it could not be read");
        return 0;
    }
    CliJson json;
    cli_json_begun(&json, NULL, 0u, NULL, 0ull);
    char why[1024];
    VerbaLinea unread = {why, sizeof(why), 0ull};
    const unsigned int value = cli_json_taken(&json, plain_buf, mmgr_line_ok(&plain) ? plain.at : 0ull, 0u, 0ull,
                                              &unread, &g_cli_window_text_cells);
    g_cli_cells->persistent_buf_release(plain_buf, &g_cli_window_text_cells);
    if (value == CLI_JSON_NONE)
    {
        cli_json_given(&json);
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, ": ");
        mmgr_line_put_n(err_msg, why, mmgr_line_ok(&unread) ? unread.at : 0ull);
        return 0;
    }
    char base[CLI_PATH_MAX_LEN] = "";
    (void)cli_path_parent(path, base);
    const unsigned int folders = cli_json_member(&json, value, "folders");
    const unsigned int first = cli_json_is(&json, folders, CFG_JSON_ARRAY) ? json.node[folders].first : CLI_JSON_NONE;
    mmgr_line_ch(out, '[');
    int any = 0;
    for (unsigned int folder = first; folder != CLI_JSON_NONE; folder = json.node[folder].next)
    {
        const char *const named = cli_json_string(&json, cli_json_member(&json, folder, "path"), NULL);
        if (named == NULL)
        {
            continue;
        }
        const int whole = (named[0] == '/') || (named[0] == '\\') ||
                          ((named[0] != '\0') && (named[1] == ':') && ((named[2] == '/') || (named[2] == '\\')));
        char joined[CLI_PATH_MAX_LEN];
        char full[CLI_PATH_MAX_LEN];
        if (whole)
        {
            memor.cpy(&(MemoriaCfg){.dst = joined,
                                    .src = named,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}) + 1u});
        }
        else
        {
            cli_full(base, named, joined);
        }
        mmgr_line_put(out, any ? "," : "");
        cli_window_quoted(out, query_path_full(joined, full, sizeof(full)) ? full : joined);
        any = 1;
    }
    mmgr_line_ch(out, ']');
    cli_json_given(&json);
    return 1;
}

const CliServeCall g_cli_window_text_calls[] = {
    {"document_edit", text_document_edit},         {"document_release", text_document_release},
    {"document_changes", text_document_changes},   {"document_find", text_document_find},
    {"structure_compare", text_structure_compare}, {"file_read_any", text_file_read_any},
    {"file_write_any", text_file_write_any},       {"workspace_read", text_workspace_read},
};
const unsigned int g_cli_window_text_count = sizeof(g_cli_window_text_calls) / sizeof(g_cli_window_text_calls[0]);
