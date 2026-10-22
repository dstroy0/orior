// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_test.c: the language servers' check: the projects a server is given are found to six folders down, in order;
// a check stopped lets its files waiting go and says it is done; clangd serves C and names CUDA files as CUDA; Zig,
// Mojo and Ansible are served where their servers are found, and Rust and Python each have one; a hover reads as
// Markdown in each shape; a workspace edit reads in either shape, the first edit first; edits are written by UTF-16
// columns and keep line ends; a signature names each parameter by its span; a server asking for settings is given its
// defaults and an edit is passed on; a file the check lets go keeps its diagnostics and wakes its checker; a file names
// another in its includes and imports only; a report of an attribute names it and its class; and a completion is named
// by what it inserts. The checks that start a server are each marked ignored where they are written and are not
// brought
#include "../../../../../ui/cli/src/servers/servers_internal.h"
#include "cli_check.h"

static CliServerSpec s_spec;
static CliCompletions s_items;
static char s_out[1u << 16u];
static char s_heard[16][4096];
static unsigned int s_heard_count;

// an event heard kept as `event json`
static void servers_told(void *context, const char *event, const char *json, unsigned long long size)
{
    (void)context;
    if (s_heard_count < 16u)
    {
        CliLine line = {s_heard[s_heard_count], sizeof(s_heard[0]), 0ull};
        cli_line_text(&line, event);
        cli_line_character(&line, ' ');
        cli_line_counted(&line, json, size);
        (void)cli_line_closed(&line);
        s_heard_count += 1u;
    }
}

// a check stopped lets its files waiting go and says it is done
static void servers_stop_check(void)
{
    CliServersEmit emit = {servers_told, NULL, ""};
    s_heard_count = 0u;
    cli_servers_check_set(1, &emit, 3ull, 10ull);
    cli_servers_stop_check();
    int on = 0;
    unsigned int waiting = 1u;
    unsigned long long done = 0ull;
    unsigned long long total = 0ull;
    cli_servers_check_count(&on, &waiting, &done, &total);
    cli_check((waiting == 0u) && (done == 3ull) && (total == 3ull), "a check stopped lets its files waiting go");
    cli_check(on, "files that change after are still checked");
    cli_check((s_heard_count == 1u) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = s_heard[0], .other = "tree-check {\"done\":3,\"total\":3}", .cap = CLI_CELLS_POOL})),
              "Problems is told the check is done");
    cli_servers_check_set(0, NULL, 0ull, 0ull);
}

// the program of the server of `language`, "" where none serves it
static const char *servers_program(const char *language)
{
    return cli_servers_spec_for(language, &s_spec) ? s_spec.program : "";
}

// which server serves each language
static void servers_specs(void)
{
    cli_check(cli_servers_spec_for("c", &s_spec) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_spec.tool, .other = "llvm", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_spec.program, .other = "clangd", .cap = CLI_CELLS_POOL})),
              "clangd serves C");
    int cuda = 0;
    for (unsigned int at = 0u; at < s_spec.ids; at += 1u)
    {
        cuda = cuda ||
               ((cellul.eq(&(CatenaFinitaCfg){.src = s_spec.id_extension[at], .other = "cu", .cap = CLI_CELLS_POOL})) &&
                (cellul.eq(&(CatenaFinitaCfg){.src = s_spec.id_name[at], .other = "cuda-cpp", .cap = CLI_CELLS_POOL})));
    }
    cli_check(cuda, "and names CUDA files as CUDA");
    cli_check(!cli_servers_spec_for("plaintext", &s_spec), "plain text has no server");
    cli_check((cellul.eq(&(CatenaFinitaCfg){.src = servers_program("zig"), .other = "zls", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = servers_program("mojo"), .other = "mojo-lsp-server", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = servers_program("ansible"), .other = "ansible-language-server", .cap = CLI_CELLS_POOL})),
              "Zig, Mojo and Ansible are served where their servers are found");
    cli_check((cellul.eq(&(CatenaFinitaCfg){
                  .src = servers_program("rust"), .other = "rust-analyzer", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = servers_program("python"), .other = "pyright-langserver", .cap = CLI_CELLS_POOL})),
              "Rust and Python each have a server");
}

// `text` read into the shared tree: its root
static unsigned int servers_parsed(const char *text, const CliJson **json)
{
    unsigned int read = CLI_JSON_NONE;
    *json = cli_check_parsed(text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), &read);
    return read;
}

