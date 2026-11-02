// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_code.c: the window's calls on code and orior's own lists: the languages orior's inspections read and
// the classes a parse names, a docstring drawn up, a class's methods sorted, a paragraph filled, whether orior_cli
// reads a file itself, the checkers the manifest knows, the languages a formatter formats, the plugins and a plugin's
// code, a plugin drafted from the generator's answers with its sample colored by its grammar, and written, the menus,
// and the shell line that brings the tree's dev container up
#include "../builds/builds.h"
#include "../code/schema.h"
#include "../command_line/cells.h"
#include "../command_line/commands.h"
#include "../comms/link.h"
#include "../deploy/devcontainer.h"
#include "../editor/fill.h"
#include "../editor/highlight.h"
#include "../inspect/inspect.h"
#include "../plugins/plugins.h"
#include "../text/edits.h"
#include "../toolchains/format.h"
#include "../toolchains/toolchains.h"
#include "window_internal.h"

// a drafted plugin's sample: the most bytes and lines it takes, and the key its grammar is stored under to color it
#define CODE_SAMPLE_BYTES 2048u
#define CODE_SAMPLE_LINES 8u
#define CODE_SAMPLE_GRAMMAR "plugin-draft"

// the languages orior's own inspections read, by the editor's names for them
static int code_inspect_languages(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    static const char *const s_languages[2] = {"python", "javascript"};
    EMBED_CALL(cli_window_out_items_internal.strings, CliWindowOutItemsCtx, .out = out, .items = s_languages,
               .count = 2u);
    return 1;
}

// the classes a parse names, by their index
static int code_parse_classes(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    EMBED_CALL(cli_window_out_items_internal.strings, CliWindowOutItemsCtx, .out = out, .items = g_cli_inspect_classes,
               .count = CLI_INSPECT_CLASSES);
    return 1;
}

typedef struct
{
    const CliInspectEdit *edit;
    const VerbaLinea *text;
    VerbaLinea *out;
    unsigned long long length;
    VerbaLinea *written;
    VerbaLinea *err_msg;
    const CliServeArgs *request;
    const char **language;
    const char **text_letters;
    unsigned long long *line;
    const CliJson *json;
    unsigned int plugin;
} WindowCallCodeCtx;
// an edit of the inspections written as the page reads one, its text written beside it
EMBED_INLINE void code_edit_write(const WindowCallCodeCtx *args)
{
    const CliInspectEdit *edit = args->edit;
    VerbaLinea *out = args->out;
    const CliTextEdit written = {{edit->from.line, edit->from.col},
                                 {edit->to.line, edit->to.col},
                                 args->text->out,
                                 mmgr_line_ok(args->text) ? args->text->at : 0ull};
    EMBED_CALL(cli_edits.text_edit_write, CliEditsCfg, .edit = &written, .out = out);
}

// a line over a cell for an edit of a text of `length` bytes, which writes at most the text twice over and a part of
// its own besides: the cell, given back by g_cli_cells->persistent_buf_release, or NULL with why given and the line
// empty
EMBED_INLINE char *code_edit_room(const WindowCallCodeCtx *args)
{
    VerbaLinea *written = args->written;
    const unsigned long long room = (2ull * args->length) + 4096ull;
    char *const cell = g_cli_cells->temporary_buf_alloc(room, &g_cli_window_cells);
    *written = mmgr_line_over(cell, room);
    if (cell == NULL)
    {
        mmgr_line_put(args->err_msg, "an edit's text could not be taken");
    }
    return cell;
}

// the text, the language and the line of a call on code: 1, or 0 with why given
EMBED_INLINE int code_text_at(const WindowCallCodeCtx *args)
{
    const CliServeArgs *arguments = args->request;
    const char **language = args->language;
    const char **text = args->text_letters;
    VerbaLinea *err_msg = args->err_msg;
    *language = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                           .key = "language", .err_msg = err_msg);
    *text = (*language != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                             .key = "text", .err_msg = err_msg)
                                : NULL;
    return (*text != NULL) && EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments,
                                         .key = "line", .value = args->line, .err_msg = err_msg);
}

