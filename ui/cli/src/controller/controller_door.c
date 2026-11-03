// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// controller_door.c: the command line reaching the window open on its tree. The door's thread hands a command here: its
// bytes are copied into one cell and pushed on a list any thread pushes to and the controller's thread alone takes
// whole, by a compare and swap and an exchange, and the controller's thread runs each as the menus run it and answers
// on the command's own channel. The bytes are reach.h's: a command's id, menu, command and words, each ended by a 0
#include "controller_internal.h"

#include "../command_line/cells.h"
#include "../command_line/command_line.h"
#include "../comms/comms.h"
#include "../comms/door.h"
#include "../machine/reach.h"

// A command waiting: the next on the list, its id and name, and its words; all of it in one cell
typedef struct ControllerDoorCommand
{
    struct ControllerDoorCommand *next;
    const char *id;
    const char *command;
    const char **word_text;
    unsigned int words;
} ControllerDoorCommand;

// The door: the commands pushed and not yet taken, a pointer in the word; and this part's key, in a cell of its own
typedef struct
{
    _Alignas(64) embed_atomic_u64 pushed;
    char *key;
} ControllerDoor;

static ControllerDoor s_door;

typedef struct
{
    const char *id;
    int ran;
    const char *command;
} ControllerDoorCtx;

// from the door's thread: a command's bytes copied and pushed, those not ended by a 0 or short of a command let go
static int door_received(const CliCommsRecvCfg *args)
{
    unsigned int fields = 0u;
    for (unsigned long long at = 0ULL; at < args->size; fields += 1u)
    {
        at += cellul.len(&(CatenaFinitaCfg){.src = args->body + at, .cap = args->size - at}) + 1ULL;
    }
    if ((fields < CLI_REACH_HEAD_FIELDS) || (args->body[args->size - 1ULL] != '\0'))
    {
        return 1;
    }
    const unsigned int words = fields - CLI_REACH_HEAD_FIELDS;
    ControllerDoorCommand *const command = g_cli_cells->persistent_buf_alloc(
        sizeof(*command) + ((unsigned long long)words * sizeof(*command->word_text)) + args->size,
        &g_cli_controller_cells);
    if (command == NULL)
    {
        return 1;
    }
    command->word_text = (const char **)(void *)(command + 1);
    char *const text = (char *)(void *)(command->word_text + words);
    memor.cpy(&(MemoriaCfg){.dst = text, .src = args->body, .bytes = args->size});
    command->id = text;
    unsigned long long at = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = args->size}) + 1ULL;
    at += cellul.len(&(CatenaFinitaCfg){.src = text + at, .cap = args->size - at}) + 1ULL;
    command->command = text + at;
    at += cellul.len(&(CatenaFinitaCfg){.src = text + at, .cap = args->size - at}) + 1ULL;
    for (unsigned int word_text = 0u; word_text < words; word_text += 1u)
    {
        command->word_text[word_text] = text + at;
        at += cellul.len(&(CatenaFinitaCfg){.src = text + at, .cap = args->size - at}) + 1ULL;
    }
    command->words = words;
    // a pointer fits the 64-bit word the list's head stands in
    embed_u64 head = EMBED_ATOMIC_LOAD(&s_door.pushed);
    do
    {
        command->next = (ControllerDoorCommand *)(uintptr_t)head;
    } while (!EMBED_ATOMIC_CAS_64(&s_door.pushed, &head, (embed_u64)(uintptr_t)command));
    cli_controller_wake();
    return 1;
}