// a hover reads as Markdown in each shape
static void servers_markdown(void)
{
    const CliJson *json = NULL;
    unsigned int read = servers_parsed("{\"kind\":\"markdown\",\"value\":\"**x**\"}", &json);
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_servers_markdown_of(json, read, &out);
    cli_check_text(s_out, out.at, "**x**", "a hover of Markdown");
    read = servers_parsed("[{\"language\":\"cpp\",\"value\":\"int x\"},\"a note\"]", &json);
    out.at = 0ull;
    cli_servers_markdown_of(json, read, &out);
    cli_check_text(s_out, out.at, "```cpp\nint x\n```\n\na note", "and one of code and words");
}

// an edit's JSON at `line`, writing `text`
static void servers_edit(CliLine *out, unsigned int line, const char *text)
{
    cli_line_text(out, "{\"range\":{\"start\":{\"line\":");
    cli_line_decimal(out, line, 1u);
    cli_line_text(out, ",\"character\":0},\"end\":{\"line\":");
    cli_line_decimal(out, line, 1u);
    cli_line_text(out, ",\"character\":1}},\"newText\":\"");
    cli_line_text(out, text);
    cli_line_text(out, "\"}");
}

// each file's edits a workspace edit gives, `path:text,text` joined by `|`, into s_out
static const char *servers_files_edited(const char *edit)
{
    const CliJson *json = NULL;
    const unsigned int read = servers_parsed(edit, &json);
    static char s_written[4096];
    CliLine written = {s_written, sizeof(s_written), 0ull};
    cli_servers_workspace_edit_of(json, read, NULL, &written);
    (void)cli_line_closed(&written);
    const unsigned int files = servers_parsed(s_written, &json);
    CliLine out = {s_out, sizeof(s_out), 0ull};
    for (unsigned int at = 0u; cli_json_item(json, files, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int file = cli_json_item(json, files, at);
        const unsigned int edits = cli_json_member(json, file, "edits");
        cli_line_text(&out, (at > 0u) ? "|" : "");
        cli_line_character(&out, ':');
        for (unsigned int one = 0u; cli_json_item(json, edits, one) != CLI_JSON_NONE; one += 1u)
        {
            cli_line_text(&out, (one > 0u) ? "," : "");
            cli_line_text(&out, cli_check_member_text(cli_json_item(json, edits, one), "text"));
        }
    }
    (void)cli_line_closed(&out);
    return s_out;
}

// a workspace edit reads in either shape, the first edit first; and edits are written by UTF-16 columns and keep
// line ends
static void servers_edits(void)
{
    char uri[CLI_PATH_LONGEST];
#if defined(_WIN32)
    cli_servers_uri_of("D:\\t\\a.c", uri);
#else
    cli_servers_uri_of("/t/a.c", uri);
#endif
    static char s_edit[4096];
    CliLine line = {s_edit, sizeof(s_edit), 0ull};
    cli_line_text(&line, "{\"changes\":{\"");
    cli_line_text(&line, uri);
    cli_line_text(&line, "\":[");
    servers_edit(&line, 3u, "b");
    cli_line_character(&line, ',');
    servers_edit(&line, 1u, "a");
    cli_line_text(&line, "]}}");
    (void)cli_line_closed(&line);
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = servers_files_edited(s_edit), .other = ":a,b", .cap = CLI_CELLS_POOL}),
        "a workspace edit's changes, the first edit first");
    line.at = 0ull;
    cli_line_text(&line, "{\"documentChanges\":[{\"textDocument\":{\"uri\":\"");
    cli_line_text(&line, uri);
    cli_line_text(&line, "\",\"version\":2},\"edits\":[");
    servers_edit(&line, 0u, "c");
    cli_line_text(&line, "]},{\"kind\":\"create\",\"uri\":\"file:///t/b.c\"}]}");
    (void)cli_line_closed(&line);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = servers_files_edited(s_edit), .other = ":c", .cap = CLI_CELLS_POOL}),
              "and its document changes, a file made not among them");
    static const char s_text[] = "a\xF0\x9F\x98\x80"
                                 "b x\r\nsecond x\r\n";
    const CliTextEdit edits[3] = {
        {{0u, 3u}, {0u, 4u}, "B", 1ull}, {{1u, 7u}, {1u, 8u}, "yy", 2ull}, {{0u, 5u}, {0u, 9u}, "!", 1ull}};
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_check(cli_text_applied(s_text, sizeof(s_text) - 1u, edits, 3u, &out), "the edits are written");
    cli_check_text(s_out, out.at,
                   "a\xF0\x9F\x98\x80"
                   "B !\r\nsecond yy\r\n",
                   "by UTF-16 columns, the line ends kept");
}