// a docstring drawn up for the function at or around `line` of a text in `language`, in `form`: its edit, and the
// place to write its summary
static int code_docstring(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *language = NULL;
    const char *text = NULL;
    unsigned long long line = 0ull;
    const char *const form = EMBED_CALL(code_text_at, WindowCallCodeCtx, .request = arguments, .language = &language,
                                        .text_letters = &text, .line = &line, .err_msg = err_msg)
                                 ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                              .request = arguments, .key = "form", .err_msg = err_msg)
                                 : NULL;
    if (form == NULL)
    {
        return 0;
    }
    CliInspectEdit edit;
    CliInspectPlace summary;
    VerbaLinea written;
    char *const room = EMBED_CALL(code_edit_room, WindowCallCodeCtx,
                                  .length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}),
                                  .written = &written, .err_msg = err_msg);
    // a line is far below what an unsigned int counts
    const int drawn =
        (room != NULL) &&
        EMBED_CALL(cli_inspect_language.docstring, CliInspectLanguageCfg, .language = language, .source = text,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}),
                   .line = (unsigned int)line, .form = form, .edit = &edit, .text_verba_linea_pointer = &written,
                   .summary = &summary, .err_msg = err_msg);
    if (drawn)
    {
        mmgr_line_ch(out, '[');
        EMBED_CALL(code_edit_write, WindowCallCodeCtx, .edit = &edit, .text = &written, .out = out);
        mmgr_line_ch(out, ',');
        const CliTextPlace place = {summary.line, summary.col};
        EMBED_CALL(cli_edits_place.text_place_write, CliEditsPlaceCfg, .place = place, .out = out);
        mmgr_line_ch(out, ']');
    }
    g_cli_cells->persistent_buf_release(room, &g_cli_window_cells);
    return drawn;
}

// the edit that sorts the methods of the class at or around `line` of a text in `language` by name
static int code_sort(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *language = NULL;
    const char *text = NULL;
    unsigned long long line = 0ull;
    if (!EMBED_CALL(code_text_at, WindowCallCodeCtx, .request = arguments, .language = &language, .text_letters = &text,
                    .line = &line, .err_msg = err_msg))
    {
        return 0;
    }
    CliInspectEdit edit;
    VerbaLinea written;
    char *const room = EMBED_CALL(code_edit_room, WindowCallCodeCtx,
                                  .length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}),
                                  .written = &written, .err_msg = err_msg);
    // a line is far below what an unsigned int counts
    const int sorted =
        (room != NULL) &&
        EMBED_CALL(cli_inspect_language.sort_methods, CliInspectLanguageCfg, .language = language, .source = text,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}),
                   .line = (unsigned int)line, .edit = &edit, .text_verba_linea_pointer = &written, .err_msg = err_msg);
    if (sorted)
    {
        EMBED_CALL(code_edit_write, WindowCallCodeCtx, .edit = &edit, .text = &written, .out = out);
    }
    g_cli_cells->persistent_buf_release(room, &g_cli_window_cells);
    return sorted;
}

// the edit that fills the paragraph of documentation at `line` of a text to `width` columns, 20 at the least
static int code_fill(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const text = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "text", .err_msg = err_msg);
    unsigned long long line = 0ull;
    unsigned long long width = 0ull;
    if ((text == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "line",
                    .value = &line, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "width",
                    .value = &width, .err_msg = err_msg))
    {
        return 0;
    }
    CliTextEdit edit;
    VerbaLinea written;
    char *const room = EMBED_CALL(code_edit_room, WindowCallCodeCtx,
                                  .length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}),
                                  .written = &written, .err_msg = err_msg);
    // a line and a width are far below what an unsigned int counts
    const int filled = (room != NULL) &&
                       EMBED_CALL(cli_fill_table.fill, CliFillCfg, .text = text,
                                  .size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}),
                                  .line = (unsigned int)line, .width = (unsigned int)((width > 20ull) ? width : 20ull),
                                  .written = &written, .edit = &edit, .err_msg = err_msg);
    if (filled)
    {
        EMBED_CALL(cli_edits.text_edit_write, CliEditsCfg, .edit = &edit, .out = out);
    }
    g_cli_cells->persistent_buf_release(room, &g_cli_window_cells);
    return filled;
}

// whether orior_cli reads the file at `path` of the tree itself: a build file, a Gradle script or catalog, a POM, an
// SConstruct or an SConscript, or JSON, read by its schema
static int code_reads_file(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    if (path == NULL)
    {
        return 0;
    }
    mmgr_line_put(args->out, ((EMBED_CALL(cli_builds_path.build_kind_get, CliBuildsPathCfg, .path = path) != 0u) ||
                              EMBED_CALL(cli_schema.reads, CliSchemaCfg, .path = path))
                                 ? "true"
                                 : "false");
    return 1;
}

