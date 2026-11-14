// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_reach.c: a command line finds the window on its tree and the window answers, a closed window is forgotten by the first command
// line to find its interface closed, and on POSIX the stored file is readable only by its owner
#include "ui/cli/src/comms/comms.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/comms/interface.h"
#include "ui/cli/src/machine/reach.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

#if !defined(_WIN32)
#include <sys/stat.h>
#endif

static const char *const s_on[1] = {"on"};
static const char *const s_off[1] = {"off"};

static char s_window[CLI_COMMS_KEY];
static char s_persistent_key[CLI_COMMS_KEY];
static char s_address[CLI_PATH_MAX_LEN];
static char s_home[CLI_PATH_MAX_LEN];
static char s_tree[CLI_PATH_MAX_LEN];
static char s_error[512];
static VerbaLinea s_err_msg;
static unsigned int s_listener;
static CliReachAnswer s_answer;

// a command the window receives answered on `answer/` and its id, its message its menu, command and first word; a
// command's bytes are its id, menu, command and words, each ended by a 0
static int command_answered(const CliCommsRecvCfg *args)
{
    const char *const id = args->body;
    const char *const menu = id + CLI_UNITY_LENGTH(id) + 1u;
    const char *const command = menu + CLI_UNITY_LENGTH(menu) + 1u;
    const char *const first = command + CLI_UNITY_LENGTH(command) + 1u;
    char answer_channel[CLI_NAME_MAX_LEN];
    VerbaLinea channel_line = {answer_channel, sizeof(answer_channel), 0ull};
    mmgr_line_put(&channel_line, "answer/");
    mmgr_line_put(&channel_line, id);
    (void)mmgr_line_finish(&channel_line);
    char reply[512];
    VerbaLinea out = {reply, sizeof(reply), 0ull};
    mmgr_line_ch(&out, CLI_REACH_DONE);
    mmgr_line_put(&out, menu);
    mmgr_line_ch(&out, ' ');
    mmgr_line_put(&out, command);
    mmgr_line_ch(&out, ' ');
    mmgr_line_put(&out, first);
    unsigned long long delivered = 0ull;
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    (void)EMBED_CALL(cli_comms_channel_err_msg.send, CliCommsChannelErrMsgCfg, .key = s_window, .channel = answer_channel, .body = reply, .size = out.at, .delivered = &delivered, .err_msg = &err_msg);
    return 1;
}

// a file of the folder counted
static void file_counted(const CliEachNameIsFolderCfg *args)
{
    unsigned int *const count = args->context;
    *count += args->is_folder ? 0u : 1u;
}

// the windows stored in the home: how many
static unsigned int stored_windows(void)
{
    char windows[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_home, .relative = "windows", .joined = windows);
    unsigned int count = 0u;
    (void)EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = windows, .each = file_counted, .context_bytess = &count);
    return count;
}

// the interface opened and the window and the command line registered, once for the process
static void ipc_opened(void)
{
    static const char *const s_command[1] = {"command"};
    static const char *const s_answers[1] = {"answer/*"};
    static int s_opened;
    if (!s_opened)
    {
        TEST_ASSERT_TRUE(EMBED_CALL(cli_ipc_address.open, CliIpcAddressCfg, .address = s_address, .err_msg = &s_err_msg));
        TEST_ASSERT_TRUE(EMBED_CALL(cli_comms_name.comms_register, CliCommsNameCfg, .name = "window", .has_process = 0, .process = 0u, .listen = s_command, .listens = 1u, .send_grant = s_answers, .send_grants = 1u, .key_out = s_window, .err_msg = &s_err_msg));
        TEST_ASSERT_TRUE(EMBED_CALL(cli_comms_name.register_persistent, CliCommsNameCfg, .name = "command line", .listen = s_answers, .listens = 1u, .send_grant = s_command, .send_grants = 1u, .key_out = s_persistent_key, .err_msg = &s_err_msg));
        s_opened = 1;
    }
}

// a window on the tree stored and answering commands
static void window_answering(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_reach.store_in, CliReachCfg, .home = s_home, .process = 1u, .root = s_tree, .address = s_address, .key = s_persistent_key, .err_msg = &s_err_msg));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_comms_channel.listen, CliCommsChannelCfg, .key = s_window, .channel = "command", .queued = 0, .recv = command_answered, .context = NULL, .listener = &s_listener, .err_msg = &s_err_msg));
}