// a signature names each parameter by its span; and a completion is named by what it inserts
static void servers_offers(void)
{
    const CliJson *json = NULL;
    unsigned int read =
        servers_parsed("{\"signatures\":[{\"label\":\"twice(int x, int y) -> int\",\"parameters\":[{\"label\":\"int "
                       "x\"},{\"label\":[13,"
                       "18]}],\"documentation\":{\"kind\":\"markdown\",\"value\":\"Doubles.\"}}],\"activeSignature\":0,"
                       "\"activeParameter\":1}",
                       &json);
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_servers_signature_of(json, read, &out);
    read = servers_parsed(s_out, &json);
    char params[256];
    CliLine line = {params, sizeof(params), 0ull};
    cli_json_write(json, cli_json_member(json, read, "params"), &line, 0u, 0);
    (void)cli_line_closed(&line);
    const unsigned int active = cli_json_member(json, read, "active");
    cli_check((cellul.eq(&(CatenaFinitaCfg){.src = params, .other = "[[6,11],[13,18]]", .cap = CLI_CELLS_POOL})) &&
                  (active != CLI_JSON_NONE) && (json->text[json->node[active].text] == '1') &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = cli_check_member_text(read, "doc"), .other = "Doubles.", .cap = CLI_CELLS_POOL})),
              "a signature names each parameter by its span");
    read = servers_parsed("{\"label\":\" printf(const char *, ...)\",\"kind\":3,\"detail\":\"int\",\"filterText\":"
                          "\"printf\",\"insertText\":\"printf\"}",
                          &json);
    cli_completions_begun(&s_items);
    cli_servers_item_of(json, read, &s_items);
    const CliCompletion *const item = &s_items.item[0];
    cli_check(
        (s_items.count == 1u) && cli_check_completes(&s_items, "printf") &&
            (cellul.eq(&(CatenaFinitaCfg){.src = item->kind, .other = "function", .cap = CLI_CELLS_POOL})) &&
            (item->detail_size == 29u) &&
            (memor.cmp(&(MemoriaCfg){
                 .src = s_items.text + item->detail_at, .other = "int printf(const char *, ...)", .bytes = 29u}) == 0),
        "a completion is named by what it inserts");
}

// what a server says on its own heard, its answer into s_out
static const char *servers_heard(const CliServersEmit *emit, const char *method, const char *params)
{
    CliLine answer = {s_out, sizeof(s_out), 0ull};
    cli_servers_heard((void *)emit, method, params,
                      cellul.len(&(CatenaFinitaCfg){.src = params, .cap = CLI_CELLS_POOL}), &answer);
    (void)cli_line_closed(&answer);
    return s_out;
}

// the count of the items of the array `member` of what was heard at `at`, its event after the first space
static unsigned int servers_heard_items(unsigned int at, const char *member)
{
    const char *const json = cellul.chr(&(CatenaFinitaCfg){.src = s_heard[at], .cap = CLI_CELLS_POOL, .byte = ' '});
    const CliJson *tree = NULL;
    const unsigned int read = servers_parsed((json != NULL) ? json + 1 : "null", &tree);
    const unsigned int array = (member != NULL) ? cli_json_member(tree, read, member) : read;
    return cli_json_is(tree, array, CFG_JSON_ARRAY) ? tree->node[array].count : 0u;
}