// every checker the manifest knows: its id, its name, and the languages it checks
static int code_checkers_know(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    CliJson tree;
    const unsigned int manifest = EMBED_CALL(cli_toolchains_json_nodes_more.manifest, CliToolchainsJsonNodesMoreCfg,
                                             .json_out = &tree, .nodes_more = 0u, .text_more = 0ull);
    const unsigned int first = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &tree,
                                          .node = manifest, .kind = CFG_JSON_ARRAY)
                                   ? tree.node[manifest].first
                                   : CLI_JSON_NONE;
    mmgr_line_ch(out, '[');
    int any = 0;
    for (unsigned int tool = first; tool != CLI_JSON_NONE; tool = tree.node[tool].next)
    {
        const unsigned int checker = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                .json_read = &tree, .object = tool, .name = "checker");
        if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &tree, .node = checker,
                        .kind = CFG_JSON_OBJECT))
        {
            continue;
        }
        const char *const id =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &tree,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &tree,
                                          .object = tool, .name = "id"),
                       .length_pointer = NULL);
        const char *const name =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &tree,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &tree,
                                          .object = tool, .name = "name"),
                       .length_pointer = NULL);
        mmgr_line_put(out, any ? ",[" : "[");
        EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out, .text = (id != NULL) ? id : "");
        mmgr_line_ch(out, ',');
        EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out,
                   .text = (name != NULL) ? name : "");
        mmgr_line_put(out, ",[");
        const unsigned int languages = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                  .json_read = &tree, .object = checker, .name = "languages");
        for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                              .json_read = &tree, .array = languages, .index = at) != CLI_JSON_NONE;
             at += 1u)
        {
            const char *const one =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &tree,
                           .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                              .json_read = &tree, .array = languages, .index = at),
                           .length_pointer = NULL);
            mmgr_line_put(out, (at > 0u) ? "," : "");
            EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out,
                       .text = (one != NULL) ? one : "");
        }
        mmgr_line_put(out, "]]");
        any = 1;
    }
    mmgr_line_ch(out, ']');
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &tree);
    return 1;
}

// every language a formatter formats
static int code_format_languages(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    EMBED_CALL(cli_format_path.languages, CliFormatPathCfg, .out = out);
    return 1;
}

// a plugin written as the page reads it, after those written
static void code_plugin_write(const CliEachPluginCfg *args)
{
    VerbaLinea *const out = args->context;
    mmgr_line_put(out, (out->out[out->at - 1u] == '[') ? "{\"id\":" : ",{\"id\":");
    EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out, .text = args->plugin->id);
    mmgr_line_put(out, ",\"source\":");
    EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out, .text = args->plugin->source);
    mmgr_line_put(out, ",\"folder\":");
    if (args->plugin->folder != NULL)
    {
        EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out, .text = args->plugin->folder);
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    mmgr_line_put(out, ",\"text\":");
    EMBED_CALL(cli_window_out_text_internal.count, CliWindowOutTextCtx, .out = out, .text = args->plugin->text,
               .size = args->plugin->size);
    mmgr_line_ch(out, '}');
}

// every plugin, as plugins.c finds them
static int code_plugins_read(const CliServeAnswerCfg *args)
{
    mmgr_line_ch(args->out, '[');
    EMBED_CALL(cli_plugins_each_table.each, CliPluginsEachCfg, .each = code_plugin_write, .context = args->out);
    mmgr_line_ch(args->out, ']');
    return 1;
}

// the code the script plugin `id` runs
static int code_plugin_code(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const id = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                      .key = "id", .err_msg = err_msg);
    if (id == NULL)
    {
        return 0;
    }
    // the code written straight into the answer's string as it is read
    char piece[4096];
    CliJsonQuoting quoting;
    VerbaLinea code;
    EMBED_CALL(cli_json_tree_out.json_quoting_begin, CliJsonTreeOutCfg, .quoting = &quoting, .out = out, .line = &code,
               .piece = piece, .piece_size = sizeof(piece));
    const int read = EMBED_CALL(cli_plugins_id.code_get, CliPluginsIdCfg, .id = id, .out = &code, .err_msg = err_msg);
    EMBED_CALL(cli_json_tree_out.json_quoting_end, CliJsonTreeOutCfg, .quoting = &quoting, .line = &code);
    return read;
}

// The runs of a draft's sample as they are written: how many lines' runs, and where
typedef struct
{
    unsigned int written;
    VerbaLinea *out;
} CodeSampleRuns;