int cli_model_door_open(const CliModelCtx *args)
{
    char address[CLI_PATH_MAX_LEN];
    char line_key[CLI_COMMS_KEY];
    s_door.key = g_cli_cells->persistent_buf_alloc(CLI_COMMS_KEY, &g_cli_controller_cells);
    if (s_door.key == NULL)
    {
        mmgr_line_put(args->err_msg, "the cells cannot meet the door's key");
        return 0;
    }
    static const char *const s_listen[1] = {"command"};
    static const char *const s_send[1] = {"answer/*"};
    unsigned int listener = 0u;
    // the door is open from the start, for every process the container starts to join; the part that takes the commands
    // is one of this process alone, whose key the door never takes, and the key the command line holds grants it the
    // command's channel
    return EMBED_CALL(cli_ipc_address.open, CliIpcAddressCfg, .address = address, .err_msg = args->err_msg) &&
           EMBED_CALL(cli_comms_name.comms_register, CliCommsNameCfg, .name = "orior", .has_process = 0, .process = 0u,
                      .listen = s_listen, .listens = 1u, .send_grant = s_send, .send_grants = 1u, .key_out = s_door.key,
                      .err_msg = args->err_msg) &&
           EMBED_CALL(cli_comms_channel.listen, CliCommsChannelCfg, .key = s_door.key, .channel = "command",
                      .queued = 0, .recv = door_received, .context = NULL, .listener = &listener,
                      .err_msg = args->err_msg) &&
           EMBED_CALL(cli_comms_name.register_persistent, CliCommsNameCfg, .name = "command line", .listen = s_send,
                      .listens = 1u, .send_grant = s_listen, .send_grants = 1u, .key_out = line_key,
                      .err_msg = args->err_msg) &&
           EMBED_CALL(cli_reach.store, CliReachCfg, .root = args->path, .address = address, .key = line_key,
                      .err_msg = args->err_msg);
}

// what a command came to, sent on its own channel: `1` where it ran, else `0` and why
EMBED_INLINE void door_answer(const ControllerDoorCtx *args)
{
    char channel[CLI_NAME_MAX_LEN];
    VerbaLinea named = {channel, sizeof(channel), 0ULL};
    mmgr_line_put(&named, "answer/");
    mmgr_line_put(&named, args->id);
    (void)mmgr_line_finish(&named);
    char text[CLI_NAME_MAX_LEN + 64u];
    VerbaLinea body = {text, sizeof(text), 0ULL};
    mmgr_line_ch(&body, args->ran ? CLI_REACH_DONE : CLI_REACH_REFUSED);
    if (!args->ran)
    {
        mmgr_line_put(&body, args->command);
        mmgr_line_put(&body, " is not a command of the window's");
    }
    char quiet[256];
    VerbaLinea err_msg = {quiet, sizeof(quiet), 0ULL};
    unsigned long long delivered = 0ULL;
    (void)EMBED_CALL(cli_comms_channel_err_msg.send, CliCommsChannelErrMsgCfg, .key = s_door.key, .channel = channel,
                     .body = text, .size = mmgr_line_ok(&body) ? body.at : 1ULL, .delivered = &delivered,
                     .err_msg = &err_msg);
}

int cli_model_door_step(const CliModelCtx *args)
{
    (void)args;
    ControllerDoorCommand *pushed =
        (ControllerDoorCommand *)(uintptr_t)EMBED_ATOMIC_EXCHANGE_64(&s_door.pushed, (embed_u64)0u);
    if (pushed == NULL)
    {
        return 0;
    }
    // the list stands newest first: turned over, each runs in the order it came
    ControllerDoorCommand *first = NULL;
    while (pushed != NULL)
    {
        ControllerDoorCommand *const next = pushed->next;
        pushed->next = first;
        first = pushed;
        pushed = next;
    }
    while (first != NULL)
    {
        ControllerDoorCommand *const next = first->next;
        const int ran = EMBED_CALL(cli_model_commands.command_named, CliModelCtx, .path = first->command,
                                   .words = first->word_text, .at = first->words);
        EMBED_CALL(door_answer, ControllerDoorCtx, .id = first->id, .ran = ran, .command = first->command);
        g_cli_cells->persistent_buf_release(first, &g_cli_controller_cells);
        first = next;
    }
    return 1;
}

void cli_model_door_end(const CliModelCtx *args)
{
    (void)args;
    g_cli_cells->persistent_buf_release(s_door.key, &g_cli_controller_cells);
    s_door.key = NULL;
}

void cli_model_launch_run(const CliModelCtx *args)
{
    (void)args;
    const CliOutcome *const launch = cli_controller_launch();
    if (launch->command[0] == '\0')
    {
        return;
    }
    const char *words[CLI_LAUNCH_ARGS];
    for (unsigned int at = 0u; at < launch->args; at += 1u)
    {
        words[at] = EMBED_CALL(cli_command_line_table.launch_arg, CliCommandLineCfg, .launch = launch, .at = at);
    }
    (void)EMBED_CALL(cli_model_commands.command_named, CliModelCtx, .path = launch->command, .words = words,
                     .at = launch->args);
}
