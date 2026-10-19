// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_inspect_test.c: a file with no server hears its findings and their fixes, and so does the tree: a file of the
// tree no tab holds is inspected too, a finding's fix is carried out, and the file's findings go once it is changed
#include "../../../../../ui/cli/src/servers/servers.h"
#include "cli_check.h"

// What the files heard: each file's name, how many findings it was last given, and the first's message
typedef struct
{
    char name[16][CLI_NAME_LONGEST];
    unsigned int count[16];
    char first[16][1024];
    unsigned int files;
} InspectHeard;

static InspectHeard s_heard;
static CliLock s_lock = CLI_LOCK_INIT;
static char s_out[1u << 16u];

// a file's diagnostics heard, kept by its name
static void inspect_told(void *context, const char *event, const char *json, unsigned long long size)
{
    (void)context;
    if (!cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "lsp-diagnostics", .cap = CLI_CELLS_POOL}))
    {
        return;
    }
    static CliJsonNode s_nodes[4096];
    static char s_text[1u << 16u];
    static CfgJsonToken s_tokens[4096];
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    CliJson tree;
    cli_lock_take(&s_lock);
    cli_json_begun(&tree, s_nodes, 4096u, s_text, sizeof(s_text));
    const unsigned int read = cli_json_read(&tree, json, size, s_tokens, 4096u, &said);
    const char *const path = cli_json_string(&tree, cli_json_member(&tree, read, "path"), NULL);
    const unsigned int items = cli_json_member(&tree, read, "items");
    const char *name = (path != NULL) ? path : "";
    for (const char *at = name; at[0] != '\0'; at += 1)
    {
        name = ((at[0] == '/') || (at[0] == '\\')) ? (at + 1) : name;
    }
    unsigned int file = 0u;
    while ((file < s_heard.files) &&
           (!cellul.eq(&(CatenaFinitaCfg){.src = s_heard.name[file], .other = name, .cap = CLI_CELLS_POOL})))
    {
        file += 1u;
    }
    if (file < 16u)
    {
        s_heard.files += (file == s_heard.files) ? 1u : 0u;
        memor.cpy(&(MemoriaCfg){.dst = s_heard.name[file],
                                .src = name,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
        s_heard.count[file] = cli_json_is(&tree, items, CFG_JSON_ARRAY) ? tree.node[items].count : 0u;
        const char *const message =
            cli_json_string(&tree, cli_json_member(&tree, cli_json_item(&tree, items, 0u), "message"), NULL);
        memor.cpy(&(MemoriaCfg){
            .dst = s_heard.first[file],
            .src = (message != NULL) ? message : "",
            .bytes =
                cellul.len(&(CatenaFinitaCfg){.src = (message != NULL) ? message : "", .cap = CLI_CELLS_POOL}) + 1u});
    }
    cli_lock_give(&s_lock);
}

// how many findings the file `name` was last given, its first's message into `first`, which holds 1024: -1 where it
// was given none yet
static int inspect_given(const char *name, char *first)
{
    int count = -1;
    cli_lock_take(&s_lock);
    for (unsigned int file = 0u; file < s_heard.files; file += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_heard.name[file], .other = name, .cap = CLI_CELLS_POOL}))
        {
            count = (int)s_heard.count[file];
            memor.cpy(&(MemoriaCfg){
                .dst = first,
                .src = s_heard.first[file],
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_heard.first[file], .cap = CLI_CELLS_POOL}) + 1u});
        }
    }
    cli_lock_give(&s_lock);
    return count;
}

