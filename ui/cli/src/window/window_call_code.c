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
static int code_inspect_languages(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    static const char *const s_languages[2] = {"python", "javascript"};
    cli_window_strings(out, s_languages, 2u);
    return 1;
}

// the classes a parse names, by their index
static int code_parse_classes(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    cli_window_strings(out, g_cli_inspect_classes, CLI_INSPECT_CLASSES);
    return 1;
}

// an edit of the inspections written as the page reads one, its text written beside it
static void code_edit_written(const CliInspectEdit *edit, const VerbaLinea *text, VerbaLinea *out)
{
    const CliTextEdit written = {{edit->from.line, edit->from.col},
                                 {edit->to.line, edit->to.col},
                                 text->out,
                                 mmgr_line_ok(text) ? text->at : 0ull};
    cli_text_edit_written(&written, out);
}

// a line over a cell for an edit of a text of `length` bytes, which writes at most the text twice over and a part of
// its own besides: the cell, given back by g_cli_cells->persistent_buf_release, or NULL with why given and the line
// empty
static char *code_edit_room(unsigned long long length, VerbaLinea *written, VerbaLinea *err_msg)
{
    const unsigned long long room = (2ull * length) + 4096ull;
    char *const cell = g_cli_cells->persistent_buf_alloc(room, NULL);
    *written = (VerbaLinea){cell, (cell != NULL) ? room : 0ull, 0ull};
    if (cell == NULL)
    {
        mmgr_line_put(err_msg, "an edit's text could not be taken");
    }
    return cell;
}

// the text, the language and the line of a call on code: 1, or 0 with why given
static int code_text_at(const CliServeArgs *args, const char **language, const char **text, unsigned long long *line,
                        VerbaLinea *err_msg)
{
    *language = cli_serve_string(args, "language", err_msg);
    *text = (*language != NULL) ? cli_serve_string(args, "text", err_msg) : NULL;
    return (*text != NULL) && cli_serve_whole(args, "line", line, err_msg);
}

// a docstring drawn up for the function at or around `line` of a text in `language`, in `form`: its edit, and the
// place to write its summary
static int code_docstring(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *language = NULL;
    const char *text = NULL;
    unsigned long long line = 0ull;
    const char *const form =
        code_text_at(args, &language, &text, &line, err_msg) ? cli_serve_string(args, "form", err_msg) : NULL;
    if (form == NULL)
    {
        return 0;
    }
    CliInspectEdit edit;
    CliInspectPlace summary;
    VerbaLinea written;
    char *const room =
        code_edit_room(cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), &written, err_msg);
    // a line is far below what an unsigned int counts
    const int drawn =
        (room != NULL) &&
        cli_inspect_docstring(language, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}),
                              (unsigned int)line, form, &edit, &written, &summary, err_msg);
    if (drawn)
    {
        mmgr_line_ch(out, '[');
        code_edit_written(&edit, &written, out);
        mmgr_line_ch(out, ',');
        const CliTextPlace place = {summary.line, summary.col};
        cli_text_place_written(place, out);
        mmgr_line_ch(out, ']');
    }
    cli_cell_release(room, NULL);
    return drawn;
}

// the edit that sorts the methods of the class at or around `line` of a text in `language` by name
static int code_sort(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *language = NULL;
    const char *text = NULL;
    unsigned long long line = 0ull;
    if (!code_text_at(args, &language, &text, &line, err_msg))
    {
        return 0;
    }
    CliInspectEdit edit;
    VerbaLinea written;
    char *const room =
        code_edit_room(cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), &written, err_msg);
    // a line is far below what an unsigned int counts
    const int sorted =
        (room != NULL) &&
        cli_inspect_sort_methods(language, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}),
                                 (unsigned int)line, &edit, &written, err_msg);
    if (sorted)
    {
        code_edit_written(&edit, &written, out);
    }
    cli_cell_release(room, NULL);
    return sorted;
}

// the edit that fills the paragraph of documentation at `line` of a text to `width` columns, 20 at the least
static int code_fill(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const text = cli_serve_string(args, "text", err_msg);
    unsigned long long line = 0ull;
    unsigned long long width = 0ull;
    if ((text == NULL) || !cli_serve_whole(args, "line", &line, err_msg) ||
        !cli_serve_whole(args, "width", &width, err_msg))
    {
        return 0;
    }
    CliTextEdit edit;
    VerbaLinea written;
    char *const room =
        code_edit_room(cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), &written, err_msg);
    // a line and a width are far below what an unsigned int counts
    const int filled =
        (room != NULL) &&
        cli_fill(text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), (unsigned int)line,
                 (unsigned int)((width > 20ull) ? width : 20ull), &written, &edit, err_msg);
    if (filled)
    {
        cli_text_edit_written(&edit, out);
    }
    cli_cell_release(room, NULL);
    return filled;
}

