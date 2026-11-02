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
static char s_out[1u << 16u];
static char s_open_answer[1024];
static char s_error[1024];
static char s_first[1024];
static char s_file[CLI_PATH_MAX_LEN];
static VerbaLinea s_err_msg;
static const CliJson *s_json;

// how many findings the file `name` was last sent, its first's message into s_first: -1 where it was sent none yet
static int findings_of(const char *name)
{
    int count = -1;
    for (unsigned int file = 0u; file < s_diagnostics.files; file += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_diagnostics.name[file], .other = name, .cap = CLI_CELLS_POOL}))
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

void setUp(void)
{
    cli_unity_begun();
    s_err_msg = (VerbaLinea){s_error, sizeof(s_error), 0ull};
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

