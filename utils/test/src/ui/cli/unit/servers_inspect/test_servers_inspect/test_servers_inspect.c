// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_servers_inspect.c: a file with no server receives its findings and their fixes, a file of the tree no tab shows is inspected too,
// a finding's fix is carried out, and the file's findings clear once it is changed
#include "ui/cli/src/servers/servers.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// What the files received: each file's name, how many findings it was last sent, and the first's message
typedef struct
{
    char name[16][CLI_NAME_MAX_LEN];
    unsigned int count[16];
    char first[16][1024];
    unsigned int files;
} InspectDiagnostics;

static const char s_text[] = "export function f() {\n  const unused = 1;\n  return 2;\n}\n";

static InspectDiagnostics s_diagnostics;
static CliLock s_lock = CLI_LOCK_INIT;
static char s_out[1u << 16u];
static char s_open_answer[1024];
static char s_why[1024];
static char s_first[1024];
static char s_file[CLI_PATH_MAX_LEN];
static VerbaLinea s_err_msg;
static const CliJson *s_json;

// a file's diagnostics received, recorded by its name
static void diagnostics_recorded(const CliEachEventJsonSizeCfg *args)
{
    const char *json = args->json;
    unsigned long long size = args->size;
    if (!cellul.eq(&(CatenaFinitaCfg){.src = args->event, .other = "lsp-diagnostics", .cap = CLI_CELLS_POOL}))
    {
        return;
    }
    static CliJsonNode s_nodes[4096];
    static char s_tree_text[1u << 16u];
    static CfgJsonToken s_tokens[4096];
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    CliJson tree;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &tree, .node_json_node_pointer = s_nodes, .nodes_most = 4096u, .text_out = s_tree_text, .text_most = sizeof(s_tree_text));
    const unsigned int read = EMBED_CALL(cli_json_tree_text_size_tokens.json_read, CliJsonTreeTextSizeTokensCfg, .json = &tree, .text = json, .size = size, .tokens = s_tokens, .tokens_most = 4096u, .err_msg = &err_msg);
    const char *const path = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &tree, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &tree, .object = read, .name = "path"), .length_pointer = NULL);
    const unsigned int items = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &tree, .object = read, .name = "items");
    const char *name = (path != NULL) ? path : "";
    for (const char *at = name; at[0] != '\0'; at += 1)
    {
        name = ((at[0] == '/') || (at[0] == '\\')) ? (at + 1) : name;
    }
    unsigned int file = 0u;
    while ((file < s_diagnostics.files) && !cellul.eq(&(CatenaFinitaCfg){.src = s_diagnostics.name[file], .other = name, .cap = CLI_CELLS_POOL}))
    {
        file += 1u;
    }
    if (file < 16u)
    {
        s_diagnostics.files += (file == s_diagnostics.files) ? 1u : 0u;
        memor.cpy(&(MemoriaCfg){.dst = s_diagnostics.name[file], .src = name, .bytes = CLI_UNITY_LENGTH(name) + 1ull});
        s_diagnostics.count[file] = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &tree, .node = items, .kind = CFG_JSON_ARRAY) ? tree.node[items].count : 0u;
        const char *const message = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &tree, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &tree, .object = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &tree, .array = items, .index = 0u), .name = "message"), .length_pointer = NULL);
        const char *const first = (message != NULL) ? message : "";
        memor.cpy(&(MemoriaCfg){.dst = s_diagnostics.first[file], .src = first, .bytes = CLI_UNITY_LENGTH(first) + 1ull});
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}

// how many findings the file `name` was last sent, its first's message into s_first: -1 where it was sent none yet
static int findings_of(const char *name)
{
    int count = -1;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    for (unsigned int file = 0u; file < s_diagnostics.files; file += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_diagnostics.name[file], .other = name, .cap = CLI_CELLS_POOL}))
        {
            // a file's findings are far below an int's range
            count = (int)s_diagnostics.count[file];
            memor.cpy(&(MemoriaCfg){.dst = s_first, .src = s_diagnostics.first[file], .bytes = CLI_UNITY_LENGTH(s_diagnostics.first[file]) + 1ull});
        }
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    return count;
}

// up to ten seconds until the file `name` has findings
static void findings_awaited(const char *name)
{
    for (unsigned int tries = 0u; (findings_of(name) < 0) && (tries < 100u); tries += 1u)
    {
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 100u);
    }
}

// the edits of the first file an action's edits name applied to `text`, NUL-terminated
static const char *edits_applied(const char *text, const char *edits_json, unsigned long long size)
{
    static CliTextEdit s_edits[64];
    static char s_fixed[4096];
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(edits_json, size, &read);
    const unsigned int edits = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = read, .index = 0u), .name = "edits");
    unsigned int count = 0u;
    for (unsigned int at = 0u; (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = edits, .index = at) != CLI_JSON_NONE) && (count < 64u); at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = edits, .index = at);
        const unsigned int from = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = one, .name = "from");
        const unsigned int to = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = one, .name = "to");
        unsigned long long length = 0ull;
        const char *const written = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = one, .name = "text"), .length_pointer = &length);
        const unsigned int places[4] = {EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = from, .name = "line"), EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = from, .name = "col"), EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = to, .name = "line"), EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = to, .name = "col")};
        unsigned int values[4] = {0u, 0u, 0u, 0u};
        for (unsigned int place = 0u; place < 4u; place += 1u)
        {
            for (unsigned long long digit = 0ull; (places[place] != CLI_JSON_NONE) && (digit < json->node[places[place]].length); digit += 1ull)
            {
                // a digit's value is below 10
                values[place] = (values[place] * 10u) + (unsigned int)(json->text[json->node[places[place]].text + digit] - '0');
            }
        }
        s_edits[count] = (CliTextEdit){{values[0], values[1]}, {values[2], values[3]}, (written != NULL) ? written : "", (written != NULL) ? length : 0ull};
        count += 1u;
    }
    VerbaLinea out = {s_fixed, sizeof(s_fixed), 0ull};
    (void)EMBED_CALL(cli_edits_out.text_apply, CliEditsOutCfg, .text = text, .size = CLI_UNITY_LENGTH(text), .edits = s_edits, .count = count, .out = &out);
    (void)mmgr_line_finish(&out);
    return s_fixed;
}