// one line of a draft's sample colored: its runs written as `[column, name, column, name, ...]`, after a comma past the
// first line
static void code_sample_line(const CliLeavesEachCfg *args)
{
    const CliLeaves *leaves = args->leaves;
    CodeSampleRuns *const runs = (CodeSampleRuns *)args->context;
    mmgr_line_put(runs->out, (runs->written != 0u) ? "," : "");
    EMBED_CALL(cli_highlight.runs_write, CliHighlightCfg, .leaves = leaves, .out = runs->out);
    runs->written += 1u;
}

// the sample a draft of a plugin opens with, colored by the draft's own grammar, written into `out` as `{"lines",
// "names", "runs"}`, the names `[""]` and no runs where the grammar does not read
EMBED_INLINE void code_sample_color(const WindowCallCodeCtx *args)
{
    const CliJson *json = args->json;
    unsigned int plugin = args->plugin;
    VerbaLinea *out = args->out;
    char text[CODE_SAMPLE_BYTES];
    VerbaLinea sample = {text, sizeof(text), 0ull};
    EMBED_CALL(cli_plugins_json_out.sample_get, CliPluginsJsonOutCfg, .json = json, .plugin = plugin, .out = &sample);
    const unsigned long long size = mmgr_line_finish(&sample);
    const char *line[CODE_SAMPLE_LINES];
    unsigned int count = 0u;
    mmgr_line_put(out, "{\"lines\":[");
    // each line ended by its newline, which is put out of it as the NUL it ends with
    for (unsigned long long at = 0ull, start = 0ull; (at < size) && (count < CODE_SAMPLE_LINES); at += 1ull)
    {
        if (text[at] == '\n')
        {
            text[at] = '\0';
            mmgr_line_put(out, (count != 0u) ? "," : "");
            EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = text + start,
                       .length = at - start);
            line[count] = text + start;
            count += 1u;
            start = at + 1ull;
        }
    }
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    char ended[CLI_NAME_MAX_LEN];
    VerbaLinea state = {ended, sizeof(ended), 0ull};
    const unsigned int grammar = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                            .object = plugin, .name = "grammar");
    const int stored = (grammar != CLI_JSON_NONE) &&
                       EMBED_CALL(cli_highlight_key_json.store, CliHighlightKeyJsonCfg, .key = CODE_SAMPLE_GRAMMAR,
                                  .json = json, .def = grammar, .err_msg = &err_msg);
    mmgr_line_put(out, "],\"names\":");
    if (!stored || !EMBED_CALL(cli_highlight_key_err_msg.names_write, CliHighlightKeyErrMsgCfg,
                               .key = CODE_SAMPLE_GRAMMAR, .out = out))
    {
        mmgr_line_put(out, "[\"\"]");
    }
    mmgr_line_put(out, ",\"runs\":[");
    CodeSampleRuns runs = {0u, out};
    if (stored)
    {
        (void)EMBED_CALL(cli_highlight_key.leaves, CliHighlightKeyCfg, .key = CODE_SAMPLE_GRAMMAR, .state = "root",
                         .lines = line, .count = count, .ends = 1, .each = code_sample_line, .context = &runs,
                         .ended = &state, .err_msg = &err_msg);
    }
    mmgr_line_put(out, "]}");
}

// the plugin the generator's answers `spec` request, as its file would contain it, and the sample it opens with
// colored by its grammar: `{"text", "sample"}`
static int code_plugin_draft(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    CliJson tree = {0};
    const unsigned int spec = EMBED_CALL(
        cli_plugins_err_msg_json_spec.draft_tree, CliPluginsErrMsgJsonSpecCfg, .draft = &tree, .json = arguments->json,
        .spec = EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "spec"),
        .err_msg = err_msg);
    const unsigned int plugin =
        (spec != CLI_JSON_NONE) ? EMBED_CALL(cli_plugins_err_msg_json_spec_kind.draft, CliPluginsErrMsgJsonSpecKindCfg,
                                             .json_out = &tree, .spec = spec, .err_msg = err_msg)
                                : CLI_JSON_NONE;
    if (plugin != CLI_JSON_NONE)
    {
        char piece[4096];
        CliJsonQuoting quoting;
        VerbaLinea text;
        mmgr_line_put(out, "{\"text\":");
        EMBED_CALL(cli_json_tree_out.json_quoting_begin, CliJsonTreeOutCfg, .quoting = &quoting, .out = out,
                   .line = &text, .piece = piece, .piece_size = sizeof(piece));
        EMBED_CALL(cli_plugins_json_out.text_get, CliPluginsJsonOutCfg, .json = &tree, .plugin = plugin, .out = &text);
        EMBED_CALL(cli_json_tree_out.json_quoting_end, CliJsonTreeOutCfg, .quoting = &quoting, .line = &text);
        mmgr_line_put(out, ",\"sample\":");
        EMBED_CALL(code_sample_color, WindowCallCodeCtx, .json = &tree, .plugin = plugin, .out = out);
        mmgr_line_ch(out, '}');
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &tree);
    return plugin != CLI_JSON_NONE;
}

