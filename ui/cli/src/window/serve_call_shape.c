// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_shape.c: the calls that write over files the editor does not have open: edits written to each file
// under the tree, a file outside it left as it is and named; and Search Structurally over a file or every file of a
// language, each match found and, where a template is given, each file's edits
#include "../code/shape.h"
#include "../command_line/cells.h"
#include "../editor/files.h"
#include "../plugins/plugins.h"
#include "serve_internal.h"

// the count of a file's edits where one is no edit or their cell is refused
#define CLI_SERVE_EDITS_REFUSED 0xFFFFFFFFu

// the bytes a text written over is written in past the text it is written over, and the bytes the shapes found, the
// files a search edits and one file's edits are written in, each
#define CLI_SERVE_WRITTEN_MORE (4ull << 10u)
#define CLI_SERVE_FOUND_ROOM (8ull << 20u)

// the edits of a file as the page gives them, `[{"from", "to", "text"}]`, read into `edit_of`, a cell of their count
// taken here and given back with g_cli_cells->persistent_buf_release(..., owner) either way: their count, or
// CLI_SERVE_EDITS_REFUSED with why given where one is no edit or the cell is refused
static unsigned int lsp_edits_of(const CliJson *json, unsigned int edits, CliTextEdit **edit_of, VerbaLinea *err_msg)
{
    const unsigned int first = cli_json_is(json, edits, CFG_JSON_ARRAY) ? json->node[edits].first : CLI_JSON_NONE;
    unsigned int most = 0u;
    for (unsigned int one = first; one != CLI_JSON_NONE; one = json->node[one].next)
    {
        most += 1u;
    }
    *edit_of = g_cli_cells->persistent_buf_alloc((most + 1ull) * sizeof(**edit_of), &g_cli_serve_shape_cells);
    if (*edit_of == NULL)
    {
        mmgr_line_put(err_msg, "a file's edits could not be given a cell");
        return CLI_SERVE_EDITS_REFUSED;
    }
    unsigned int count = 0u;
    for (unsigned int one = first; one != CLI_JSON_NONE; one = json->node[one].next)
    {
        const CliServeArgs edit = {json, one};
        CliTextPlace from = {0u, 0u};
        CliTextPlace to = {0u, 0u};
        unsigned long long size = 0ull;
        const char *const text = cli_json_string(json, cli_json_member(json, one, "text"), &size);
        if (!cli_serve_place(&edit, "from", &from, err_msg) || !cli_serve_place(&edit, "to", &to, err_msg) ||
            (text == NULL))
        {
            mmgr_line_put(err_msg, (text == NULL) ? "files: missing field `text`" : "");
            return CLI_SERVE_EDITS_REFUSED;
        }
        (*edit_of)[count] = (CliTextEdit){from, to, text, size};
        count += 1u;
    }
    return count;
}