// whether orior_cli reads the file at `path` of the tree itself: a build file, a Gradle script or catalog, a POM, an
// SConstruct or an SConscript, or JSON, read by its schema
static int code_reads_file(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const path = cli_serve_string(args, "path", err_msg);
    if (path == NULL)
    {
        return 0;
    }
    mmgr_line_put(out, ((cli_build_kind_of(path) != 0u) || cli_schema_reads(path)) ? "true" : "false");
    return 1;
}

// every checker the manifest knows: its id, its name, and the languages it checks
static int code_checkers_known(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    CliJson tree;
    const unsigned int manifest = cli_toolchains_manifest(&tree, 0u, 0ull);
    const unsigned int first = cli_json_is(&tree, manifest, CFG_JSON_ARRAY) ? tree.node[manifest].first : CLI_JSON_NONE;
    mmgr_line_ch(out, '[');
    int any = 0;
    for (unsigned int tool = first; tool != CLI_JSON_NONE; tool = tree.node[tool].next)
    {
        const unsigned int checker = cli_json_member(&tree, tool, "checker");
        if (!cli_json_is(&tree, checker, CFG_JSON_OBJECT))
        {
            continue;
        }
        const char *const id = cli_json_string(&tree, cli_json_member(&tree, tool, "id"), NULL);
        const char *const name = cli_json_string(&tree, cli_json_member(&tree, tool, "name"), NULL);
        mmgr_line_put(out, any ? ",[" : "[");
        cli_window_quoted(out, (id != NULL) ? id : "");
        mmgr_line_ch(out, ',');
        cli_window_quoted(out, (name != NULL) ? name : "");
        mmgr_line_put(out, ",[");
        const unsigned int languages = cli_json_member(&tree, checker, "languages");
        for (unsigned int at = 0u; cli_json_item(&tree, languages, at) != CLI_JSON_NONE; at += 1u)
        {
            const char *const one = cli_json_string(&tree, cli_json_item(&tree, languages, at), NULL);
            mmgr_line_put(out, (at > 0u) ? "," : "");
            cli_window_quoted(out, (one != NULL) ? one : "");
        }
        mmgr_line_put(out, "]]");
        any = 1;
    }
    mmgr_line_ch(out, ']');
    cli_json_given(&tree);
    return 1;
}

// every language a formatter formats
static int code_format_languages(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    cli_format_languages(out);
    return 1;
}

// a plugin written as the page reads it, after those written
static void code_plugin_written(void *context, const CliPlugin *plugin)
{
    VerbaLinea *const out = context;
    mmgr_line_put(out, (out->out[out->at - 1u] == '[') ? "{\"id\":" : ",{\"id\":");
    cli_window_quoted(out, plugin->id);
    mmgr_line_put(out, ",\"source\":");
    cli_window_quoted(out, plugin->source);
    mmgr_line_put(out, ",\"folder\":");
    if (plugin->folder != NULL)
    {
        cli_window_quoted(out, plugin->folder);
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    mmgr_line_put(out, ",\"text\":");
    cli_window_counted(out, plugin->text, plugin->size);
    mmgr_line_ch(out, '}');
}

// every plugin, as plugins.c finds them
static int code_plugins_read(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    mmgr_line_ch(out, '[');
    cli_plugins_each(code_plugin_written, out);
    mmgr_line_ch(out, ']');
    return 1;
}

// the code the script plugin `id` runs
static int code_plugin_code(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const id = cli_serve_string(args, "id", err_msg);
    if (id == NULL)
    {
        return 0;
    }
    // the code written straight into the answer's string as it is read
    char piece[4096];
    CliJsonQuoting quoting;
    VerbaLinea code;
    cli_json_quoting_begun(&quoting, out, &code, piece, sizeof(piece));
    const int read = cli_plugins_code_of(id, &code, err_msg);
    cli_json_quoting_ended(&quoting, &code);
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
static void code_sample_line(void *context, const CliLeaves *leaves)
{
    CodeSampleRuns *const runs = (CodeSampleRuns *)context;
    mmgr_line_put(runs->out, (runs->written != 0u) ? "," : "");
    cli_highlight_runs_written(leaves, runs->out);
    runs->written += 1u;
}

// the sample a draft of a plugin opens with, colored by the draft's own grammar, written into `out` as `{"lines",
// "names", "runs"}`, the names `[""]` and no runs where the grammar does not read
static void code_sample_colored(const CliJson *json, unsigned int plugin, VerbaLinea *out)
{
    char text[CODE_SAMPLE_BYTES];
    VerbaLinea sample = {text, sizeof(text), 0ull};
    cli_plugins_sample_of(json, plugin, &sample);
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
            cli_json_quoted(out, text + start, at - start);
            line[count] = text + start;
            count += 1u;
            start = at + 1ull;
        }
    }
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    char ended[CLI_NAME_MAX_LEN];
    VerbaLinea state = {ended, sizeof(ended), 0ull};
    const unsigned int grammar = cli_json_member(json, plugin, "grammar");
    const int stored = (grammar != CLI_JSON_NONE) && cli_highlight_store(CODE_SAMPLE_GRAMMAR, json, grammar, &err_msg);
    mmgr_line_put(out, "],\"names\":");
    if (!stored || !cli_highlight_names_written(CODE_SAMPLE_GRAMMAR, out))
    {
        mmgr_line_put(out, "[\"\"]");
    }
    mmgr_line_put(out, ",\"runs\":[");
    CodeSampleRuns runs = {0u, out};
    if (stored)
    {
        (void)cli_highlight_leaves(CODE_SAMPLE_GRAMMAR, "root", line, count, 1, code_sample_line, &runs, &state,
                                   &err_msg);
    }
    mmgr_line_put(out, "]}");
}

