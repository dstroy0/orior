// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// reach_test.c: the windows' reach's check: a command line finds the window on its tree and the window answers, a
// window gone is forgotten by the first command line to find its door shut, and the stored file is the reader's alone
#include "../../../../../ui/cli/src/comms/comms.h"
#include "../../../../../ui/cli/src/comms/door.h"
#include "../../../../../ui/cli/src/machine/reach.h"
#include "cli_check.h"

#if !defined(_WIN32)
#include <sys/stat.h>
#endif

static char s_window[CLI_COMMS_KEY];

// a command the window receives answered on `answer/` and its id, its message its menu, command and first word
static int reach_command_answer(void *context, const char *channel, const char *from, const char *body,
                                unsigned long long size)
{
    (void)context;
    (void)channel;
    (void)from;
    unsigned int root = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(body, size, &root);
    char answer_channel[CLI_NAME_MAX_LEN];
    VerbaLinea channel_line = {answer_channel, sizeof(answer_channel), 0ull};
    mmgr_line_put(&channel_line, "answer/");
    mmgr_line_put(&channel_line, cli_check_member_text(root, "id"));
    (void)mmgr_line_finish(&channel_line);
    char reply_text[256];
    VerbaLinea reply_line = {reply_text, sizeof(reply_text), 0ull};
    mmgr_line_put(&reply_line, cli_check_member_text(root, "menu"));
    mmgr_line_ch(&reply_line, ' ');
    mmgr_line_put(&reply_line, cli_check_member_text(root, "command"));
    mmgr_line_ch(&reply_line, ' ');
    const char *const first = cli_json_string(json, cli_json_item(json, cli_json_member(json, root, "args"), 0u), NULL);
    mmgr_line_put(&reply_line, (first != NULL) ? first : "");
    char reply[512];
    VerbaLinea out = {reply, sizeof(reply), 0ull};
    mmgr_line_put(&out, "{\"done\":true,\"message\":");
    cli_json_quoted(&out, reply_text, reply_line.at);
    mmgr_line_ch(&out, '}');
    unsigned long long delivered = 0ull;
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    (void)cli_comms_send(s_window, answer_channel, reply, out.at, &delivered, &err_msg);
    return 1;
}

// a file of the folder counted
static void reach_counted(void *context, const char *name, int is_folder)
{
    unsigned int *const count = context;
    (void)name;
    *count += is_folder ? 0u : 1u;
}

// the windows stored in `home`: how many
static unsigned int reach_stored_count(const char *home)
{
    char windows[CLI_PATH_MAX_LEN];
    cli_full(home, "windows", windows);
    unsigned int count = 0u;
    (void)query_folder_each(windows, reach_counted, &count);
    return count;
}

// a command line finds the window on its tree and the window answers
static void reach_answers(void)
{
    static const char *const s_command[1] = {"command"};
    static const char *const s_answer[1] = {"answer/*"};
    char home[CLI_PATH_MAX_LEN];
    char tree[CLI_PATH_MAX_LEN];
    char other[CLI_PATH_MAX_LEN];
    char address[CLI_PATH_MAX_LEN];
    char persistent_key[CLI_COMMS_KEY];
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_check_folder("reach_answers", home);
    cli_check(cli_door_open(address, &err_msg), "the door opens");
    cli_check(cli_comms_register("window", 0, 0u, s_command, 1u, s_answer, 1u, s_window, &err_msg) &&
                  cli_comms_register_persistent("command line", s_answer, 1u, s_command, 1u, persistent_key, &err_msg),
              "the window and the command line are parts");
    cli_full(home, "tree", tree);
    cli_check(cli_reach_store_in(home, 1u, tree, address, persistent_key, &err_msg), "the window is stored");
    unsigned int listener = 0u;
    (void)cli_comms_listen(s_window, "command", 0, reach_command_answer, NULL, &listener, &err_msg);
    static const char *const s_on[1] = {"on"};
    static const char *const s_off[1] = {"off"};
    CliReachAnswer answer;
    memor.set(&(MemoriaCfg){.dst = &answer, .val = 0, .bytes = sizeof(answer)});
    const int requested = cli_reach_request_in(home, tree, "view", "word-wrap", s_on, 1u, &answer, &err_msg);
    cli_check(
        (requested == 1) && answer.done &&
            (cellul.eq(&(CatenaFinitaCfg){.src = answer.text, .other = "view word-wrap on", .cap = CLI_CELLS_POOL})),
        "the window answers");
    cli_full(home, "other", other);
    cli_check(cli_reach_request_in(home, other, "view", "word-wrap", NULL, 0u, &answer, &err_msg) == -1,
              "no window on another tree");
    cli_check(cli_reach_request_in(home, NULL, "view", "word-wrap", s_off, 1u, &answer, &err_msg) == 1,
              "the one window, with no tree named");
    cli_comms_unlisten(listener);
}

// a window gone is forgotten by the first command line to find its door shut
static void reach_gone(void)
{
    char home[CLI_PATH_MAX_LEN];
    char tree[CLI_PATH_MAX_LEN];
    char shut[CLI_PATH_MAX_LEN];
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_check_folder("reach_gone", home);
    cli_full(home, "tree", tree);
#if defined(_WIN32)
    memor.cpy(&(MemoriaCfg){
        .dst = shut,
        .src = "\\\\.\\pipe\\orior-comms-gone-0",
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = "\\\\.\\pipe\\orior-comms-gone-0", .cap = CLI_CELLS_POOL}) + 1u});
#else
    cli_full(home, "door", shut);
#endif
    (void)cli_reach_store_in(home, 2u, tree, shut, "00", &err_msg);
    cli_check(reach_stored_count(home) == 1u, "the window is stored");
    CliReachAnswer answer;
    cli_check(cli_reach_request_in(home, tree, "file", "save", NULL, 0u, &answer, &err_msg) == -1,
              "a window whose door is shut does not answer");
    cli_check(reach_stored_count(home) == 0u, "and is forgotten");
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    reach_answers();
    reach_gone();
#if !defined(_WIN32)
    char home[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_check_folder("reach_alone", home);
    (void)cli_reach_store_in(home, 3u, NULL, "a", "k", &err_msg);
    struct stat file;
    struct stat folder;
    cli_full(home, "windows/3.json", path);
    const int file_read = stat(path, &file) == 0;
    cli_full(home, "windows", path);
    const int folder_read = stat(path, &folder) == 0;
    cli_check(file_read && ((file.st_mode & 0777u) == 0600u), "the stored file is the reader's alone");
    cli_check(folder_read && ((folder.st_mode & 0777u) == 0700u), "and its folder");
#endif
    return cli_checked();
}
