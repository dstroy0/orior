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

typedef struct
{
    const CliJson *json;
    unsigned int edits;
    CliTextEdit **edit_of;
    VerbaLinea *err_msg;
} ServeCallShapeCtx;
// the edits of a file as the page gives them, `[{"from", "to", "text"}]`, read into `edit_of`, a cell of their count
// taken here and given back with g_cli_cells->persistent_buf_release(..., owner) either way: their count, or
// CLI_SERVE_EDITS_REFUSED with why given where one is no edit or the cell is refused
EMBED_INLINE unsigned int lsp_edits_get(const ServeCallShapeCtx *args)
{
    const CliJson *json = args->json;
    CliTextEdit **edit_of = args->edit_of;
    VerbaLinea *err_msg = args->err_msg;
    const unsigned int first = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                          .node = args->edits, .kind = CFG_JSON_ARRAY)
                                   ? json->node[args->edits].first
                                   : CLI_JSON_NONE;
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
        const char *const text =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = one, .name = "text"),
                       .length_pointer = &size);
        if (!EMBED_CALL(cli_serve_err_msg_internal.place, CliServeErrMsgCtx, .request = &edit, .key = "from",
                        .place = &from, .err_msg = err_msg) ||
            !EMBED_CALL(cli_serve_err_msg_internal.place, CliServeErrMsgCtx, .request = &edit, .key = "to",
                        .place = &to, .err_msg = err_msg) ||
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

static int edits_write(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    const unsigned int files =
        EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "files");
    const CliJson *const json = arguments->json;
    unsigned long long written = 0ull;
    int done = 1;
    for (unsigned int file = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                        .node = files, .kind = CFG_JSON_ARRAY)
                                 ? json->node[files].first
                                 : CLI_JSON_NONE;
         done && (file != CLI_JSON_NONE); file = json->node[file].next)
    {
        const char *const path =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = file, .name = "path"),
                       .length_pointer = NULL);
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
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = path, .joined = full);
        unsigned long long size = 0ull;
        const unsigned char *const mapped = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = full,
                                                       .size_pointer = &size, .owner = &g_cli_serve_shape_cells);
        const char *const was = (const char *)mapped;
        CliTextEdit *edit = NULL;
        const unsigned int count =
            EMBED_CALL(lsp_edits_get, ServeCallShapeCtx, .json = json,
                       .edits = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                           .object = file, .name = "edits"),
                       .edit_of = &edit, .err_msg = err_msg);
        const unsigned long long text_room = size + CLI_SERVE_WRITTEN_MORE;
        char *const text_cell = g_cli_cells->temporary_buf_alloc(text_room, &g_cli_serve_shape_cells);
        VerbaLinea text = mmgr_line_over(text_cell, text_room);
        const int begun = text.out != NULL;
        const int read = (mapped != NULL) &&
                         EMBED_CALL(cli_json_tree_size.utf8_whole, CliJsonTreeSizeCfg, .bytes = was, .size = size);
        if (!read)
        {
            mmgr_line_put(err_msg, path);
            mmgr_line_put(err_msg, ": it could not be read");
            done = 0;
        }
        else if ((count == CLI_SERVE_EDITS_REFUSED) || !begun ||
                 !EMBED_CALL(cli_edits_out.text_apply, CliEditsOutCfg, .text = was, .size = size, .edits = edit,
                             .count = count, .out = &text) ||
                 !mmgr_line_ok(&text))
        {
            done = 0;
        }
        // the file is let go before it is written over, as the write puts a new file in its place
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = mapped,
                   .owner = &g_cli_serve_shape_cells);
        if (done && !EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = full,
                                .bytes = text.out, .size = text.at))
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
static void lsp_shape_find(const CliShapeEachCfg *args)
{
    const CliShapeFound *found = args->found;
    CliServeShapes *const shapes = args->context;
    mmgr_line_put(shapes->found, (shapes->found->at > 1ull) ? "," : "");
    EMBED_CALL(cli_shape_table.found_write, CliShapeCfg, .path = shapes->file, .found = found, .out = shapes->found);
    if (found->has_with)
    {
        const CliTextEdit edit = {found->from, found->to, found->with, found->with_size};
        mmgr_line_put(shapes->edits, shapes->any ? "," : "");
        EMBED_CALL(cli_edits.text_edit_write, CliEditsCfg, .edit = &edit, .out = shapes->edits);
        shapes->any = 1;
    }
}