// the edits of the first file an action's edits give written over `text` into s_out
static const char *inspect_applied(const char *text, const char *edits_json, unsigned long long size)
{
    static CliTextEdit s_edits[64];
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(edits_json, size, &read);
    const unsigned int edits = cli_json_member(json, cli_json_item(json, read, 0u), "edits");
    unsigned int count = 0u;
    for (unsigned int at = 0u; (cli_json_item(json, edits, at) != CLI_JSON_NONE) && (count < 64u); at += 1u)
    {
        const unsigned int one = cli_json_item(json, edits, at);
        const unsigned int from = cli_json_member(json, one, "from");
        const unsigned int to = cli_json_member(json, one, "to");
        unsigned long long length = 0ull;
        const char *const written = cli_json_string(json, cli_json_member(json, one, "text"), &length);
        const unsigned int places[4] = {cli_json_member(json, from, "line"), cli_json_member(json, from, "col"),
                                        cli_json_member(json, to, "line"), cli_json_member(json, to, "col")};
        unsigned int values[4] = {0u, 0u, 0u, 0u};
        for (unsigned int place = 0u; place < 4u; place += 1u)
        {
            for (unsigned long long digit = 0ull;
                 (places[place] != CLI_JSON_NONE) && (digit < json->node[places[place]].length); digit += 1ull)
            {
                values[place] =
                    (values[place] * 10u) + (unsigned int)(json->text[json->node[places[place]].text + digit] - '0');
            }
        }
        s_edits[count] = (CliTextEdit){{values[0], values[1]},
                                       {values[2], values[3]},
                                       (written != NULL) ? written : "",
                                       (written != NULL) ? length : 0ull};
        count += 1u;
    }
    static char s_fixed[4096];
    CliLine out = {s_fixed, sizeof(s_fixed), 0ull};
    (void)cli_text_applied(text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), s_edits, count,
                           &out);
    (void)cli_line_closed(&out);
    return s_fixed;
}

int main(void)
{
    static const char s_text[] = "export function f() {\n  const unused = 1;\n  return 2;\n}\n";
    char dir[CLI_PATH_LONGEST];
    char file[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    char why[1024];
    char first[1024];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check_folder("servers_inspect", dir);
    cli_full(dir, "a.js", file);
    (void)query_file_written(file, s_text, sizeof(s_text) - 1u);
    cli_full(dir, "b.py", path);
    static const char s_python[] = "def g():\n    '''Return two.'''\n    return 2\n";
    (void)query_file_written(path, s_python, sizeof(s_python) - 1u);
    CliServersEmit emit = {inspect_told, NULL, ""};
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_check(cli_servers_open(dir, file, "javascript", s_text, sizeof(s_text) - 1u, &emit, &out, &said) &&
                  (cellul.starts(&(CatenaFinitaCfg){.src = s_out, .other = "false", .cap = CLI_CELLS_POOL})),
              "a file with no server opens without one");
    for (unsigned int tries = 0u;
         ((inspect_given("a.js", first) < 0) || (inspect_given("b.py", first) < 0)) && (tries < 100u); tries += 1u)
    {
        query_paused(100u);
    }
    cli_check((inspect_given("a.js", first) == 1) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = first, .other = "`unused` is declared and never read.", .cap = CLI_CELLS_POOL})),
              "a file with no server hears its findings");
    cli_check(inspect_given("b.py", first) == 1, "a file of the tree no tab holds is inspected too");
    const CliTextPlace place = {1u, 9u};
    out.at = 0ull;
    said.at = 0ull;
    cli_check(cli_servers_actions(file, place, place, &out, &said), "its fixes are offered");
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_out, out.at, &read);
    const unsigned int action = cli_json_item(json, read, 0u);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(action, "title"),
                                           .other = "Take out the declaration of unused",
                                           .cap = CLI_CELLS_POOL}),
              "the finding's fix among them");
    static char s_raw[4096];
    CliLine raw = {s_raw, sizeof(s_raw), 0ull};
    cli_json_write(json, cli_json_member(json, action, "raw"), &raw, 0u, 0);
    static char s_edits[4096];
    CliLine edits = {s_edits, sizeof(s_edits), 0ull};
    said.at = 0ull;
    cli_check(cli_servers_act(NULL, file, s_raw, raw.at, &edits, &said), "the fix is carried out");
    const char *const fixed = inspect_applied(s_text, s_edits, edits.at);
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = fixed, .other = "export function f() {\n  return 2;\n}\n", .cap = CLI_CELLS_POOL}),
              "the declaration taken out");
    said.at = 0ull;
    cli_check(
        cli_servers_change(file, fixed, cellul.len(&(CatenaFinitaCfg){.src = fixed, .cap = CLI_CELLS_POOL}), &said),
        "the file is changed");
    for (unsigned int tries = 0u; (inspect_given("a.js", first) != 0) && (tries < 100u); tries += 1u)
    {
        query_paused(100u);
    }
    cli_check(inspect_given("a.js", first) == 0, "and its findings go");
    cli_servers_stop_all();
    return cli_checked();
}