// the first action offered at the unused declaration, parsed: its node
static unsigned int first_action(void)
{
    const CliTextPlace place = {1u, 9u};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_servers_path_out_err_msg_from.actions, CliServersPathOutErrMsgFromCfg, .path = s_file, .from = place, .to = place, .out = &out, .err_msg = &s_err_msg));
    unsigned int read = CLI_JSON_NONE;
    s_json = cli_unity_parsed(s_out, out.at, &read);
    return EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = read, .index = 0u);
}

// the first action carried out, its edits applied to the text: the fixed text
static const char *fix_carried_out(void)
{
    static char s_raw[4096];
    static char s_edits[4096];
    const unsigned int action = first_action();
    VerbaLinea raw = {s_raw, sizeof(s_raw), 0ull};
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = s_json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = action, .name = "raw"), .out = &raw, .indent = 0u, .sorted = 0);
    VerbaLinea edits = {s_edits, sizeof(s_edits), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_servers_path_size.act, CliServersPathSizeCfg, .tree = NULL, .path = s_file, .raw = s_raw, .size = raw.at, .out = &edits, .err_msg = &s_err_msg));
    return edits_applied(s_text, s_edits, edits.at);
}

// the tree written and its first file opened, once for the process: the inspections' tree of a root is read once and a finding
// that has not changed is not sent again, and the tests read what the one opening sent
static void tree_opened(void)
{
    static int s_opened = 0;
    if (s_opened)
    {
        return;
    }
    s_opened = 1;
    char dir[CLI_PATH_MAX_LEN];
    cli_unity_folder("servers_inspect", dir);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = dir, .relative = "a.js", .joined = s_file);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = s_file, .bytes = s_text, .size = sizeof(s_text) - 1u));
    cli_unity_file(dir, "b.py", "def g():\n    '''Return two.'''\n    return 2\n");
    // a class whose family sets one attribute twice, the second put of a file's key replacing the first
    cli_unity_file(dir, "c.py",
                   "class Counter:\n    def __init__(self):\n        self.count = 0\n\n"
                   "    def add(self, by=1):\n        '''Count by more.'''\n        self.count += by\n"
                   "        return self.count\n");
    CliServersSink sink = {diagnostics_recorded, NULL, ""};
    VerbaLinea out = {s_open_answer, sizeof(s_open_answer), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_servers_root.open, CliServersRootCfg, .root = dir, .path = s_file, .language = "javascript", .text = s_text, .size = sizeof(s_text) - 1u, .sink = &sink, .out = &out, .err_msg = &s_err_msg));
}

void setUp(void)
{
    cli_unity_begun();
    s_err_msg = (VerbaLinea){s_why, sizeof(s_why), 0ull};
    tree_opened();
}

void tearDown(void)
{
}

void test_a_file_with_no_server_opens_without_one(void)
{
    TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = s_open_answer, .other = "false", .cap = CLI_CELLS_POOL}));
}

void test_a_file_with_no_server_receives_its_findings(void)
{
    findings_awaited("a.js");
    TEST_ASSERT_EQUAL_INT(1, findings_of("a.js"));
    TEST_ASSERT_EQUAL_STRING("`unused` is declared and never read.", s_first);
}

void test_a_file_of_the_tree_no_tab_shows_is_inspected(void)
{
    findings_awaited("b.py");
    TEST_ASSERT_EQUAL_INT(1, findings_of("b.py"));
}

void test_a_class_that_sets_one_attribute_twice_is_inspected(void)
{
    findings_awaited("c.py");
    TEST_ASSERT_GREATER_OR_EQUAL_INT(1, findings_of("c.py"));
}

void test_a_findings_fix_is_offered(void)
{
    findings_awaited("a.js");
    TEST_ASSERT_EQUAL_STRING("Take out the declaration of unused", cli_unity_member_text(first_action(), "title"));
}

void test_the_fix_removes_the_declaration(void)
{
    findings_awaited("a.js");
    TEST_ASSERT_EQUAL_STRING("export function f() {\n  return 2;\n}\n", fix_carried_out());
}

void test_a_files_findings_clear_once_it_is_changed(void)
{
    findings_awaited("a.js");
    const char *const fixed = fix_carried_out();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_servers_root.change, CliServersRootCfg, .path = s_file, .text = fixed, .size = CLI_UNITY_LENGTH(fixed), .err_msg = &s_err_msg));
    for (unsigned int tries = 0u; (findings_of("a.js") != 0) && (tries < 100u); tries += 1u)
    {
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 100u);
    }
    TEST_ASSERT_EQUAL_INT(0, findings_of("a.js"));
    cli_servers_stop_all();
}