static int shape_search(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    const char *const language = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                            .key = "language", .err_msg = err_msg);
    const char *const pattern = (language != NULL)
                                    ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                 .request = arguments, .key = "pattern", .err_msg = err_msg)
                                    : NULL;
    if ((pattern == NULL) || !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx,
                                         .root = root, .err_msg = err_msg))
    {
        return 0;
    }
    const char *const template_text = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx,
                                                 .request = arguments, .key = "template", .otherwise = NULL);
    const char *const one = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx, .request = arguments,
                                       .key = "path", .otherwise = NULL);
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
    CliServeFiles *const listed = (one == NULL)
                                      ? EMBED_CALL(cli_serve_err_msg_root_internal.files_list, CliServeErrMsgRootCtx,
                                                   .root_read = root, .err_msg = err_msg)
                                      : NULL;
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
        EMBED_CALL(cli_plugins_table.languages_for_call, CliPluginsCfg, .languages = languages);
    }
    // the languages the pattern and each file are read in, read once for the search
    CliSymbolsLanguages *const read = cli_symbols_languages_for_call();
    mmgr_line_ch(&found, '[');
    mmgr_line_ch(&files, '[');
    const unsigned int count = (one != NULL) ? 1u : listed->count;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const file = (one != NULL) ? one : listed->paths[at];
        char id[CLI_NAME_MAX_LEN];
        if ((one == NULL) && (!EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg,
                                          .languages_read = languages, .path = file, .id_out = id) ||
                              (!cellul.eq(&(CatenaFinitaCfg){.src = id, .other = language, .cap = CLI_CELLS_POOL}))))
        {
            continue;
        }
        char full[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = file, .joined = full);
        // the text as the editor has it, a cell of its size, else the file as the disk stores it, mapped
        unsigned long long size = 0ull;
        char *const editor_text = EMBED_CALL(cli_servers_path_size_kind.text_take, CliServersPathSizeKindCfg,
                                             .path = full, .size_pointer = &size);
        const unsigned char *const mapped = (editor_text == NULL)
                                                ? EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = full,
                                                             .size_pointer = &size, .owner = &g_cli_serve_shape_cells)
                                                : NULL;
        const char *const text = (editor_text != NULL) ? editor_text : (const char *)mapped;
        if ((text != NULL) && ((editor_text != NULL) || EMBED_CALL(cli_json_tree_size.utf8_whole, CliJsonTreeSizeCfg,
                                                                   .bytes = text, .size = size)))
        {
            edits.at = 0ull;
            CliServeShapes shapes = {file, &found, &edits, 0};
            EMBED_CALL(cli_shape_languages.search, CliShapeLanguagesCfg, .languages = read, .language = language,
                       .src = text, .size = size, .pattern = pattern, .template_text = template_text,
                       .each = lsp_shape_find, .context = &shapes);
            if (shapes.any && mmgr_line_ok(&edits))
            {
                mmgr_line_put(&files, (files.at > 1ull) ? ",{\"path\":" : "{\"path\":");
                EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &files,
                           .text_bytes = (const unsigned char *)file,
                           .length = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}));
                mmgr_line_put(&files, ",\"edits\":[");
                mmgr_line_put_n(&files, edits.out, edits.at);
                mmgr_line_put(&files, "]}");
            }
        }
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = mapped,
                   .owner = &g_cli_serve_shape_cells);
        g_cli_cells->persistent_buf_release(editor_text, &g_cli_serve_shape_cells);
    }
    mmgr_line_ch(&found, ']');
    mmgr_line_ch(&files, ']');
    mmgr_line_put(out, "{\"found\":");
    mmgr_line_put_n(out, found.out, found.at);
    mmgr_line_put(out, ",\"files\":");
    mmgr_line_put_n(out, files.out, files.at);
    mmgr_line_ch(out, '}');
    EMBED_CALL(cli_symbols.languages_free, CliSymbolsCfg, .read = read);
    EMBED_CALL(cli_serve_listed_internal.files_free, CliServeListedCtx, .listed = listed);
    if (languages != NULL)
    {
        EMBED_CALL(cli_plugins_table.languages_free, CliPluginsCfg, .languages = languages);
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