// the plugin the generator's answers `spec` request, as its file would contain it, and the sample it opens with
// colored by its grammar: `{"text", "sample"}`
static int code_plugin_draft(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    CliJson tree = {0};
    const unsigned int spec = cli_plugins_draft_tree(&tree, args->json, cli_serve_arg(args, "spec"), err_msg);
    const unsigned int plugin = (spec != CLI_JSON_NONE) ? cli_plugins_draft(&tree, spec, err_msg) : CLI_JSON_NONE;
    if (plugin != CLI_JSON_NONE)
    {
        char piece[4096];
        CliJsonQuoting quoting;
        VerbaLinea text;
        mmgr_line_put(out, "{\"text\":");
        cli_json_quoting_begun(&quoting, out, &text, piece, sizeof(piece));
        cli_plugins_text_of(&tree, plugin, &text);
        cli_json_quoting_ended(&quoting, &text);
        mmgr_line_put(out, ",\"sample\":");
        code_sample_colored(&tree, plugin, out);
        mmgr_line_ch(out, '}');
    }
    cli_json_given(&tree);
    return plugin != CLI_JSON_NONE;
}

// the plugin the answers `spec` request written to the reader's plugins folder, one of its id there retained unless
// `replace`: its folder
static int code_plugin_create(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    int replace = 0;
    if (!cli_serve_bool(args, "replace", &replace, err_msg))
    {
        return 0;
    }
    char folder[CLI_PATH_MAX_LEN];
    const int created = cli_plugins_create(args->json, cli_serve_arg(args, "spec"), replace, folder, err_msg);
    if (created)
    {
        cli_window_quoted(out, folder);
    }
    return created;
}

// the menus, as the command line reads them
static int code_commands_read(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    if (!cli_commands_read(err_msg))
    {
        return 0;
    }
    unsigned long long size = 0ull;
    const char *const text = cli_commands_text(&size);
    cli_window_counted(out, text, size);
    return 1;
}

// the shell line that brings the tree's dev container up and opens a window on the tree in it, for the terminal to run
static int code_devcontainer_line(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    char root[CLI_PATH_MAX_LEN];
    char program[CLI_PATH_MAX_LEN];
    if (!cli_serve_root(root, err_msg))
    {
        return 0;
    }
    CliPlan *const plan = g_cli_cells->persistent_buf_alloc(sizeof(*plan), NULL);
    const int planned = (plan != NULL) && cli_devcontainer_plan(root, plan, err_msg);
    cli_cell_release(plan, NULL);
    if (!planned)
    {
        return 0;
    }
    if (!cli_program_own(program, sizeof(program)))
    {
        mmgr_line_put(err_msg, "orior's program could not be found");
        return 0;
    }
    char piece[1024];
    CliJsonQuoting quoting;
    VerbaLinea line;
    cli_json_quoting_begun(&quoting, out, &line, piece, sizeof(piece));
    cli_link_path_word(program, &line);
    mmgr_line_put(&line, " --root ");
    cli_link_path_word(root, &line);
    mmgr_line_put(&line, " file dev-container");
    cli_json_quoting_ended(&quoting, &line);
    return 1;
}

const CliServeCall g_cli_window_code_calls[] = {
    {"inspect_languages", code_inspect_languages},
    {"parse_classes", code_parse_classes},
    {"code_docstring", code_docstring},
    {"code_sort", code_sort},
    {"fill_paragraph", code_fill},
    {"reads_file", code_reads_file},
    {"checkers_known", code_checkers_known},
    {"format_languages", code_format_languages},
    {"plugins_read", code_plugins_read},
    {"plugin_code", code_plugin_code},
    {"plugin_draft", code_plugin_draft},
    {"plugin_create", code_plugin_create},
    {"commands_read", code_commands_read},
    {"devcontainer_line", code_devcontainer_line},
};
const unsigned int g_cli_window_code_count = sizeof(g_cli_window_code_calls) / sizeof(g_cli_window_code_calls[0]);
