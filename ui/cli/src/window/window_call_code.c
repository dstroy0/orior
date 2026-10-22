// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_code.c: the window's calls on code and orior's own lists: the languages orior's inspections read and
// the classes a parse names, a docstring drawn up, a class's methods sorted, a paragraph filled, whether orior_cli
// reads a file itself, the checkers the manifest knows, the languages a formatter formats, the plugins and a plugin's
// code, a plugin drafted from the generator's answers and written, the menus, and the shell line that brings the tree's
// dev container up
#include "../builds/builds.h"
#include "../code/schema.h"
#include "../command_line/cells.h"
#include "../command_line/commands.h"
#include "../comms/link.h"
#include "../deploy/devcontainer.h"
#include "../editor/fill.h"
#include "../inspect/inspect.h"
#include "../plugins/plugins.h"
#include "../text/edits.h"
#include "../toolchains/format.h"
#include "../toolchains/toolchains.h"
#include "window_internal.h"

// the languages orior's own inspections read, by the editor's names for them
static int code_inspect_languages(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    static const char *const s_languages[2] = {"python", "javascript"};
    cli_window_strings(out, s_languages, 2u);
    return 1;
}

// the classes a parse names, by their index
static int code_parse_classes(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    cli_window_strings(out, g_cli_inspect_classes, CLI_INSPECT_CLASSES);
    return 1;
}

// an edit of the inspections written as the page reads one, its text written beside it
static void code_edit_written(const CliInspectEdit *edit, const CliLine *text, CliLine *out)
{
    const CliTextEdit written = {{edit->from.line, edit->from.col},
                                 {edit->to.line, edit->to.col},
                                 text->out,
                                 cli_line_fits(text) ? text->at : 0ull};
    cli_text_edit_written(&written, out);
}

// a line over a cell for an edit of a text of `length` bytes, which writes at most the text twice over and a part of
// its own besides: the cell, given back by cli_cell_given, or NULL with why said and the line empty
static char *code_edit_room(unsigned long long length, CliLine *written, CliLine *said)
{
    const unsigned long long room = (2ull * length) + 4096ull;
    char *const cell = cli_cell_taken(room, "an edit's text");
    *written = (CliLine){cell, (cell != NULL) ? room : 0ull, 0ull};
    if (cell == NULL)
    {
        cli_line_text(said, "an edit's text could not be taken");
    }
    return cell;
}

// the text, the language and the line of a call on code: 1, or 0 with why said
static int code_text_at(const CliServeArgs *args, const char **language, const char **text, unsigned long long *line,
                        CliLine *said)
{
    *language = cli_serve_string(args, "language", said);
    *text = (*language != NULL) ? cli_serve_string(args, "text", said) : NULL;
    return (*text != NULL) && cli_serve_whole(args, "line", line, said);
}

// a docstring drawn up for the function at or around `line` of a text in `language`, in `form`: its edit, and the
// place to write its summary
static int code_docstring(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *language = NULL;
    const char *text = NULL;
    unsigned long long line = 0ull;
    const char *const form =
        code_text_at(args, &language, &text, &line, said) ? cli_serve_string(args, "form", said) : NULL;
    if (form == NULL)
    {
        return 0;
    }
    CliInspectEdit edit;
    CliInspectPlace summary;
    CliLine written;
    char *const room =
        code_edit_room(cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), &written, said);
    // a line is far below what an unsigned int counts
    const int drawn =
        (room != NULL) &&
        cli_inspect_docstring(language, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}),
                              (unsigned int)line, form, &edit, &written, &summary, said);
    if (drawn)
    {
        cli_line_character(out, '[');
        code_edit_written(&edit, &written, out);
        cli_line_character(out, ',');
        const CliTextPlace place = {summary.line, summary.col};
        cli_text_place_written(place, out);
        cli_line_character(out, ']');
    }
    cli_cell_given(room);
    return drawn;
}

