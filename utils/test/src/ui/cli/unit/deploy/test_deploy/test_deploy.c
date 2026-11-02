// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_deploy.c: patterns leave out names, paths and folders, a release is named by its UTC time, a place is a machine or a local
// folder, a tar stream contains each file with its mode and a long name in a PAX header, and a difference shows removed and added lines
#include "ui/cli/src/deploy/deploy_internal.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_script[] = "#!/bin/sh\r\necho hi\r\n";

static char s_printed[1024];
static VerbaLinea s_printed_line = {s_printed, sizeof(s_printed), 0ull};
static unsigned char s_stream[8192];
static char s_long_name[256];
static char s_release[32];

// the pattern that leaves out `path`, the empty string where none does
static const char *left_out_by(const char *path)
{
    static const char *const s_patterns[4] = {".git/", "*.map", "drafts/", "secret/key.txt"};
    const char *const pattern = EMBED_CALL(cli_deploy_patterns_internal.left_out_by, CliDeployPatternsCtx, .patterns = s_patterns, .count = 4u, .path = path);
    return (pattern != NULL) ? pattern : "";
}

// the release named `seconds` after 1970 began
static const char *release_named(unsigned long long seconds)
{
    VerbaLinea line = {s_release, sizeof(s_release), 0ull};
    EMBED_CALL(cli_deploy_out_seconds_internal.release_name, CliDeployOutSecondsCtx, .seconds = seconds, .out = &line);
    (void)mmgr_line_finish(&line);
    return s_release;
}

// whether `text` reads as a local place, 1, or a machine, 0; -1 where it reads as neither
static int position_is_local(const char *text)
{
    CliDeployPosition place;
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    return EMBED_CALL(cli_deploy_text_internal.place, CliDeployTextCtx, .text = text, .root = "C:/tree", .position_out = &place, .err_msg = &err_msg) ? place.here : -1;
}

// a line the run prints appended to the others, `|` between them
static void line_printed(const CliEachLineSizeCfg *args)
{
    mmgr_line_put(&s_printed_line, (s_printed_line.at > 0ull) ? "|" : "");
    mmgr_line_put_n(&s_printed_line, args->line, args->size);
}

// the tar stream of a short and a long name read into s_stream: its size, at least two kibibytes
static unsigned long long tar_written(void)
{
    char dir[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char tar[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    VerbaLinea name = {s_long_name, sizeof(s_long_name), 0ull};
    for (unsigned int at = 0u; at < 20u; at += 1u)
    {
        mmgr_line_put(&name, "folder");
    }
    (void)mmgr_line_finish(&name);
    cli_unity_folder("deploy_tar", dir);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = dir, .relative = s_long_name, .joined = folder);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder));
    mmgr_line_put(&name, "/x.txt");
    (void)mmgr_line_finish(&name);
    cli_unity_file(dir, "run.sh", s_script);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = dir, .relative = s_long_name, .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = s_script, .size = sizeof(s_script) - 1u));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = dir, .relative = "stream.tar", .joined = tar);
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    g_cli_deploy.root = dir;
    g_cli_deploy.deployment.from = ".";
    CliDeploySent sent[2] = {{"run.sh", 0755u, ""}, {s_long_name, 0644u, ""}};
    g_cli_deploy.sent = sent;
    g_cli_deploy.sents = 2u;
    const int written = EMBED_CALL(cli_deploy_err_msg_path_internal.tar, CliDeployErrMsgPathCtx, .path = tar, .preserved = NULL, .preserved_count = 0u, .time = 1000ull, .err_msg = &err_msg);
    g_cli_deploy.sent = NULL;
    g_cli_deploy.sents = 0u;
    TEST_ASSERT_TRUE(written);
    const unsigned long long size = qry_member_file_read(tar, s_stream, sizeof(s_stream));
    TEST_ASSERT_NOT_EQUAL(QRY_MEMBER_UNREAD, size);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(2048ull, size);
    return size;
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_pattern_leaves_out_a_name(void)
{
    TEST_ASSERT_EQUAL_STRING("*.map", left_out_by("app.js.map"));
}

