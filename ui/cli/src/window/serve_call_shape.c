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

// the most edits written to one file, and the most bytes a file's text takes
#define CLI_SERVE_EDITS 65536u
#define CLI_SERVE_TEXT (32ull << 20u)

// the most bytes of a search's edits for one file
#define CLI_SERVE_FILE_EDITS (1u << 22u)

// a file's text and what is written over it, cells of CLI_SERVE_TEXT, and the edits of one file, a cell of
// CLI_SERVE_EDITS of them, each taken with their lock and given back before it is
static char *s_text;
static char *s_written;
static CliTextEdit *s_edit;
static CliLock s_text_lock = CLI_LOCK_INIT;

// the cells given back, and the lock with them
static void shape_released(void)
{
    cli_cell_given(s_edit);
    cli_cell_given(s_written);
    cli_cell_given(s_text);
    s_edit = NULL;
    s_written = NULL;
    s_text = NULL;
    cli_lock_give(&s_text_lock);
}

// the lock taken and the text's cells with it: 1, or 0 with why said and the lock given back where the cells cannot
// meet them
static int shape_held(CliLine *said)
{
    cli_lock_take(&s_text_lock);
    s_text = cli_cell_taken(CLI_SERVE_TEXT, "a file's text");
    s_written = cli_cell_taken(CLI_SERVE_TEXT, "a file's text written over");
    s_edit = cli_cell_taken(sizeof(CliTextEdit) * CLI_SERVE_EDITS, "a file's edits");
    if ((s_text == NULL) || (s_written == NULL) || (s_edit == NULL))
    {
        shape_released();
        cli_line_text(said, "the file's cells could not be taken");
        return 0;
    }
    return 1;
}