// the edit that sorts the methods of the class at or around `line` of a text in `language` by name
static int code_sort(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *language = NULL;
    const char *text = NULL;
    unsigned long long line = 0ull;
    if (!code_text_at(args, &language, &text, &line, said))
    {
        return 0;
    }
    CliInspectEdit edit;
    CliLine written;
    char *const room =
        code_edit_room(cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), &written, said);
    // a line is far below what an unsigned int counts
    const int sorted =
        (room != NULL) &&
        cli_inspect_sort_methods(language, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}),
                                 (unsigned int)line, &edit, &written, said);
    if (sorted)
    {
        code_edit_written(&edit, &written, out);
    }
    cli_cell_given(room);
    return sorted;
}

// the edit that fills the paragraph of documentation at `line` of a text to `width` columns, 20 at the least
static int code_fill(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const text = cli_serve_string(args, "text", said);
    unsigned long long line = 0ull;
    unsigned long long width = 0ull;
    if ((text == NULL) || !cli_serve_whole(args, "line", &line, said) || !cli_serve_whole(args, "width", &width, said))
    {
        return 0;
    }
    CliTextEdit edit;
    CliLine written;
    char *const room =
        code_edit_room(cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), &written, said);
    // a line and a width are far below what an unsigned int counts
    const int filled =
        (room != NULL) &&
        cli_fill(text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), (unsigned int)line,
                 (unsigned int)((width > 20ull) ? width : 20ull), &written, &edit, said);
    if (filled)
    {
        cli_text_edit_written(&edit, out);
    }
    cli_cell_given(room);
    return filled;
}

// whether orior_cli reads the file at `path` of the tree itself: a build file, a Gradle script or catalog, a POM, an
// SConstruct or an SConscript, or JSON, read by its schema
static int code_reads_file(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const path = cli_serve_string(args, "path", said);
    if (path == NULL)
    {
        return 0;
    }
    cli_line_text(out, ((cli_build_kind_of(path) != 0u) || cli_schema_reads(path)) ? "true" : "false");
    return 1;
}

// every checker the manifest knows: its id, its name, and the languages it checks
static int code_checkers_known(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    CliJson tree;
    const unsigned int manifest = cli_toolchains_manifest(&tree, 0u, 0ull);
    const unsigned int first = cli_json_is(&tree, manifest, CFG_JSON_ARRAY) ? tree.node[manifest].first : CLI_JSON_NONE;
    cli_line_character(out, '[');
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
        cli_line_text(out, any ? ",[" : "[");
        cli_window_quoted(out, (id != NULL) ? id : "");
        cli_line_character(out, ',');
        cli_window_quoted(out, (name != NULL) ? name : "");
        cli_line_text(out, ",[");
        const unsigned int languages = cli_json_member(&tree, checker, "languages");
        for (unsigned int at = 0u; cli_json_item(&tree, languages, at) != CLI_JSON_NONE; at += 1u)
        {
            const char *const one = cli_json_string(&tree, cli_json_item(&tree, languages, at), NULL);
            cli_line_text(out, (at > 0u) ? "," : "");
            cli_window_quoted(out, (one != NULL) ? one : "");
        }
        cli_line_text(out, "]]");
        any = 1;
    }
    cli_line_character(out, ']');
    cli_json_given(&tree);
    return 1;
}

// every language a formatter formats
static int code_format_languages(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    cli_format_languages(out);
    return 1;
}

// a plugin written as the page reads it, after those written
static void code_plugin_written(void *context, const CliPlugin *plugin)
{
    CliLine *const out = context;
    cli_line_text(out, (out->out[out->at - 1u] == '[') ? "{\"id\":" : ",{\"id\":");
    cli_window_quoted(out, plugin->id);
    cli_line_text(out, ",\"source\":");
    cli_window_quoted(out, plugin->source);
    cli_line_text(out, ",\"folder\":");
    if (plugin->folder != NULL)
    {
        cli_window_quoted(out, plugin->folder);
    }
    else
    {
        cli_line_text(out, "null");
    }
    cli_line_text(out, ",\"text\":");
    cli_window_counted(out, plugin->text, plugin->size);
    cli_line_character(out, '}');
}