// a window on the tree stored behind a closed interface
static void window_closed(void)
{
    char closed[CLI_PATH_MAX_LEN];
#if defined(_WIN32)
    static const char s_pipe[] = "\\\\.\\pipe\\orior-comms-gone-0";
    memor.cpy(&(MemoriaCfg){.dst = closed, .src = s_pipe, .bytes = sizeof(s_pipe)});
#else
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_home, .relative = "interface", .joined = closed);
#endif
    (void)EMBED_CALL(cli_reach.store_in, CliReachCfg, .home = s_home, .process = 2u, .root = s_tree, .address = closed, .key = "00", .err_msg = &s_err_msg);
}

void setUp(void)
{
    cli_unity_begun();
    s_err_msg = (VerbaLinea){s_error, sizeof(s_error), 0ull};
    memor.set(&(MemoriaCfg){.dst = &s_answer, .val = 0, .bytes = sizeof(s_answer)});
    ipc_opened();
    cli_unity_folder("reach", s_home);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_home, .relative = "tree", .joined = s_tree);
    s_listener = 0u;
}

void tearDown(void)
{
    if (s_listener != 0u)
    {
        EMBED_CALL(cli_comms_listener.unlisten, CliCommsListenerCfg, .listener_value = s_listener);
    }
}

void test_a_window_on_the_tree_answers(void)
{
    window_answering();
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, EMBED_CALL(cli_reach_menu.request_in, CliReachMenuCfg, .home = s_home, .root = s_tree, .menu = "view", .command = "word-wrap", .request = s_on, .count = 1u, .answer = &s_answer, .err_msg = &s_err_msg), s_error);
    TEST_ASSERT_TRUE(s_answer.done);
    TEST_ASSERT_EQUAL_STRING("view word-wrap on", s_answer.text);
}

void test_no_window_answers_on_another_tree(void)
{
    char other[CLI_PATH_MAX_LEN];
    window_answering();
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_home, .relative = "other", .joined = other);
    TEST_ASSERT_EQUAL_INT(-1, EMBED_CALL(cli_reach_menu.request_in, CliReachMenuCfg, .home = s_home, .root = other, .menu = "view", .command = "word-wrap", .request = NULL, .count = 0u, .answer = &s_answer, .err_msg = &s_err_msg));
}

void test_the_only_window_answers_with_no_tree_named(void)
{
    window_answering();
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, EMBED_CALL(cli_reach_menu.request_in, CliReachMenuCfg, .home = s_home, .root = NULL, .menu = "view", .command = "word-wrap", .request = s_off, .count = 1u, .answer = &s_answer, .err_msg = &s_err_msg), s_error);
}

void test_a_window_is_stored(void)
{
    window_closed();
    TEST_ASSERT_EQUAL_UINT(1u, stored_windows());
}

void test_a_window_whose_ipc_is_closed_does_not_answer(void)
{
    window_closed();
    TEST_ASSERT_EQUAL_INT(-1, EMBED_CALL(cli_reach_menu.request_in, CliReachMenuCfg, .home = s_home, .root = s_tree, .menu = "file", .command = "save", .request = NULL, .count = 0u, .answer = &s_answer, .err_msg = &s_err_msg));
}

void test_a_window_whose_ipc_is_closed_is_forgotten(void)
{
    window_closed();
    (void)EMBED_CALL(cli_reach_menu.request_in, CliReachMenuCfg, .home = s_home, .root = s_tree, .menu = "file", .command = "save", .request = NULL, .count = 0u, .answer = &s_answer, .err_msg = &s_err_msg);
    TEST_ASSERT_EQUAL_UINT(0u, stored_windows());
}

void test_posix_the_stored_file_is_readable_only_by_its_owner(void)
{
#if defined(_WIN32)
    TEST_IGNORE_MESSAGE("POSIX file modes");
#else
    char path[CLI_PATH_MAX_LEN];
    struct stat file;
    (void)EMBED_CALL(cli_reach.store_in, CliReachCfg, .home = s_home, .process = 3u, .root = NULL, .address = "a", .key = "k", .err_msg = &s_err_msg);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_home, .relative = "windows/3.window", .joined = path);
    TEST_ASSERT_EQUAL_INT(0, stat(path, &file));
    TEST_ASSERT_EQUAL_UINT(0600u, file.st_mode & 0777u);
#endif
}

void test_posix_the_stored_folder_is_readable_only_by_its_owner(void)
{
#if defined(_WIN32)
    TEST_IGNORE_MESSAGE("POSIX file modes");
#else
    char path[CLI_PATH_MAX_LEN];
    struct stat folder;
    (void)EMBED_CALL(cli_reach.store_in, CliReachCfg, .home = s_home, .process = 3u, .root = NULL, .address = "a", .key = "k", .err_msg = &s_err_msg);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_home, .relative = "windows", .joined = path);
    TEST_ASSERT_EQUAL_INT(0, stat(path, &folder));
    TEST_ASSERT_EQUAL_UINT(0700u, folder.st_mode & 0777u);
#endif
}