void test_a_pattern_leaves_out_a_name_in_any_folder(void)
{
    TEST_ASSERT_EQUAL_STRING("*.map", left_out_by("deep/app.js.map"));
}

void test_a_folders_pattern_leaves_out_its_contents(void)
{
    TEST_ASSERT_EQUAL_STRING("drafts/", left_out_by("drafts/one.md"));
}

void test_a_folders_pattern_applies_at_any_depth(void)
{
    TEST_ASSERT_EQUAL_STRING("drafts/", left_out_by("site/drafts/one.md"));
}

void test_a_path_leaves_out_its_file(void)
{
    TEST_ASSERT_EQUAL_STRING("secret/key.txt", left_out_by("secret/key.txt"));
}

void test_gits_folder_is_left_out(void)
{
    TEST_ASSERT_EQUAL_STRING(".git/", left_out_by(".git/HEAD"));
}

void test_a_file_no_pattern_names_is_sent(void)
{
    TEST_ASSERT_EQUAL_STRING("", left_out_by("index.html"));
}

void test_a_name_starting_like_a_folder_is_sent(void)
{
    TEST_ASSERT_EQUAL_STRING("", left_out_by("drafts.html"));
}

void test_a_release_is_named_by_its_utc_time(void)
{
    TEST_ASSERT_EQUAL_STRING("19700101-000000", release_named(0ull));
}

void test_a_release_is_named_to_the_second(void)
{
    TEST_ASSERT_EQUAL_STRING("20261010-224453", release_named(1791672293ull));
}

void test_a_position_on_a_machine_is_not_local(void)
{
    TEST_ASSERT_EQUAL_INT(0, position_is_local("me@web1:/srv/site"));
}

void test_a_position_in_a_container_is_not_local(void)
{
    TEST_ASSERT_EQUAL_INT(0, position_is_local("docker:web:/srv"));
}

void test_a_folder_is_a_local_position(void)
{
    TEST_ASSERT_EQUAL_INT(1, position_is_local("C:/www/site"));
}

void test_a_folder_under_the_tree_is_a_local_position(void)
{
    TEST_ASSERT_EQUAL_INT(1, position_is_local("./out"));
}

void test_a_files_header_names_it_and_its_mode(void)
{
    (void)tar_written();
    TEST_ASSERT_EQUAL_MEMORY("run.sh", s_stream, 6u);
    TEST_ASSERT_EQUAL_MEMORY("0000755", s_stream + 100, 7u);
}

void test_a_files_bytes_follow_its_header(void)
{
    (void)tar_written();
    TEST_ASSERT_EQUAL_MEMORY(s_script, s_stream + 512, sizeof(s_script) - 1u);
}

void test_a_long_name_goes_in_a_pax_header(void)
{
    (void)tar_written();
    TEST_ASSERT_EQUAL_CHAR('x', (char)s_stream[1024 + 156]);
    char pax[300];
    VerbaLinea pax_line = {pax, sizeof(pax), 0ull};
    mmgr_line_put(&pax_line, "path=");
    mmgr_line_put(&pax_line, s_long_name);
    (void)mmgr_line_finish(&pax_line);
    int named = 0;
    for (unsigned long long at = 1536ull; at + pax_line.at <= 2048ull; at += 1ull)
    {
        named |= memor.cmp(&(MemoriaCfg){.src = s_stream + at, .other = pax, .bytes = pax_line.at}) == 0;
    }
    TEST_ASSERT_TRUE(named);
}

void test_a_tar_stream_is_complete_blocks(void)
{
    TEST_ASSERT_EQUAL_UINT64(0ull, tar_written() % 512ull);
}

void test_a_difference_shows_the_removed_and_added_lines(void)
{
    s_printed_line.at = 0ull;
    g_cli_deploy.print = line_printed;
    g_cli_deploy.context = NULL;
    EMBED_CALL(cli_deploy_old_internal.difference, CliDeployOldCtx, .old = "a\nb\nc\n", .old_size = 6ull, .new_text = "a\nB\nc\n", .new_size = 6ull);
    CLI_TEST_ASSERT_TEXT("  @@ line 2|  - b|  + B", s_printed, s_printed_line.at);
}