// every plugin, as plugins.c finds them
static int code_plugins_read(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    cli_line_character(out, '[');
    cli_plugins_each(code_plugin_written, out);
    cli_line_character(out, ']');
    return 1;
}

// the code the script plugin `id` runs
static int code_plugin_code(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const id = cli_serve_string(args, "id", said);
    if (id == NULL)
    {
        return 0;
    }
    // the code written straight into the answer's string as it is read
    char piece[4096];
    CliJsonQuoting quoting;
    CliLine code;
    cli_json_quoting_begun(&quoting, out, &code, piece, sizeof(piece));
    const int read = cli_plugins_code_of(id, &code, said);
    cli_json_quoting_ended(&quoting, &code);
    return read;
}

// the plugin the generator's answers `spec` ask for, as its file would hold it
static int code_plugin_draft(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    CliJson tree = {0};
    const unsigned int spec = cli_plugins_draft_tree(&tree, args->json, cli_serve_arg(args, "spec"), said);
    const unsigned int plugin = (spec != CLI_JSON_NONE) ? cli_plugins_draft(&tree, spec, said) : CLI_JSON_NONE;
    if (plugin != CLI_JSON_NONE)
    {
        char piece[4096];
        CliJsonQuoting quoting;
        CliLine text;
        cli_json_quoting_begun(&quoting, out, &text, piece, sizeof(piece));
        cli_plugins_text_of(&tree, plugin, &text);
        cli_json_quoting_ended(&quoting, &text);
    }
    cli_json_given(&tree);
    return plugin != CLI_JSON_NONE;
}

// the plugin the answers `spec` ask for written to the reader's plugins folder, one of its id there kept unless
// `replace`: its folder
static int code_plugin_create(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    int replace = 0;
    if (!cli_serve_bool(args, "replace", &replace, said))
    {
        return 0;
    }
    char folder[CLI_PATH_LONGEST];
    const int created = cli_plugins_create(args->json, cli_serve_arg(args, "spec"), replace, folder, said);
    if (created)
    {
        cli_window_quoted(out, folder);
    }
    return created;
}

// the menus, as the command line reads them
static int code_commands_read(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    if (!cli_commands_read(said))
    {
        return 0;
    }
    unsigned long long size = 0ull;
    const char *const text = cli_commands_text(&size);
    cli_window_counted(out, text, size);
    return 1;
}

// a path written as one word of a POSIX shell's line, its backslashes made slashes
static void code_shell_word(const char *path, CliLine *out)
{
    char slashed[CLI_PATH_LONGEST];
    memor.cpy(&(MemoriaCfg){
        .dst = slashed, .src = path, .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    for (char *at = slashed; *at != '\0'; at += 1)
    {
        *at = (*at == '\\') ? '/' : *at;
    }
    cli_link_quote(slashed, cellul.len(&(CatenaFinitaCfg){.src = slashed, .cap = CLI_CELLS_POOL}), out);
}

// the shell line that brings the tree's dev container up and opens a window on the tree in it, for the terminal to run
static int code_devcontainer_line(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    char root[CLI_PATH_LONGEST];
    char program[CLI_PATH_LONGEST];
    if (!cli_serve_root(root, said))
    {
        return 0;
    }
    CliPlan *const plan = cli_cell_taken(sizeof(*plan), "a dev container's plan");
    const int planned = (plan != NULL) && cli_devcontainer_plan(root, plan, said);
    cli_cell_given(plan);
    if (!planned)
    {
        return 0;
    }
    if (!cli_program_own(program, sizeof(program)))
    {
        cli_line_text(said, "orior's program could not be found");
        return 0;
    }
    char piece[1024];
    CliJsonQuoting quoting;
    CliLine line;
    cli_json_quoting_begun(&quoting, out, &line, piece, sizeof(piece));
    code_shell_word(program, &line);
    cli_line_text(&line, " --root ");
    code_shell_word(root, &line);
    cli_line_text(&line, " file dev-container");
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
