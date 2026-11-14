// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_host.c: inside a sandbox, each program on the host gets a wrapper script that runs it there
#include "ui/cli/src/machine/host.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_text[8192];

// the wrapper of `program` written into the shared text: its size
static unsigned long long wrapper_of(const char *program)
{
    VerbaLinea line = {s_text, sizeof(s_text) - 1u, 0ull};
    EMBED_CALL(cli_host.are_in, CliHostCfg, .name = program, .line = &line);
    s_text[line.at] = '\0';
    return line.at;
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_wrapper_is_a_shell_script(void)
{
    (void)wrapper_of("cargo");
    TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = s_text, .other = "#!/bin/sh\n", .cap = CLI_POOL_BYTES}));
}

void test_a_wrapper_passes_its_program_and_arguments(void)
{
    (void)wrapper_of("cargo");
    CLI_TEST_ASSERT_CONTAINS("set -- 'cargo' \"$@\"", s_text);
}

void test_a_wrapper_runs_on_the_host_in_the_working_directory_with_the_terminal_type(void)
{
    (void)wrapper_of("cargo");
    CLI_TEST_ASSERT_CONTAINS("exec flatpak-spawn --host --watch-bus --directory=\"$PWD\" ${TERM+\"--env=TERM=$TERM\"}", s_text);
}

void test_a_wrapper_ends_with_the_arguments(void)
{
    const unsigned long long size = wrapper_of("cargo");
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(5ull, size);
    TEST_ASSERT_EQUAL_STRING("\"$@\"\n", s_text + size - 5ull);
}

void test_a_quote_in_a_program_name_is_escaped(void)
{
    (void)wrapper_of("it's");
    CLI_TEST_ASSERT_CONTAINS("'it'\\''s'", s_text);
}

void test_writing_wrappers_skips_the_sandbox_tools_and_bad_names(void)
{
    char folder[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    cli_unity_folder("host_wrappers", folder);
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = folder);
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    TEST_ASSERT_EQUAL_INT64(1ll, EMBED_CALL(cli_host_into.are_ins_write, CliHostIntoCfg, .into = folder, .names = "git\nflatpak-spawn\norior\n../x\n\n", .err_msg = &err_msg));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "git", .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "flatpak-spawn", .joined = path);
    TEST_ASSERT_FALSE(EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path));
}