// the edits of a file as the page gives them, `[{"from", "to", "text"}]`, read into the edits: their count, or
// CLI_SERVE_EDITS with why said where one is no edit
static unsigned int lsp_edits_of(const CliJson *json, unsigned int edits, CliLine *said)
{
    unsigned int count = 0u;
    for (unsigned int one = cli_json_is(json, edits, CFG_JSON_ARRAY) ? json->node[edits].first : CLI_JSON_NONE;
         (one != CLI_JSON_NONE) && (count < CLI_SERVE_EDITS); one = json->node[one].next)
    {
        const CliServeArgs edit = {json, one};
        CliTextPlace from = {0u, 0u};
        CliTextPlace to = {0u, 0u};
        unsigned long long size = 0ull;
        const char *const text = cli_json_string(json, cli_json_member(json, one, "text"), &size);
        if (!cli_serve_place(&edit, "from", &from, said) || !cli_serve_place(&edit, "to", &to, said) || (text == NULL))
        {
            cli_line_text(said, (text == NULL) ? "files: missing field `text`" : "");
            return CLI_SERVE_EDITS;
        }
        s_edit[count] = (CliTextEdit){from, to, text, size};
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
    if (!shape_held(said))
    {
        return 0;
    }
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
        const unsigned long long size = qry_member_file_read(full, (unsigned char *)s_text, CLI_SERVE_TEXT);
        const unsigned int count = lsp_edits_of(json, cli_json_member(json, file, "edits"), said);
        CliLine text = {s_written, CLI_SERVE_TEXT, 0ull};
        if ((size == QRY_MEMBER_UNREAD) || !cli_utf8_whole(s_text, size))
        {
            cli_line_text(said, path);
            cli_line_text(said, ": it could not be read");
            done = 0;
        }
        else if ((count == CLI_SERVE_EDITS) || !cli_text_applied(s_text, size, s_edit, count, &text) ||
                 !cli_line_fits(&text) || !query_file_written(full, s_written, text.at))
        {
            if (count != CLI_SERVE_EDITS)
            {
                cli_line_text(said, path);
                cli_line_text(said, ": it could not be written");
            }
            done = 0;
        }
        written += done ? 1ull : 0ull;
    }
    shape_released();
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
    static CliLanguages s_languages;
    char root[CLI_PATH_LONGEST];
    const char *const language = cli_serve_string(args, "language", said);
    const char *const pattern = (language != NULL) ? cli_serve_string(args, "pattern", said) : NULL;
    if ((pattern == NULL) || !cli_serve_tree(root, said))
    {
        return 0;
    }
    const char *const template_text = cli_serve_string_or(args, "template", NULL);
    const char *const one = cli_serve_string_or(args, "path", NULL);
    if (!shape_held(said))
    {
        return 0;
    }
    char *const found_text = cli_cell_taken(CLI_SERVE_TEXT, "the shapes found");
    char *const files_text = cli_cell_taken(CLI_SERVE_TEXT, "the files a search edits");
    char *const edits_text = cli_cell_taken(CLI_SERVE_FILE_EDITS, "one file's edits");
    if ((found_text == NULL) || (files_text == NULL) || (edits_text == NULL))
    {
        cli_cell_given(edits_text);
        cli_cell_given(files_text);
        cli_cell_given(found_text);
        shape_released();
        cli_line_text(said, "the search's cells could not be taken");
        return 0;
    }
    CliServeFiles *const listed = (one == NULL) ? cli_serve_files_listed(root, said) : NULL;
    if ((one == NULL) && (listed == NULL))
    {
        cli_cell_given(edits_text);
        cli_cell_given(files_text);
        cli_cell_given(found_text);
        shape_released();
        return 0;
    }
    if (one == NULL)
    {
        cli_plugins_languages(&s_languages);
    }
    CliLine found = {found_text, CLI_SERVE_TEXT, 0ull};
    CliLine files = {files_text, CLI_SERVE_TEXT, 0ull};
    cli_line_character(&found, '[');
    cli_line_character(&files, '[');
    const unsigned int count = (one != NULL) ? 1u : listed->count;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const file = (one != NULL) ? one : listed->paths[at];
        char id[CLI_NAME_LONGEST];
        if ((one == NULL) && (!cli_plugins_language_for(&s_languages, file, id) ||
                              (!cellul.eq(&(CatenaFinitaCfg){.src = id, .other = language, .cap = CLI_CELLS_POOL}))))
        {
            continue;
        }
        char full[CLI_PATH_LONGEST];
        cli_full(root, file, full);
        CliLine held = {s_text, CLI_SERVE_TEXT, 0ull};
        unsigned long long size = 0ull;
        if (cli_servers_text_of(full, &held) && cli_line_fits(&held))
        {
            size = held.at;
        }
        else
        {
            size = qry_member_file_read(full, (unsigned char *)s_text, CLI_SERVE_TEXT);
            if ((size == QRY_MEMBER_UNREAD) || !cli_utf8_whole(s_text, size))
            {
                continue;
            }
        }
        CliLine edits = {edits_text, CLI_SERVE_FILE_EDITS, 0ull};
        CliServeShapes shapes = {file, &found, &edits, 0};
        cli_shape_search(language, s_text, size, pattern, template_text, lsp_shape_found, &shapes);
        if (shapes.any && cli_line_fits(&edits))
        {
            cli_line_text(&files, (files.at > 1ull) ? ",{\"path\":" : "{\"path\":");
            cli_json_quoted_lossy(&files, (const unsigned char *)file,
                                  cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}));
            cli_line_text(&files, ",\"edits\":[");
            cli_line_counted(&files, edits_text, edits.at);
            cli_line_text(&files, "]}");
        }
    }
    cli_line_character(&found, ']');
    cli_line_character(&files, ']');
    cli_line_text(out, "{\"found\":");
    cli_line_counted(out, found_text, found.at);
    cli_line_text(out, ",\"files\":");
    cli_line_counted(out, files_text, files.at);
    cli_line_character(out, '}');
    cli_serve_files_given(listed);
    cli_cell_given(edits_text);
    cli_cell_given(files_text);
    cli_cell_given(found_text);
    shape_released();
    return 1;
}

const CliServeCall g_cli_serve_shape_calls[] = {
    {"edits_write", edits_write},
    {"shape_search", shape_search},
};
const unsigned int g_cli_serve_shape_count = sizeof(g_cli_serve_shape_calls) / sizeof(g_cli_serve_shape_calls[0]);
