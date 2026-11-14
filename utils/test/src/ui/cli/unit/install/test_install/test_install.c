// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_install.c: the install's words fill its options and a word it does not take is refused; the default folder
// stands under the reader's programs; a name another PATH folder answers to collides, and the program's own folder and
// the install folder do not
#include "ui/cli/src/machine/install.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_why[CLI_ERR_MSG_MAX_LEN];
static VerbaLinea s_err_msg;

void setUp(void)
{
    cli_unity_begun();
    s_err_msg = (VerbaLinea){s_why, sizeof(s_why), 0ULL};
}

void tearDown(void)
{
}

void test_the_words_fill_the_options(void)
{
    const char *const words[] = {"--accept", "--no-tab", "--debug-port", "9333", "D:/somewhere/orior", "--yes"};
    CliInstallOptions options;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_install.options, CliInstallCfg, .words = words, .count = 6u, .options = &options,
                                .err_msg = &s_err_msg));
    TEST_ASSERT_EQUAL_INT(1, options.accepted);
    TEST_ASSERT_EQUAL_INT(1, options.yes);
    TEST_ASSERT_EQUAL_INT(0, options.skipped[0]);
    TEST_ASSERT_EQUAL_INT(1, options.skipped[1]);
    TEST_ASSERT_EQUAL_STRING("9333", options.port);
    TEST_ASSERT_EQUAL_STRING("D:/somewhere/orior", options.folder);
    TEST_ASSERT_EQUAL_INT(0, options.aliases);
    TEST_ASSERT_EQUAL_INT(0, options.files);
}

void test_a_word_not_taken_is_refused(void)
{
    const char *const words[] = {"--bogus"};
    CliInstallOptions options;
    TEST_ASSERT_FALSE(EMBED_CALL(cli_install.options, CliInstallCfg, .words = words, .count = 1u, .options = &options,
                                 .err_msg = &s_err_msg));
    TEST_ASSERT_TRUE(mmgr_line_finish(&s_err_msg) > 0ULL);
}

void test_a_port_that_is_not_a_number_is_refused(void)
{
    const char *const words[] = {"--debug-port", "soon"};
    CliInstallOptions options;
    TEST_ASSERT_FALSE(EMBED_CALL(cli_install.options, CliInstallCfg, .words = words, .count = 2u, .options = &options,
                                 .err_msg = &s_err_msg));
}

void test_the_default_folder_stands_under_the_readers_programs(void)
{
    char folder[CLI_PATH_MAX_LEN];
    TEST_ASSERT_TRUE(EMBED_CALL(cli_install.default_folder, CliInstallCfg, .home = "/home/reader",
                                .local_app_data = "C:/Users/reader/AppData/Local", .folder_out = folder));
#if defined(_WIN32)
    TEST_ASSERT_EQUAL_STRING("C:/Users/reader/AppData/Local/Programs/orior", folder);
#else
    TEST_ASSERT_EQUAL_STRING("/home/reader/.local/orior", folder);
#endif
}

void test_no_base_gives_no_default_folder(void)
{
    char folder[CLI_PATH_MAX_LEN];
    TEST_ASSERT_FALSE(EMBED_CALL(cli_install.default_folder, CliInstallCfg, .home = "", .local_app_data = "",
                                 .folder_out = folder));
}

void test_a_name_another_folder_answers_to_collides(void)
{
    char taken[CLI_PATH_MAX_LEN];
    char empty[CLI_PATH_MAX_LEN];
    cli_unity_folder("build/install_test/taken", taken);
    cli_unity_folder("build/install_test/empty", empty);
#if defined(_WIN32)
    cli_unity_file(taken, "imp.exe", "not a program");
#else
    cli_unity_file(taken, "imp", "not a program");
#endif
    char folders[2][CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = folders[0], .src = empty, .bytes = sizeof(folders[0])});
    memor.cpy(&(MemoriaCfg){.dst = folders[1], .src = taken, .bytes = sizeof(folders[1])});
    TEST_ASSERT_EQUAL_UINT(2u, EMBED_CALL(cli_install.collision, CliInstallCfg, .name = "imp",
                                          .folders = (const char (*)[CLI_PATH_MAX_LEN])folders, .folder_count = 2u,
                                          .own = "build/install_test/own", .into = "build/install_test/into"));
    TEST_ASSERT_EQUAL_UINT(0u, EMBED_CALL(cli_install.collision, CliInstallCfg, .name = "tab",
                                          .folders = (const char (*)[CLI_PATH_MAX_LEN])folders, .folder_count = 2u,
                                          .own = "build/install_test/own", .into = "build/install_test/into"));
}

void test_the_own_and_install_folders_do_not_collide(void)
{
    char taken[CLI_PATH_MAX_LEN];
    cli_unity_folder("build/install_test/taken", taken);
#if defined(_WIN32)
    cli_unity_file(taken, "tab.exe", "not a program");
#else
    cli_unity_file(taken, "tab", "not a program");
#endif
    char folders[1][CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = folders[0], .src = taken, .bytes = sizeof(folders[0])});
    TEST_ASSERT_EQUAL_UINT(0u, EMBED_CALL(cli_install.collision, CliInstallCfg, .name = "tab",
                                          .folders = (const char (*)[CLI_PATH_MAX_LEN])folders, .folder_count = 1u,
                                          .own = taken, .into = "build/install_test/into"));
    TEST_ASSERT_EQUAL_UINT(0u, EMBED_CALL(cli_install.collision, CliInstallCfg, .name = "tab",
                                          .folders = (const char (*)[CLI_PATH_MAX_LEN])folders, .folder_count = 1u,
                                          .own = "build/install_test/own", .into = taken));
}
