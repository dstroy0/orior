// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_templates.c: project templates: they list in order, a project created from one has its name substituted, binary files
// copy unchanged, and paths that escape their folder are refused
#include "ui/cli/src/command_line/cells.h"
#include "ui/cli/src/plugins/templates.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_home[CLI_PATH_MAX_LEN];
static char s_parent[CLI_PATH_MAX_LEN];
static char s_made[CLI_PATH_MAX_LEN];
static char s_why[1024];

// a project created from `template_name` as `name`: whether it was
static int created(const char *template_name, const char *name)
{
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    return EMBED_CALL(cli_templates_template_name.create, CliTemplatesTemplateNameCfg, .template_name = template_name, .parent = s_parent, .name = name, .made = s_made, .err_msg = &err_msg);
}

void setUp(void)
{
    cli_unity_begun();
    char path[CLI_PATH_MAX_LEN];
    cli_unity_folder("templates_home", s_home);
    TEST_ASSERT_TRUE(EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_HOME", .value = s_home));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_home, .relative = "templates/script/{{name}}", .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_home, .relative = "templates/script/{{name}}/main.py", .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = "\"\"\"{{name}}.\"\"\"\n{{name}}()\n", .size = 27ull));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_home, .relative = "templates/script/logo.bin", .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = "\xff\xfe\x00", .size = 3ull));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_home, .relative = "templates/app", .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_home, .relative = "work", .joined = s_parent);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = s_parent));
}

void tearDown(void)
{
}

void test_templates_list_in_order(void)
{
    char list[256];
    VerbaLinea listed = {list, sizeof(list), 0ull};
    EMBED_CALL(cli_templates_out.list, CliTemplatesOutCfg, .out = &listed);
    CLI_TEST_ASSERT_TEXT("[\"app\",\"script\"]", list, listed.at);
}

void test_a_project_has_its_name_substituted_everywhere(void)
{
    TEST_ASSERT_TRUE(created("script", "counter"));
    char path[CLI_PATH_MAX_LEN];
    char text[64];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_made, .relative = "counter/main.py", .joined = path);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)text, sizeof(text));
    TEST_ASSERT_NOT_EQUAL(QRY_MEMBER_UNREAD, size);
    CLI_TEST_ASSERT_TEXT("\"\"\"counter.\"\"\"\ncounter()\n", text, size);
}

void test_a_binary_file_copies_unchanged(void)
{
    TEST_ASSERT_TRUE(created("script", "counter"));
    char path[CLI_PATH_MAX_LEN];
    char text[64];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_made, .relative = "logo.bin", .joined = path);
    TEST_ASSERT_EQUAL_UINT64(3ull, qry_member_file_read(path, (unsigned char *)text, sizeof(text)));
    TEST_ASSERT_EQUAL_MEMORY("\xff\xfe\x00", text, 3u);
}

void test_an_existing_folder_is_not_overwritten(void)
{
    TEST_ASSERT_TRUE(created("script", "counter"));
    TEST_ASSERT_FALSE(created("script", "counter"));
}

void test_a_template_name_that_escapes_its_folder_is_refused(void)
{
    TEST_ASSERT_FALSE(created("../x", "y"));
}

void test_a_project_name_that_escapes_its_folder_is_refused(void)
{
    TEST_ASSERT_FALSE(created("script", "a/b"));
}

void test_every_allocation_is_released(void)
{
    (void)created("script", "counter");
    (void)created("script", "counter");
    (void)created("../x", "y");
    TEST_ASSERT_EQUAL_UINT64(0ull, embed_atomic_usize_load(&g_cli_templates_cells.in_use));
}