// a server asking for settings is given its defaults, and an edit is passed on
static void servers_settings(void)
{
    CliServersEmit emit = {servers_told, NULL, ""};
    s_heard_count = 0u;
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = servers_heard(&emit, "workspace/configuration", "{\"items\":[{},{}]}"),
                                     .other = "[null,null]",
                                     .cap = CLI_CELLS_POOL}),
        "a setting orior_cli does not give is left to the server's default");
    const CliJson *json = NULL;
    const unsigned int python =
        servers_parsed(servers_heard(&emit, "workspace/configuration",
                                     "{\"items\":[{\"section\":\"python\"},{\"section\":\"python.analysis\"}]}"),
                       &json);
    const unsigned int analysis = cli_json_member(json, cli_json_item(json, python, 0u), "analysis");
    cli_check(cli_json_is(json, cli_json_member(json, analysis, "useLibraryCodeForTypes"), CFG_JSON_TRUE),
              "the settings on by default stay on");
    char first[4096];
    char second[4096];
    CliLine one = {first, sizeof(first), 0ull};
    CliLine two = {second, sizeof(second), 0ull};
    cli_json_write(json, analysis, &one, 0u, 1);
    cli_json_write(json, cli_json_item(json, python, 1u), &two, 0u, 1);
    cli_check((one.at == two.at) && (memor.cmp(&(MemoriaCfg){.src = first, .other = second, .bytes = one.at}) == 0),
              "Python's analysis given alike either way");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = servers_heard(&emit, "workspace/applyEdit", "{\"edit\":{\"changes\":{\"file:///t/a.c\":[]}}}"),
                  .other = "{\"applied\":true}",
                  .cap = CLI_CELLS_POOL}),
              "an edit the server asks for is applied");
    cli_check(
        (s_heard_count == 1u) &&
            (cellul.starts(&(CatenaFinitaCfg){.src = s_heard[0], .other = "lsp-edits ", .cap = CLI_CELLS_POOL})) &&
            (servers_heard_items(0u, NULL) == 1u),
        "and passed on");
}

// a file's diagnostics published, `items` of them
static void servers_published(const CliServersEmit *emit, const char *uri, unsigned int items)
{
    static char s_params[1024];
    CliLine line = {s_params, sizeof(s_params), 0ull};
    cli_line_text(&line, "{\"uri\":\"");
    cli_line_text(&line, uri);
    cli_line_text(&line, "\",\"diagnostics\":[");
    for (unsigned int at = 0u; at < items; at += 1u)
    {
        cli_line_text(&line, (at > 0u) ? "," : "");
        cli_line_text(&line, "{\"range\":{\"start\":{\"line\":0,\"character\":0},\"end\":{\"line\":0,\"character\":1}},"
                             "\"severity\":1,\"message\":\"no\"}");
    }
    cli_line_text(&line, "]}");
    (void)cli_line_closed(&line);
    (void)servers_heard(emit, "textDocument/publishDiagnostics", s_params);
}

// the counts of the diagnostics passed on, joined by `,`
static const char *servers_passed_on(void)
{
    static char s_counts[256];
    CliLine line = {s_counts, sizeof(s_counts), 0ull};
    for (unsigned int at = 0u; at < s_heard_count; at += 1u)
    {
        if (cellul.starts(&(CatenaFinitaCfg){.src = s_heard[at], .other = "lsp-diagnostics ", .cap = CLI_CELLS_POOL}))
        {
            cli_line_text(&line, (line.at > 0ull) ? "," : "");
            cli_line_decimal(&line, servers_heard_items(at, "items"), 1u);
        }
    }
    (void)cli_line_closed(&line);
    return s_counts;
}

// a file the check lets go keeps its diagnostics and wakes its checker
static void servers_letting_go(void)
{
#if defined(_WIN32)
    static const char s_uri[] = "file:///C:/t/a.c";
    static const char s_written[] = "file:///c%3A/t/a.c";
#else
    static const char s_uri[] = "file:///t/a.c";
    static const char s_written[] = "file:///t/a.c";
#endif
    CliServersEmit emit = {servers_told, NULL, ""};
    char key[CLI_PATH_LONGEST];
    cli_lsp_key_of(s_uri, sizeof(s_uri) - 1u, key);
    s_heard_count = 0u;
    cli_servers_wait_on(0u, key);
    servers_published(&emit, s_uri, 1u);
    cli_check(cli_servers_was_woken(0u), "a file's diagnostics wake the checker waiting on it");
    cli_servers_let_go_of(key);
    servers_published(&emit, s_uri, 1u);
    servers_published(&emit, s_written, 0u);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = servers_passed_on(), .other = "1,1", .cap = CLI_CELLS_POOL}),
              "a file's diagnostics given after it is let go pass on, and its clearing, however its URI is written, "
              "does not");
    const char *beside = NULL;
    unsigned long long size = 0ull;
    cli_lock_take(&g_cli_servers_published_lock);
    const char *const kept = cli_keyed_get(&g_cli_servers_published, key, &beside, &size);
    unsigned int items = 0u;
    if (kept != NULL)
    {
        const CliJson *json = NULL;
        unsigned int read = CLI_JSON_NONE;
        json = cli_check_parsed(kept, size, &read);
        items = json->node[read].count;
    }
    cli_lock_give(&g_cli_servers_published_lock);
    cli_check(items == 1u, "its diagnostics are kept");
    servers_published(&emit, s_uri, 0u);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = servers_passed_on(), .other = "1,1,0", .cap = CLI_CELLS_POOL}),
              "a clearing once it is not let go passes on");
}