static int edits_write(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    if (!cli_serve_tree(root, err_msg))
    {
        return 0;
    }
    const unsigned int files = cli_serve_arg(args, "files");
    const CliJson *const json = args->json;
    unsigned long long written = 0ull;
    int done = 1;
    for (unsigned int file = cli_json_is(json, files, CFG_JSON_ARRAY) ? json->node[files].first : CLI_JSON_NONE;
         done && (file != CLI_JSON_NONE); file = json->node[file].next)
    {
        const char *const path = cli_json_string(json, cli_json_member(json, file, "path"), NULL);
        const char *const first = (path != NULL) ? path : "";
        int outside = (first[0] == '/') || (first[0] == '\\') || ((first[0] != '\0') && (first[1] == ':'));
        for (const char *part = cellul.find(
                 &(CatenaFinitaCfg){.src = first, .cap = CLI_CELLS_POOL, .other = "..", .other_cap = CLI_CELLS_POOL});
             !outside && (part != NULL);
             part = cellul.find(&(CatenaFinitaCfg){
                 .src = part + 1, .cap = CLI_CELLS_POOL, .other = "..", .other_cap = CLI_CELLS_POOL}))
        {
            outside = ((part == first) || (part[-1] == '/')) && ((part[2] == '/') || (part[2] == '\0'));
        }
        if ((path == NULL) || outside)
        {
            mmgr_line_put(err_msg, first);
            mmgr_line_put(err_msg, " is outside the tree and was not changed");
            done = 0;
            continue;
        }
        char full[CLI_PATH_MAX_LEN];
        cli_full(root, path, full);
        unsigned long long size = 0ull;
        const unsigned char *const mapped = cli_file_taken(full, &size, &g_cli_serve_shape_cells);
        const char *const was = (const char *)mapped;
        CliTextEdit *edit = NULL;
        const unsigned int count = lsp_edits_of(json, cli_json_member(json, file, "edits"), &edit, err_msg);
        const unsigned long long text_room = size + CLI_SERVE_WRITTEN_MORE;
        char *const text_cell = g_cli_cells->temporary_buf_alloc(text_room, &g_cli_serve_shape_cells);
        VerbaLinea text = mmgr_line_over(text_cell, text_room);
        const int begun = text.out != NULL;
        const int read = (mapped != NULL) && cli_utf8_whole(was, size);
        if (!read)
        {
            mmgr_line_put(err_msg, path);
            mmgr_line_put(err_msg, ": it could not be read");
            done = 0;
        }
        else if ((count == CLI_SERVE_EDITS_REFUSED) || !begun || !cli_text_applied(was, size, edit, count, &text) ||
                 !mmgr_line_ok(&text))
        {
            done = 0;
        }
        // the file is let go before it is written over, as the write puts a new file in its place
        cli_file_given(mapped, &g_cli_serve_shape_cells);
        if (done && !query_file_written(full, text.out, text.at))
        {
            done = 0;
        }
        if (!done && read && (count != CLI_SERVE_EDITS_REFUSED))
        {
            mmgr_line_put(err_msg, path);
            mmgr_line_put(err_msg, ": it could not be written");
        }
        g_cli_cells->persistent_buf_release(text.out, &g_cli_serve_shape_cells);
        g_cli_cells->persistent_buf_release(edit, &g_cli_serve_shape_cells);
        written += done ? 1ull : 0ull;
    }
    if (done)
    {
        mmgr_line_decimal(out, written, 1u);
    }
    return done;
}

// A search of shapes as it is written: the file, the matches written, the edits written, and whether any is
typedef struct
{
    const char *file;
    VerbaLinea *found;
    VerbaLinea *edits;
    int any;
} CliServeShapes;

// a match written among the found, and its edit among the file's where a template wrote it
static void lsp_shape_found(void *context, const CliShapeFound *found)
{
    CliServeShapes *const shapes = context;
    mmgr_line_put(shapes->found, (shapes->found->at > 1ull) ? "," : "");
    cli_shape_found_written(shapes->file, found, shapes->found);
    if (found->has_with)
    {
        const CliTextEdit edit = {found->from, found->to, found->with, found->with_size};
        mmgr_line_put(shapes->edits, shapes->any ? "," : "");
        cli_text_edit_written(&edit, shapes->edits);
        shapes->any = 1;
    }
}

