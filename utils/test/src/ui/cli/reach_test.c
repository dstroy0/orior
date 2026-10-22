// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// reach_test.c: the windows' reach's check: a command line finds the window on its tree and the window answers, a
// window gone is forgotten by the first command line to find its door shut, and the kept file is the reader's alone
#include "../../../../../ui/cli/src/comms/comms.h"
#include "../../../../../ui/cli/src/comms/door.h"
#include "../../../../../ui/cli/src/machine/reach.h"
#include "cli_check.h"

#if !defined(_WIN32)
#include <sys/stat.h>
#endif

static char s_window[CLI_COMMS_KEY];

// a command the window hears answered on `answer/` and its id, what it says its menu, command and first word
static int reach_heard(void *context, const char *channel, const char *from, const char *body, unsigned long long size)
{
    (void)context;
    (void)channel;
    (void)from;
    unsigned int root = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(body, size, &root);
    char answer_channel[CLI_NAME_LONGEST];
    CliLine channel_line = {answer_channel, sizeof(answer_channel), 0ull};
    cli_line_text(&channel_line, "answer/");
    cli_line_text(&channel_line, cli_check_member_text(root, "id"));
    (void)cli_line_closed(&channel_line);
    char said_text[256];
    CliLine said_line = {said_text, sizeof(said_text), 0ull};
    cli_line_text(&said_line, cli_check_member_text(root, "menu"));
    cli_line_character(&said_line, ' ');
    cli_line_text(&said_line, cli_check_member_text(root, "command"));
    cli_line_character(&said_line, ' ');
    const char *const first = cli_json_string(json, cli_json_item(json, cli_json_member(json, root, "args"), 0u), NULL);
    cli_line_text(&said_line, (first != NULL) ? first : "");
    char reply[512];
    CliLine out = {reply, sizeof(reply), 0ull};
    cli_line_text(&out, "{\"done\":true,\"said\":");
    cli_json_quoted(&out, said_text, said_line.at);
    cli_line_character(&out, '}');
    unsigned long long heard = 0ull;
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    (void)cli_comms_say(s_window, answer_channel, reply, out.at, &heard, &said);
    return 1;
}

// a file of the folder counted
static void reach_counted(void *context, const char *name, int is_folder)
{
    unsigned int *const count = context;
    (void)name;
    *count += is_folder ? 0u : 1u;
}

// the windows kept in `home`: how many
static unsigned int reach_kept(const char *home)
{
    char windows[CLI_PATH_LONGEST];
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
    char home[CLI_PATH_LONGEST];
    char tree[CLI_PATH_LONGEST];
    char other[CLI_PATH_LONGEST];
    char address[CLI_PATH_LONGEST];
    char kept[CLI_COMMS_KEY];
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check_folder("reach_answers", home);
    cli_check(cli_door_open(address, &said), "the door opens");
    cli_check(cli_comms_register("window", 0, 0u, s_command, 1u, s_answer, 1u, s_window, &said) &&
                  cli_comms_register_kept("command line", s_answer, 1u, s_command, 1u, kept, &said),
              "the window and the command line are parts");
    cli_full(home, "tree", tree);
    cli_check(cli_reach_keep_in(home, 1u, tree, address, kept, &said), "the window is kept");
    unsigned int listener = 0u;
    (void)cli_comms_listen(s_window, "command", 0, reach_heard, NULL, &listener, &said);
    static const char *const s_on[1] = {"on"};
    static const char *const s_off[1] = {"off"};
    CliReachAnswer answer;
    memor.set(&(MemoriaCfg){.dst = &answer, .val = 0, .bytes = sizeof(answer)});
    const int asked = cli_reach_ask_in(home, tree, "view", "word-wrap", s_on, 1u, &answer, &said);
    cli_check(
        (asked == 1) && answer.done &&
            (cellul.eq(&(CatenaFinitaCfg){.src = answer.said, .other = "view word-wrap on", .cap = CLI_CELLS_POOL})),
        "the window answers");
    cli_full(home, "other", other);
    cli_check(cli_reach_ask_in(home, other, "view", "word-wrap", NULL, 0u, &answer, &said) == -1,
              "no window on another tree");
    cli_check(cli_reach_ask_in(home, NULL, "view", "word-wrap", s_off, 1u, &answer, &said) == 1,
              "the one window, with no tree named");
    cli_comms_unlisten(listener);
}

// a window gone is forgotten by the first command line to find its door shut
static void reach_gone(void)
{
    char home[CLI_PATH_LONGEST];
    char tree[CLI_PATH_LONGEST];
    char shut[CLI_PATH_LONGEST];
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
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
    (void)cli_reach_keep_in(home, 2u, tree, shut, "00", &said);
    cli_check(reach_kept(home) == 1u, "the window is kept");
    CliReachAnswer answer;
    cli_check(cli_reach_ask_in(home, tree, "file", "save", NULL, 0u, &answer, &said) == -1,
              "a window whose door is shut does not answer");
    cli_check(reach_kept(home) == 0u, "and is forgotten");
}

int main(void)
{
    reach_answers();
    reach_gone();
#if !defined(_WIN32)
    char home[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check_folder("reach_alone", home);
    (void)cli_reach_keep_in(home, 3u, NULL, "a", "k", &said);
    struct stat file;
    struct stat folder;
    cli_full(home, "windows/3.json", path);
    const int file_read = stat(path, &file) == 0;
    cli_full(home, "windows", path);
    const int folder_read = stat(path, &folder) == 0;
    cli_check(file_read && ((file.st_mode & 0777u) == 0600u), "the kept file is the reader's alone");
    cli_check(folder_read && ((folder.st_mode & 0777u) == 0700u), "and its folder");
#endif
    return cli_checked();
}