// whether a line of `text` names the stem `stem`
static int servers_names(const char *text, const char *stem)
{
    const char *line = text;
    while (line[0] != '\0')
    {
        const char *const end =
            (cellul.chr(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL, .byte = '\n'}) != NULL)
                ? cellul.chr(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL, .byte = '\n'})
                : (line + cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL}));
        if (cli_servers_line_names(line, (unsigned long long)(end - line), stem,
                                   cellul.len(&(CatenaFinitaCfg){.src = stem, .cap = CLI_CELLS_POOL})))
        {
            return 1;
        }
        line = (end[0] != '\0') ? (end + 1) : end;
    }
    return 0;
}

// a file names another in its includes and imports only; and a report of an attribute names it and its class
static void servers_reports(void)
{
    cli_check(servers_names("#include \"shape.h\"\nint x;\n", "shape"), "an include names a file");
    cli_check(!servers_names("#include \"shapes.h\"\n/* shape */\n", "shape"), "a longer name and a comment do not");
    cli_check(servers_names("import os\nfrom geometry.shape import area\n", "shape"), "an import names a module");
    cli_check(!servers_names("shape = 1\n", "shape"), "a name set does not");
    static const char s_report[] = "Cannot access attribute \"render\" for class \"Saving*\"\n  Attribute \"render\" "
                                   "is unknown";
    const char *name = NULL;
    const char *class_name = NULL;
    unsigned long long name_size = 0ull;
    unsigned long long class_size = 0ull;
    cli_check(cli_servers_attribute_of(s_report, sizeof(s_report) - 1u, &name, &name_size, &class_name, &class_size) &&
                  (name_size == 6ull) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "render", .bytes = 6u}) == 0) &&
                  (class_size == 6ull) &&
                  (memor.cmp(&(MemoriaCfg){.src = class_name, .other = "Saving", .bytes = 6u}) == 0),
              "a report of an attribute names it and its class");
    static const char s_other[] = "Import \"x\" could not be resolved";
    cli_check(!cli_servers_attribute_of(s_other, sizeof(s_other) - 1u, &name, &name_size, &class_name, &class_size),
              "and another report none");
}

// the projects a server is given are found to six folders down, past the folders a build writes, in order
static void servers_projects(void)
{
    static const char *const s_made[6] = {"b/Cargo.toml",       "a/x/Cargo.toml",
                                          "target/Cargo.toml",  "c/d/e/f/g/h/i/Cargo.toml",
                                          ".hidden/Cargo.toml", "Cargo.toml"};
    char root[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    cli_check_folder("servers_projects", root);
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        cli_full(root, s_made[at], path);
        if (cli_path_parent(path, folder))
        {
            (void)query_folder_made(folder);
        }
        (void)query_file_written(path, "", 0ull);
    }
    char listed[4096];
    CliLine out = {listed, sizeof(listed), 0ull};
    cli_servers_projects_in(root, "Cargo.toml", &out);
    char wanted[4096];
    CliLine want = {wanted, sizeof(wanted), 0ull};
    static const char *const s_found[3] = {"Cargo.toml", "a/x/Cargo.toml", "b/Cargo.toml"};
    cli_line_character(&want, '[');
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        cli_full(root, s_found[at], path);
        cli_line_text(&want, (at > 0u) ? "," : "");
        cli_json_quoted_lossy(&want, (const unsigned char *)path,
                              cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    }
    cli_line_character(&want, ']');
    cli_check_text(listed, out.at, wanted, "the projects a server is given are found to six folders down, in order");
    cli_check(g_cli_servers_launch_cells.holds == 0ull, "and the cells the search took are given back");
}

int main(void)
{
    servers_projects();
    servers_stop_check();
    servers_specs();
    servers_markdown();
    servers_edits();
    // the workspace edits read are a few files of a few edits each, far below the room of every edit and file
    cli_check((g_cli_servers_edits_cells.holds == 0ull) && (g_cli_servers_edits_cells.hw != 0ull) &&
                  (g_cli_servers_edits_cells.hw < 16384ull),
              "a workspace edit is held in a cell counted to its files and edits, and given back");
    servers_offers();
    servers_settings();
    servers_letting_go();
    servers_reports();
    return cli_checked();
}
