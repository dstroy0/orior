// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_scripts_rights.c: the rights of a script plugin, with a plugin.json of the test's own in a fresh ORIOR_HOME.
// The patterns of reads and writes name paths of the tree as a .gitignore names them. A path outside the tree is
// never named. A host matches its name in any case, and `*.` names each host under it. The store keeps one answer
// for each right, and a right that the reader did not answer is unasked
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/plugins/plugins.h"
#include "ui/cli/src/scripts/scripts.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_manifest[] =
    "{\"id\": \"rights\", \"name\": \"Rights\", \"version\": \"1.0.0\", \"type\": \"script\", \"api\": 5, "
    "\"main\": \"plugin.js\", \"reads\": [\"src/**\", \"*.md\"], \"writes\": [\".sonarlint/*.json\"], "
    "\"reaches\": [\"localhost\", \"*.sonarcloud.io\"]}";
static int s_written;

static void file_written(const char *folder, const char *name, const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = name, .joined = path);
    for (char *at = path + CLI_UNITY_LENGTH(folder) + 1u; at[0] != '\0'; at += 1)
    {
        if (at[0] == '/')
        {
            at[0] = '\0';
            EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path);
            at[0] = '/';
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path,
                                        .bytes = text, .size = CLI_UNITY_LENGTH(text)),
                             name);
}

// the plugin written once in a fresh ORIOR_HOME
static void rights_written(void)
{
    if (s_written)
    {
        return;
    }
    char home[CLI_PATH_MAX_LEN];
    cli_unity_folder("scripts_rights_home", home);
    (void)EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_HOME", .value = home);
    file_written(home, "plugins/rights/plugin.json", s_manifest);
    file_written(home, "plugins/rights/plugin.js", "orior.commands.on('none', () => null);\n");
    // the test tells the plugins module of the change, as the folder watch tells it in the window
    cli_plugins_languages_changed();
    s_written = 1;
}

// whether the plugin's list of `right` names `name`
static int rights_named(unsigned int right, const char *name)
{
    rights_written();
    return EMBED_CALL(cli_scripts_rights.named, CliScriptsRightsCfg, .id = "rights", .right = right, .name = name);
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_reads_names_the_paths_its_patterns_name(void)
{
    TEST_ASSERT_TRUE(rights_named(CLI_SCRIPTS_READS, "src/a/b.c"));
    TEST_ASSERT_TRUE(rights_named(CLI_SCRIPTS_READS, "README.md"));
    TEST_ASSERT_TRUE(rights_named(CLI_SCRIPTS_READS, "docs/guide.md"));
    TEST_ASSERT_FALSE(rights_named(CLI_SCRIPTS_READS, "lib/a.c"));
}

void test_writes_names_only_the_files_its_pattern_names(void)
{
    TEST_ASSERT_TRUE(rights_named(CLI_SCRIPTS_WRITES, ".sonarlint/connected.json"));
    TEST_ASSERT_FALSE(rights_named(CLI_SCRIPTS_WRITES, ".sonarlint/folder/connected.json"));
    TEST_ASSERT_FALSE(rights_named(CLI_SCRIPTS_WRITES, "connected.json"));
    TEST_ASSERT_FALSE(rights_named(CLI_SCRIPTS_WRITES, "src/a.c"));
}

void test_a_path_outside_the_tree_is_never_named(void)
{
    TEST_ASSERT_FALSE(rights_named(CLI_SCRIPTS_READS, "../outside.md"));
    TEST_ASSERT_FALSE(rights_named(CLI_SCRIPTS_READS, "src/../../outside.md"));
    TEST_ASSERT_FALSE(rights_named(CLI_SCRIPTS_READS, "/etc/outside.md"));
    TEST_ASSERT_FALSE(rights_named(CLI_SCRIPTS_READS, "C:/outside.md"));
    TEST_ASSERT_FALSE(rights_named(CLI_SCRIPTS_READS, "src\\a.c"));
    TEST_ASSERT_FALSE(rights_named(CLI_SCRIPTS_READS, "./README.md"));
    TEST_ASSERT_FALSE(rights_named(CLI_SCRIPTS_READS, ""));
}

void test_reaches_names_a_host_and_the_hosts_under_a_star(void)
{
    TEST_ASSERT_TRUE(rights_named(CLI_SCRIPTS_REACHES, "localhost"));
    TEST_ASSERT_TRUE(rights_named(CLI_SCRIPTS_REACHES, "LocalHost"));
    TEST_ASSERT_TRUE(rights_named(CLI_SCRIPTS_REACHES, "api.sonarcloud.io"));
    TEST_ASSERT_FALSE(rights_named(CLI_SCRIPTS_REACHES, "sonarcloud.io"));
    TEST_ASSERT_FALSE(rights_named(CLI_SCRIPTS_REACHES, "evilsonarcloud.io"));
    TEST_ASSERT_FALSE(rights_named(CLI_SCRIPTS_REACHES, "example.com"));
}

void test_a_plugin_that_lists_nothing_names_nothing(void)
{
    rights_written();
    TEST_ASSERT_FALSE(EMBED_CALL(cli_scripts_rights.named, CliScriptsRightsCfg, .id = "nobody",
                                 .right = CLI_SCRIPTS_READS, .name = "README.md"));
}

void test_the_store_keeps_one_answer_for_each_right(void)
{
    rights_written();
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    TEST_ASSERT_EQUAL_UINT(CLI_SCRIPTS_UNASKED, EMBED_CALL(cli_scripts_rights.answer, CliScriptsRightsCfg,
                                                           .id = "rights", .right = CLI_SCRIPTS_READS));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_scripts_rights.answer_set, CliScriptsRightsCfg, .id = "rights",
                                .right = CLI_SCRIPTS_READS, .answer = CLI_SCRIPTS_ALLOWED, .err_msg = &err_msg));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_scripts_rights.answer_set, CliScriptsRightsCfg, .id = "rights",
                                .right = CLI_SCRIPTS_REACHES, .answer = CLI_SCRIPTS_REFUSED, .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_UINT(CLI_SCRIPTS_ALLOWED, EMBED_CALL(cli_scripts_rights.answer, CliScriptsRightsCfg,
                                                           .id = "rights", .right = CLI_SCRIPTS_READS));
    TEST_ASSERT_EQUAL_UINT(CLI_SCRIPTS_UNASKED, EMBED_CALL(cli_scripts_rights.answer, CliScriptsRightsCfg,
                                                           .id = "rights", .right = CLI_SCRIPTS_WRITES));
    TEST_ASSERT_EQUAL_UINT(CLI_SCRIPTS_REFUSED, EMBED_CALL(cli_scripts_rights.answer, CliScriptsRightsCfg,
                                                           .id = "rights", .right = CLI_SCRIPTS_REACHES));
}
