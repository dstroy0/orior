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

// the bytes a text written over is begun in past the text it is written over, and the bytes the shapes found, the
// files a search edits and one file's edits are begun in, each before it grows
#define CLI_SERVE_WRITTEN_MORE (4ull << 10u)
#define CLI_SERVE_FOUND_FIRST (64ull << 10u)

// the cells a call writing over files or searching them holds while it answers, each sized to what it holds
static CliCellsTally s_cells = CLI_CELLS_TALLY("cli_serve_shape");

// the edits of a file as the page gives them, `[{"from", "to", "text"}]`, read into `edit_of`, a cell of their count
// taken here and given back with cli_cell_given_by(&s_cells, ...) either way: their count, or CLI_SERVE_EDITS_REFUSED
// with why said where one is no edit or the cell is refused
static unsigned int lsp_edits_of(const CliJson *json, unsigned int edits, CliTextEdit **edit_of, CliLine *said)
{
    const unsigned int first = cli_json_is(json, edits, CFG_JSON_ARRAY) ? json->node[edits].first : CLI_JSON_NONE;
    unsigned int most = 0u;
    for (unsigned int one = first; one != CLI_JSON_NONE; one = json->node[one].next)
    {
        most += 1u;
    }
    *edit_of = cli_cell_taken_by(&s_cells, (most + 1ull) * sizeof(**edit_of), "a file's edits");
    if (*edit_of == NULL)
    {
        cli_line_text(said, "a file's edits could not be given a cell");
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
        if (!cli_serve_place(&edit, "from", &from, said) || !cli_serve_place(&edit, "to", &to, said) || (text == NULL))
        {
            cli_line_text(said, (text == NULL) ? "files: missing field `text`" : "");
            return CLI_SERVE_EDITS_REFUSED;
        }
        (*edit_of)[count] = (CliTextEdit){from, to, text, size};
        count += 1u;
    }
    return count;
}

static int edits_write(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    if (!cli_serve_tree(root, said))
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
        for (const char *part = first; !outside && (*part != '\0'); part += 1)
        {
            outside = (part == first || part[-1] == '/') &&
                      (cellul.starts(&(CatenaFinitaCfg){.src = part, .other = "..", .cap = CLI_CELLS_POOL})) &&
                      ((part[2] == '/') || (part[2] == '\0'));
        }
        if ((path == NULL) || outside)
        {
            cli_line_text(said, first);
            cli_line_text(said, " is outside the tree and was not changed");
            done = 0;
            continue;
        }
        char full[CLI_PATH_LONGEST];
        cli_full(root, path, full);
        unsigned long long size = 0ull;
        const unsigned char *const mapped = cli_file_held(full, &size);
        const char *const was = (const char *)mapped;
        CliTextEdit *edit = NULL;
        const unsigned int count = lsp_edits_of(json, cli_json_member(json, file, "edits"), &edit, said);
        CliLine text;
        const int begun = cli_line_growing(&text, size + CLI_SERVE_WRITTEN_MORE, "a file's text written over");
        const int read = (mapped != NULL) && cli_utf8_whole(was, size);
        if (!read)
        {
            cli_line_text(said, path);
            cli_line_text(said, ": it could not be read");
            done = 0;
        }
        else if ((count == CLI_SERVE_EDITS_REFUSED) || !begun || !cli_text_applied(was, size, edit, count, &text) ||
                 !cli_line_fits(&text))
        {
            done = 0;
        }
        // the file is let go before it is written over, as the write puts a new file in its place
        cli_file_let_go(mapped, size);
        if (done && !query_file_written(full, text.out, text.at))
        {
            done = 0;
        }
        if (!done && read && (count != CLI_SERVE_EDITS_REFUSED))
        {
            cli_line_text(said, path);
            cli_line_text(said, ": it could not be written");
        }
        cli_cell_given(text.out);
        cli_cell_given_by(&s_cells, edit);
        written += done ? 1ull : 0ull;
    }
    if (done)
    {
        cli_line_decimal(out, written, 1u);
    }
    return done;
}

// A search of shapes as it is written: the file, the matches written, the edits written, and whether any is
typedef struct
{
    const char *file;
    CliLine *found;
    CliLine *edits;
    int any;
} CliServeShapes;

// a match written among the found, and its edit among the file's where a template wrote it
static void lsp_shape_found(void *context, const CliShapeFound *found)
{
    CliServeShapes *const shapes = context;
    cli_line_text(shapes->found, (shapes->found->at > 1ull) ? "," : "");
    cli_shape_found_written(shapes->file, found, shapes->found);
    if (found->has_with)
    {
        const CliTextEdit edit = {found->from, found->to, found->with, found->with_size};
        cli_line_text(shapes->edits, shapes->any ? "," : "");
        cli_text_edit_written(&edit, shapes->edits);
        shapes->any = 1;
    }
}