static int shape_search(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    const char *const language = cli_serve_string(args, "language", err_msg);
    const char *const pattern = (language != NULL) ? cli_serve_string(args, "pattern", err_msg) : NULL;
    if ((pattern == NULL) || !cli_serve_tree(root, err_msg))
    {
        return 0;
    }
    const char *const template_text = cli_serve_string_or(args, "template", NULL);
    const char *const one = cli_serve_string_or(args, "path", NULL);
    char *const found_cell = g_cli_cells->temporary_buf_alloc(CLI_SERVE_FOUND_ROOM, &g_cli_serve_shape_cells);
    char *const files_cell = g_cli_cells->temporary_buf_alloc(CLI_SERVE_FOUND_ROOM, &g_cli_serve_shape_cells);
    char *const edits_cell = g_cli_cells->temporary_buf_alloc(CLI_SERVE_FOUND_ROOM, &g_cli_serve_shape_cells);
    VerbaLinea found = mmgr_line_over(found_cell, CLI_SERVE_FOUND_ROOM);
    VerbaLinea files = mmgr_line_over(files_cell, CLI_SERVE_FOUND_ROOM);
    VerbaLinea edits = mmgr_line_over(edits_cell, CLI_SERVE_FOUND_ROOM);
    const int begun = (found_cell != NULL) && (files_cell != NULL) && (edits_cell != NULL);
    CliLanguages *const languages = (begun && (one == NULL))
                                        ? g_cli_cells->temporary_buf_alloc(sizeof(*languages), &g_cli_serve_shape_cells)
                                        : NULL;
    if (!begun || ((one == NULL) && (languages == NULL)))
    {
        g_cli_cells->persistent_buf_release(edits.out, &g_cli_serve_shape_cells);
        g_cli_cells->persistent_buf_release(files.out, &g_cli_serve_shape_cells);
        g_cli_cells->persistent_buf_release(found.out, &g_cli_serve_shape_cells);
        mmgr_line_put(err_msg, "the search's cells could not be taken");
        return 0;
    }
    CliServeFiles *const listed = (one == NULL) ? cli_serve_files_listed(root, err_msg) : NULL;
    if ((one == NULL) && (listed == NULL))
    {
        g_cli_cells->persistent_buf_release(languages, &g_cli_serve_shape_cells);
        g_cli_cells->persistent_buf_release(edits.out, &g_cli_serve_shape_cells);
        g_cli_cells->persistent_buf_release(files.out, &g_cli_serve_shape_cells);
        g_cli_cells->persistent_buf_release(found.out, &g_cli_serve_shape_cells);
        return 0;
    }
    if (one == NULL)
    {
        cli_plugins_languages(languages);
    }
    // the languages the pattern and each file are read in, read once for the search
    CliSymbolsLanguages *const read = cli_symbols_languages_read();
    mmgr_line_ch(&found, '[');
    mmgr_line_ch(&files, '[');
    const unsigned int count = (one != NULL) ? 1u : listed->count;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const file = (one != NULL) ? one : listed->paths[at];
        char id[CLI_NAME_MAX_LEN];
        if ((one == NULL) && (!cli_plugins_language_for(languages, file, id) ||
                              (!cellul.eq(&(CatenaFinitaCfg){.src = id, .other = language, .cap = CLI_CELLS_POOL}))))
        {
            continue;
        }
        char full[CLI_PATH_MAX_LEN];
        cli_full(root, file, full);
        // the text as the editor has it, a cell of its size, else the file as the disk stores it, mapped
        unsigned long long size = 0ull;
        char *const editor_text = cli_servers_text_taken(full, &size);
        const unsigned char *const mapped =
            (editor_text == NULL) ? cli_file_taken(full, &size, &g_cli_serve_shape_cells) : NULL;
        const char *const text = (editor_text != NULL) ? editor_text : (const char *)mapped;
        if ((text != NULL) && ((editor_text != NULL) || cli_utf8_whole(text, size)))
        {
            edits.at = 0ull;
            CliServeShapes shapes = {file, &found, &edits, 0};
            cli_shape_search(read, language, text, size, pattern, template_text, lsp_shape_found, &shapes);
            if (shapes.any && mmgr_line_ok(&edits))
            {
                mmgr_line_put(&files, (files.at > 1ull) ? ",{\"path\":" : "{\"path\":");
                cli_json_quoted_lossy(&files, (const unsigned char *)file,
                                      cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}));
                mmgr_line_put(&files, ",\"edits\":[");
                mmgr_line_put_n(&files, edits.out, edits.at);
                mmgr_line_put(&files, "]}");
            }
        }
        cli_file_given(mapped, &g_cli_serve_shape_cells);
        g_cli_cells->persistent_buf_release(editor_text, &g_cli_serve_shape_cells);
    }
    mmgr_line_ch(&found, ']');
    mmgr_line_ch(&files, ']');
    mmgr_line_put(out, "{\"found\":");
    mmgr_line_put_n(out, found.out, found.at);
    mmgr_line_put(out, ",\"files\":");
    mmgr_line_put_n(out, files.out, files.at);
    mmgr_line_ch(out, '}');
    cli_symbols_languages_given(read);
    cli_serve_files_given(listed);
    if (languages != NULL)
    {
        cli_plugins_languages_given(languages);
    }
    g_cli_cells->persistent_buf_release(languages, &g_cli_serve_shape_cells);
    g_cli_cells->persistent_buf_release(edits.out, &g_cli_serve_shape_cells);
    g_cli_cells->persistent_buf_release(files.out, &g_cli_serve_shape_cells);
    g_cli_cells->persistent_buf_release(found.out, &g_cli_serve_shape_cells);
    return 1;
}

const CliServeCall g_cli_serve_shape_calls[] = {
    {"edits_write", edits_write},
    {"shape_search", shape_search},
};
const unsigned int g_cli_serve_shape_count = sizeof(g_cli_serve_shape_calls) / sizeof(g_cli_serve_shape_calls[0]);
