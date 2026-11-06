// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_servers_inspect.c: a file with no server receives its findings, and a file of the tree no tab shows is inspected too
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
static char s_error[1024];
static char s_first[1024];
static char s_file[CLI_PATH_MAX_LEN];
static int s_served = 1;
static VerbaLinea s_err_msg;

// a file's findings received, recorded by its name
static void diagnostics_recorded(const CliServerDiagnosticsCfg *args)
{
    const char *name = args->path;
    for (const char *at = args->path; *at != '\0'; at += 1u)
    {
        name = ((*at == '/') || (*at == '\\')) ? at + 1u : name;
    }
    unsigned int file = 0u;
    while ((file < s_diagnostics.files) && !cellul.eq(&(CatenaFinitaCfg){.src = s_diagnostics.name[file], .other = name, .cap = CLI_POOL_BYTES}))
    {
        file += 1u;
    }
    if (file >= 16u)
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = s_diagnostics.name[file], .src = name, .bytes = CLI_UNITY_LENGTH(name) + 1ull});
    s_diagnostics.count[file] = args->count;
    VerbaLinea first = {s_diagnostics.first[file], sizeof(s_diagnostics.first[file]), 0ull};
    mmgr_line_put_n(&first, (args->count != 0u) ? args->item[0].message : "", (args->count != 0u) ? args->item[0].message_length : 0ull);
    (void)mmgr_line_finish(&first);
    s_diagnostics.files += (file == s_diagnostics.files) ? 1u : 0u;
}

// how many findings the file `name` was last sent, its first's message into s_first: -1 where it was sent none yet
static int findings_of(const char *name)
{
    int count = -1;
    for (unsigned int file = 0u; file < s_diagnostics.files; file += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_diagnostics.name[file], .other = name, .cap = CLI_POOL_BYTES}))
        {
            // a file's findings are far below an int's range
            count = (int)s_diagnostics.count[file];
            memor.cpy(&(MemoriaCfg){.dst = s_first, .src = s_diagnostics.first[file], .bytes = CLI_UNITY_LENGTH(s_diagnostics.first[file]) + 1ull});
        }
    }
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

// the file a.js opened in a tree beside two others, once for the process: a file's findings are sent as they change, and
// the tests read what the one opening sent
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
    static CliServersSink s_sink;
    s_sink = (CliServersSink){.diagnostics = diagnostics_recorded};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_servers_root.open, CliServersRootCfg, .root = dir, .path = s_file, .language = "javascript", .text = s_text, .size = sizeof(s_text) - 1u, .sink = &s_sink, .served = &s_served, .err_msg = &s_err_msg));
}

void setUp(void)
{
    cli_unity_begun();
    s_err_msg = (VerbaLinea){s_error, sizeof(s_error), 0ull};
    tree_opened();
}

void tearDown(void)
{
}

void test_a_file_with_no_server_opens_without_one(void)
{
    TEST_ASSERT_FALSE(s_served);
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