static int shape_search(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    const char *const language = cli_serve_string(args, "language", said);
    const char *const pattern = (language != NULL) ? cli_serve_string(args, "pattern", said) : NULL;
    if ((pattern == NULL) || !cli_serve_tree(root, said))
    {
        return 0;
    }
    const char *const template_text = cli_serve_string_or(args, "template", NULL);
    const char *const one = cli_serve_string_or(args, "path", NULL);
    CliLine found = {NULL, 0ull, 0ull};
    CliLine files = {NULL, 0ull, 0ull};
    CliLine edits = {NULL, 0ull, 0ull};
    const int begun = cli_line_growing(&found, CLI_SERVE_FOUND_FIRST, "the shapes found") &&
                      cli_line_growing(&files, CLI_SERVE_FOUND_FIRST, "the files a search edits") &&
                      cli_line_growing(&edits, CLI_SERVE_FOUND_FIRST, "one file's edits");
    CliLanguages *const languages =
        (begun && (one == NULL)) ? cli_cell_taken_by(&s_cells, sizeof(*languages), "the languages a search knows")
                                 : NULL;
    if (!begun || ((one == NULL) && (languages == NULL)))
    {
        cli_cell_given(edits.out);
        cli_cell_given(files.out);
        cli_cell_given(found.out);
        cli_line_text(said, "the search's cells could not be taken");
        return 0;
    }
    CliServeFiles *const listed = (one == NULL) ? cli_serve_files_listed(root, said) : NULL;
    if ((one == NULL) && (listed == NULL))
    {
        cli_cell_given_by(&s_cells, languages);
        cli_cell_given(edits.out);
        cli_cell_given(files.out);
        cli_cell_given(found.out);
        return 0;
    }
    if (one == NULL)
    {
        cli_plugins_languages(languages);
    }
    cli_line_character(&found, '[');
    cli_line_character(&files, '[');
    const unsigned int count = (one != NULL) ? 1u : listed->count;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const file = (one != NULL) ? one : listed->paths[at];
        char id[CLI_NAME_LONGEST];
        if ((one == NULL) && (!cli_plugins_language_for(languages, file, id) ||
                              (!cellul.eq(&(CatenaFinitaCfg){.src = id, .other = language, .cap = CLI_CELLS_POOL}))))
        {
            continue;
        }
        char full[CLI_PATH_LONGEST];
        cli_full(root, file, full);
        // the text as the editor holds it, a cell of its size, else the file as the disk holds it, mapped
        unsigned long long size = 0ull;
        char *const kept = cli_servers_text_taken(full, &size);
        const unsigned char *const mapped = (kept == NULL) ? cli_file_held(full, &size) : NULL;
        const char *const text = (kept != NULL) ? kept : (const char *)mapped;
        if ((text != NULL) && ((kept != NULL) || cli_utf8_whole(text, size)))
        {
            edits.at = 0ull;
            CliServeShapes shapes = {file, &found, &edits, 0};
            cli_shape_search(language, text, size, pattern, template_text, lsp_shape_found, &shapes);
            if (shapes.any && cli_line_fits(&edits))
            {
                cli_line_text(&files, (files.at > 1ull) ? ",{\"path\":" : "{\"path\":");
                cli_json_quoted_lossy(&files, (const unsigned char *)file,
                                      cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}));
                cli_line_text(&files, ",\"edits\":[");
                cli_line_counted(&files, edits.out, edits.at);
                cli_line_text(&files, "]}");
            }
        }
        cli_file_let_go(mapped, size);
        cli_cell_given(kept);
    }
    cli_line_character(&found, ']');
    cli_line_character(&files, ']');
    cli_line_text(out, "{\"found\":");
    cli_line_counted(out, found.out, found.at);
    cli_line_text(out, ",\"files\":");
    cli_line_counted(out, files.out, files.at);
    cli_line_character(out, '}');
    cli_serve_files_given(listed);
    if (languages != NULL)
    {
        cli_plugins_languages_given(languages);
    }
    cli_cell_given_by(&s_cells, languages);
    cli_cell_given(edits.out);
    cli_cell_given(files.out);
    cli_cell_given(found.out);
    return 1;
}

const CliServeCall g_cli_serve_shape_calls[] = {
    {"edits_write", edits_write},
    {"shape_search", shape_search},
};
const unsigned int g_cli_serve_shape_count = sizeof(g_cli_serve_shape_calls) / sizeof(g_cli_serve_shape_calls[0]);