// the plugin the answers `spec` request written to the reader's plugins folder, one of its id there retained unless
// `replace`: its folder
static int code_plugin_create(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    int replace = 0;
    if (!EMBED_CALL(cli_serve_err_msg_internal.serve_bool, CliServeErrMsgCtx, .request = arguments, .key = "replace",
                    .value_i32s = &replace, .err_msg = err_msg))
    {
        return 0;
    }
    char folder[CLI_PATH_MAX_LEN];
    const int created = EMBED_CALL(
        cli_plugins_err_msg.create, CliPluginsErrMsgCfg, .json = arguments->json,
        .spec = EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "spec"),
        .replace = replace, .folder = folder, .err_msg = err_msg);
    if (created)
    {
        EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out, .text = folder);
    }
    return created;
}

// the menus, as the command line reads them
static int code_commands_read(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    if (!EMBED_CALL(cli_commands.read, CliCommandsCfg, .err_msg = err_msg))
    {
        return 0;
    }
    unsigned long long size = 0ull;
    const char *const text = EMBED_CALL(cli_commands_size.text, CliCommandsSizeCfg, .size = &size);
    EMBED_CALL(cli_window_out_text_internal.count, CliWindowOutTextCtx, .out = out, .text = text, .size = size);
    return 1;
}

// the shell line that brings the tree's dev container up and opens a window on the tree in it, for the terminal to run
static int code_devcontainer_line(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char program[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_root.root, CliServeErrMsgRootCfg, .root_out = root, .err_msg = err_msg))
    {
        return 0;
    }
    CliPlan *const plan = g_cli_cells->temporary_buf_alloc(sizeof(*plan), &g_cli_window_cells);
    const int planned = (plan != NULL) && EMBED_CALL(cli_devcontainer_root.plan, CliDevcontainerRootCfg, .root = root,
                                                     .plan_out = plan, .err_msg = err_msg);
    g_cli_cells->persistent_buf_release(plan, &g_cli_window_cells);
    if (!planned)
    {
        return 0;
    }
    if (!EMBED_CALL(cli_path_room_internal.program_own, CliPathRoomCtx, .path_out = program,
                    .room_size = sizeof(program)))
    {
        mmgr_line_put(err_msg, "orior's program could not be found");
        return 0;
    }
    char piece[1024];
    CliJsonQuoting quoting;
    VerbaLinea line;
    EMBED_CALL(cli_json_tree_out.json_quoting_begin, CliJsonTreeOutCfg, .quoting = &quoting, .out = out, .line = &line,
               .piece = piece, .piece_size = sizeof(piece));
    EMBED_CALL(cli_link_out_path.path_word, CliLinkOutPathCfg, .path = program, .out = &line);
    mmgr_line_put(&line, " --root ");
    EMBED_CALL(cli_link_out_path.path_word, CliLinkOutPathCfg, .path = root, .out = &line);
    mmgr_line_put(&line, " file dev-container");
    EMBED_CALL(cli_json_tree_out.json_quoting_end, CliJsonTreeOutCfg, .quoting = &quoting, .line = &line);
    return 1;
}

const CliServeCall g_cli_window_code_calls[] = {
    {"inspect_languages", code_inspect_languages},
    {"parse_classes", code_parse_classes},
    {"code_docstring", code_docstring},
    {"code_sort", code_sort},
    {"fill_paragraph", code_fill},
    {"reads_file", code_reads_file},
    {"checkers_known", code_checkers_know},
    {"format_languages", code_format_languages},
    {"plugins_read", code_plugins_read},
    {"plugin_code", code_plugin_code},
    {"plugin_draft", code_plugin_draft},
    {"plugin_create", code_plugin_create},
    {"commands_read", code_commands_read},
    {"devcontainer_line", code_devcontainer_line},
};
const unsigned int g_cli_window_code_count = sizeof(g_cli_window_code_calls) / sizeof(g_cli_window_code_calls[0]);
