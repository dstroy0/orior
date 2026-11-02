// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_root.c: mounts: a folder and a file mounted beside the tree are reached by name, and a path may not leave a mount
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_tree[CLI_PATH_MAX_LEN];
static char s_folder[CLI_NAME_MAX_LEN];
static char s_file[CLI_NAME_MAX_LEN];
static char s_other[CLI_PATH_MAX_LEN];
static char s_error[1024];

// the file at `path` holds exactly `required`
static void assert_reads(const char *required, const char *path)
{
    char text[256];
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)text, sizeof(text));
    TEST_ASSERT_NOT_EQUAL(QRY_MEMBER_UNREAD, size);
    CLI_TEST_ASSERT_TEXT(required, text, size);
}

// `@<mount><rest>` into `relative`
static void mounted_path(char *relative, const char *mount, const char *rest)
{
    VerbaLinea named = {relative, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_ch(&named, '@');
    mmgr_line_put(&named, mount);
    mmgr_line_put(&named, rest);
    (void)mmgr_line_finish(&named);
}

// `relative` resolved under the tree into `full`: whether it is allowed
static int resolved(const char *relative, char *full)
{
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    return EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = s_tree, .relative = relative, .joined = full, .err_msg = &err_msg);
}

void setUp(void)
{
    cli_unity_begun();
    char base[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char lone[CLI_PATH_MAX_LEN];
    cli_unity_folder("mounts", base);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = "tree", .joined = s_tree);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = "other", .joined = s_other);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_other, .relative = "sub", .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path));
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = s_tree));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_other, .relative = "sub/a.txt", .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = "a", .size = 1ull));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = "lone.txt", .joined = lone);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = lone, .bytes = "lone", .size = 4ull));
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_path_err_msg_name_internal.mount, CliPathErrMsgNameCtx, .path = s_other, .name_out = s_folder, .err_msg = &err_msg));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_path_err_msg_name_internal.mount, CliPathErrMsgNameCtx, .path = lone, .name_out = s_file, .err_msg = &err_msg));
}

void tearDown(void)
{
    EMBED_CALL(cli_err_msg_name_internal.unmount, CliErrMsgNameCtx, .name = s_folder);
    EMBED_CALL(cli_err_msg_name_internal.unmount, CliErrMsgNameCtx, .name = s_file);
}

void test_mounting_a_folder_again_saves_its_name(void)
{
    char again[CLI_NAME_MAX_LEN];
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_path_err_msg_name_internal.mount, CliPathErrMsgNameCtx, .path = s_other, .name_out = again, .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_STRING(s_folder, again);
}

void test_a_file_in_a_mounted_folder_is_reached_by_its_name(void)
{
    char relative[CLI_PATH_MAX_LEN];
    char reached[CLI_PATH_MAX_LEN];
    mounted_path(relative, s_folder, "/sub/a.txt");
    TEST_ASSERT_TRUE(resolved(relative, reached));
    assert_reads("a", reached);
}

void test_a_mounted_file_is_reached_by_its_name_alone(void)
{
    char relative[CLI_PATH_MAX_LEN];
    char reached[CLI_PATH_MAX_LEN];
    mounted_path(relative, s_file, "");
    TEST_ASSERT_TRUE(resolved(relative, reached));
    assert_reads("lone", reached);
}

void test_a_mounted_path_written_relative_to_the_tree_names_the_mount(void)
{
    char relative[CLI_PATH_MAX_LEN];
    char reached[CLI_PATH_MAX_LEN];
    char back[CLI_PATH_MAX_LEN];
    mounted_path(relative, s_folder, "/sub/a.txt");
    TEST_ASSERT_TRUE(resolved(relative, reached));
    EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = s_tree, .path = reached, .relative_out = back);
    TEST_ASSERT_EQUAL_STRING(relative, back);
}

void test_a_path_of_the_tree_is_written_from_its_top(void)
{
    char path[CLI_PATH_MAX_LEN];
    char back[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_tree, .relative = "x/y.rs", .joined = path);
    EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = s_tree, .path = path, .relative_out = back);
    TEST_ASSERT_EQUAL_STRING("x/y.rs", back);
}

void test_a_path_climbing_out_of_a_mount_is_refused(void)
{
    char relative[CLI_PATH_MAX_LEN];
    char reached[CLI_PATH_MAX_LEN];
    mounted_path(relative, s_folder, "/../escape");
    TEST_ASSERT_FALSE(resolved(relative, reached));
}

void test_a_mount_that_does_not_exist_is_refused(void)
{
    char reached[CLI_PATH_MAX_LEN];
    TEST_ASSERT_FALSE(resolved("@nothing/a", reached));
}

void test_an_unmounted_name_is_no_longer_reached(void)
{
    char relative[CLI_PATH_MAX_LEN];
    char reached[CLI_PATH_MAX_LEN];
    mounted_path(relative, s_folder, "/sub/a.txt");
    EMBED_CALL(cli_err_msg_name_internal.unmount, CliErrMsgNameCtx, .name = s_folder);
    TEST_ASSERT_FALSE(resolved(relative, reached));
}
