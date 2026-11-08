// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// reach.c: the windows stored in orior's home, each in a file of its own, written beside and moved into place whole;
// and a command sent to the window open on a tree, its answer waited for on a channel of the command's own
#include "reach.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"

#include "../comms/comms.h"
#include "../comms/door.h"

#include "../text/text.h"
#include "embed_turnstile.h"

// the most windows stored that are read, and how long a command line waits for the window's answer in milliseconds
#define CLI_REACH_WINDOWS 64u
#define CLI_REACH_WAIT 10000ULL
// the milliseconds a held command's answer is waited for: a week
#define CLI_REACH_HELD_WAIT 604800000ULL

// A window stored: its process, the tree it has open, its door's address, and its key
typedef struct
{
    unsigned int process;
    char root[CLI_PATH_MAX_LEN];
    int has_root;
    char address[CLI_PATH_MAX_LEN];
    char key[CLI_NAME_MAX_LEN];
} CliReachEntry;

// The windows stored, read from the folder: where each goes, and how many there are
typedef struct
{
    const char *folder;
    CliReachEntry *entries;
    unsigned int count;
} CliReachRead;

static CliReachEntry s_entries[CLI_REACH_WINDOWS];

typedef struct
{
    const char *home;
    char *folder;
    const char *folder_read;
    const char *before;
    unsigned int process;
    char *path;
    const char *one;
    const char *other;
    CliIpcClient *client;
    const char *menu;
    const char *command;
    const char *const *request;
    unsigned int count;
    CliReachAnswer *answer;
    VerbaLinea *err_msg;
} ReachCtx;
// the folder windows are stored in, under `home`, into `folder`
EMBED_INLINE void reach_folder(const ReachCtx *args)
{
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->home, .relative = "windows", .joined = args->folder);
}

// the file of the window of `process` in `folder`, its name begun by `before`, into `path`
EMBED_INLINE void reach_file(const ReachCtx *args)
{
    char name[64];
    VerbaLinea named = {name, sizeof(name), 0ULL};
    mmgr_line_put(&named, args->before);
    mmgr_line_decimal(&named, args->process, 1u);
    mmgr_line_put(&named, ".window");
    mmgr_line_finish(&named);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->folder_read, .relative = name, .joined = args->path);
}

int cli_reach_store_in(const CliReachCfg *args)
{
    char folder[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char adjacent[CLI_PATH_MAX_LEN];
    char text[CLI_PATH_MAX_LEN * 3u];
    EMBED_CALL(reach_folder, ReachCtx, .home = args->home, .folder = folder);
    if (!EMBED_CALL(cli_words_err_msg_internal.private_folder_init, CliWordsErrMsgCtx, .folder = folder))
    {
        mmgr_line_put(args->err_msg, folder);
        mmgr_line_put(args->err_msg, ": the folder could not be made the reader's alone");
        return 0;
    }
    EMBED_CALL(reach_file, ReachCtx, .folder_read = folder, .before = "", .process = args->process, .path = path);
    EMBED_CALL(reach_file, ReachCtx, .folder_read = folder, .before = ".", .process = args->process, .path = adjacent);
    VerbaLinea out = {text, sizeof(text), 0ULL};
    mmgr_line_decimal(&out, args->process, 1u);
    mmgr_line_ch(&out, '\0');
    mmgr_line_put(&out, (args->root != NULL) ? args->root : "");
    mmgr_line_ch(&out, '\0');
    mmgr_line_put(&out, args->address);
    mmgr_line_ch(&out, '\0');
    mmgr_line_put(&out, args->key);
    mmgr_line_ch(&out, '\0');
    if (!mmgr_line_ok(&out) ||
        !EMBED_CALL(cli_path_bytes_internal.private_file_write, CliPathBytesCtx, .path = adjacent, .bytes = out.out,
                    .size = out.at) ||
        !EMBED_CALL(cli_err_msg_from_internal.move, CliErrMsgFromCtx, .from_text = adjacent, .to = path))
    {
        mmgr_line_put(args->err_msg, folder);
        mmgr_line_put(args->err_msg, ": the window's file could not be written");
        return 0;
    }
    return 1;
}

int cli_reach_store(const CliReachCfg *args)
{
    char home[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = home))
    {
        mmgr_line_put(args->err_msg, "orior_cli has no home folder");
        return 0;
    }
    return EMBED_CALL(cli_reach.store_in, CliReachCfg, .home = home, .process = cli_process_own(), .root = args->root,
                      .address = args->address, .key = args->key, .err_msg = args->err_msg);
}

void cli_reach_forget(void)
{
    char home[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = home))
    {
        EMBED_CALL(reach_folder, ReachCtx, .home = home, .folder = folder);
        EMBED_CALL(reach_file, ReachCtx, .folder_read = folder, .before = "", .process = cli_process_own(),
                   .path = path);
        (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = path);
    }
}

// one file of the folder read where it names a window stored, ending in .window and not beginning with a dot
static void reach_entry(const CliEachNameIsFolderCfg *args)
{
    const char *name = args->name;
    CliReachRead *const read = (CliReachRead *)args->context;
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES});
    if (args->is_folder || (name[0] == '.') || (length < 7u) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = name + length - 7u, .other = ".window", .cap = CLI_POOL_BYTES})) ||
        (read->count == CLI_REACH_WINDOWS))
    {
        return;
    }
    char path[CLI_PATH_MAX_LEN];
    unsigned long long size = 0ULL;
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = read->folder, .relative = name, .joined = path);
    const char *const bytes = (const char *)EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                       .size_pointer = &size, .owner = g_cli_machine_owner);
    if (bytes == NULL)
    {
        return;
    }
    const char *field[4];
    unsigned int fields = 0u;
    unsigned long long at = 0ULL;
    while ((fields < 4u) && (at < size))
    {
        field[fields] = bytes + at;
        fields += 1u;
        at += cellul.len(&(CatenaFinitaCfg){.src = bytes + at, .cap = size - at}) + 1ULL;
    }
    if ((fields == 4u) && (at <= size) && (bytes[size - 1ULL] == '\0'))
    {
        CliReachEntry *const one = &read->entries[read->count];
        const unsigned long long digits = cellul.len(&(CatenaFinitaCfg){.src = field[0], .cap = size});
        // a process's id is no more than an unsigned int stores
        one->process = (unsigned int)EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = field[0], .at = 0ULL,
                                                .size = digits, .end = NULL);
        one->has_root = field[1][0] != '\0';
        VerbaLinea root = {one->root, sizeof(one->root), 0ULL};
        mmgr_line_put(&root, field[1]);
        VerbaLinea address = {one->address, sizeof(one->address), 0ULL};
        mmgr_line_put(&address, field[2]);
        VerbaLinea key = {one->key, sizeof(one->key), 0ULL};
        mmgr_line_put(&key, field[3]);
        if ((mmgr_line_finish(&root) != 0ULL) || !one->has_root)
        {
            read->count += ((mmgr_line_finish(&address) != 0ULL) && (mmgr_line_finish(&key) != 0ULL)) ? 1u : 0u;
        }
    }
    g_cli_guard->persistent_buf_release((void *)(uintptr_t)bytes, (g_cli_machine_owner));
}

// whether two paths name one place, as their parts compare, a separator at the end aside
EMBED_INLINE int reach_same(const ReachCtx *args)
{
    const char *one = args->one;
    const char *other = args->other;
    size_t one_length = cellul.len(&(CatenaFinitaCfg){.src = one, .cap = CLI_POOL_BYTES});
    size_t other_length = cellul.len(&(CatenaFinitaCfg){.src = other, .cap = CLI_POOL_BYTES});
    one_length = (size_t)(1u + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = one + 1u,
                                          .at = one_length - 1u, .set = "/\\"));
    other_length = (size_t)(1u + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg,
                                            .text = other + 1u, .at = other_length - 1u, .set = "/\\"));
    return (one_length == other_length) && EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = one,
                                                      .other_pointer = other, .size = one_length);
}

// the command sent to the window `client` joined, and its answer waited for on a channel of the command's own
EMBED_INLINE int reach_window(const ReachCtx *args)
{
    CliIpcClient *client = args->client;
    VerbaLinea *err_msg = args->err_msg;
    static char s_request_buf[1u << 16u];
    static char s_body[1u << 16u];
    unsigned char secret[8];
    char id[24];
    char channel[64];
    if (!EMBED_CALL(cli_size_bytes_internal.random_bytes, CliSizeBytesCtx, .bytes_out = secret, .size = sizeof(secret)))
    {
        mmgr_line_put(err_msg, "the system gave no secret");
        return 0;
    }
    VerbaLinea id_line = {id, sizeof(id), 0ULL};
    for (unsigned int at = 0u; at < sizeof(secret); at += 1u)
    {
        mmgr_line_ch(&id_line, "0123456789abcdef"[secret[at] >> 4u]);
        mmgr_line_ch(&id_line, "0123456789abcdef"[secret[at] & 15u]);
    }
    mmgr_line_finish(&id_line);
    VerbaLinea channel_line = {channel, sizeof(channel), 0ULL};
    mmgr_line_put(&channel_line, "answer/");
    mmgr_line_put(&channel_line, id);
    mmgr_line_finish(&channel_line);
    if (!EMBED_CALL(cli_ipc_err_msg.listen, CliIpcErrMsgCfg, .client = client, .channel = channel, .err_msg = err_msg))
    {
        return 0;
    }
    VerbaLinea request_line = {s_request_buf, sizeof(s_request_buf), 0ULL};
    mmgr_line_put(&request_line, id);
    mmgr_line_ch(&request_line, '\0');
    mmgr_line_put(&request_line, args->menu);
    mmgr_line_ch(&request_line, '\0');
    mmgr_line_put(&request_line, args->command);
    mmgr_line_ch(&request_line, '\0');
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        mmgr_line_put(&request_line, args->request[at]);
        mmgr_line_ch(&request_line, '\0');
    }
    if (!mmgr_line_ok(&request_line))
    {
        mmgr_line_put(err_msg, "the command does not fit what is sent");
        return 0;
    }
    unsigned long long delivered = 0ULL;
    if (!EMBED_CALL(cli_ipc_err_msg.send, CliIpcErrMsgCfg, .client = client, .channel = "command",
                    .body = request_line.out, .size = request_line.at, .delivered = &delivered, .err_msg = err_msg))
    {
        return 0;
    }
    // the count shows where the silence is: 0 where no part of the window listens on command any more, more where the
    // window received and did not answer
    char from[CLI_NAME_MAX_LEN];
    char reply_channel[CLI_NAME_MAX_LEN];
    VerbaLinea body = {s_body, sizeof(s_body), 0ULL};
    if (!EMBED_CALL(cli_ipc_client.next, CliIpcClientCfg, .client = client, .wait = CLI_REACH_WAIT,
                    .channel_out = reply_channel, .from = from, .body_verba_linea_pointer = &body))
    {
        mmgr_line_put(err_msg, "the window gave no answer, received by ");
        mmgr_line_decimal(err_msg, delivered, 1u);
        return 0;
    }
    // a held command's answer comes when a person answers in the window, so the wait has no short bound
    if ((body.at != 0ULL) && (body.out[0] == CLI_REACH_HELD))
    {
        body.at = 0ULL;
        if (!EMBED_CALL(cli_ipc_client.next, CliIpcClientCfg, .client = client, .wait = CLI_REACH_HELD_WAIT,
                        .channel_out = reply_channel, .from = from, .body_verba_linea_pointer = &body))
        {
            mmgr_line_put(err_msg, "the window held the command and gave no answer");
            return 0;
        }
    }
    args->answer->done = (body.at != 0ULL) && (body.out[0] == CLI_REACH_DONE);
    VerbaLinea answer = {args->answer->text, sizeof(args->answer->text), 0ULL};
    if (body.at > 1ULL)
    {
        mmgr_line_put_n(&answer, body.out + 1, body.at - 1ULL);
    }
    if (mmgr_line_finish(&answer) == 0ULL)
    {
        args->answer->text[0] = '\0';
    }
    return 1;
}

int cli_reach_request_in(const CliReachMenuCfg *args)
{
    VerbaLinea *err_msg = args->err_msg;
    char folder[CLI_PATH_MAX_LEN];
    EMBED_CALL(reach_folder, ReachCtx, .home = args->home, .folder = folder);
    CliReachRead read = {folder, s_entries, 0u};
    (void)EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = folder, .each = reach_entry,
                     .context_bytess = &read);
    int answered = -1;
    for (unsigned int at = 0u; (at < read.count) && (answered < 0); at += 1u)
    {
        const CliReachEntry *const entry = &s_entries[at];
        // a command for a tree goes to the windows open on it; with no tree named, to the one window where there is
        // only one
        const int goes =
            (args->root != NULL)
                ? (entry->has_root && EMBED_CALL(reach_same, ReachCtx, .one = entry->root, .other = args->root))
                : (read.count == 1u);
        if (!goes)
        {
            continue;
        }
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea refused = {why, sizeof(why), 0ULL};
        CliIpcClient *const client = EMBED_CALL(cli_ipc_err_msg_address.join, CliIpcErrMsgAddressCfg,
                                                .address_read = entry->address, .key = entry->key, .err_msg = &refused);
        if (client == NULL)
        {
            char path[CLI_PATH_MAX_LEN];
            EMBED_CALL(reach_file, ReachCtx, .folder_read = folder, .before = "", .process = entry->process,
                       .path = path);
            (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = path);
            continue;
        }
        answered =
            EMBED_CALL(reach_window, ReachCtx, .client = client, .menu = args->menu, .command = args->command,
                       .request = args->request, .count = args->count, .answer = args->answer, .err_msg = err_msg);
        EMBED_CALL(cli_ipc_client.leave, CliIpcClientCfg, .client = client);
    }
    return answered;
}

int cli_reach_request(const CliReachMenuCfg *args)
{
    char home[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = home))
    {
        return -1;
    }
    return EMBED_CALL(cli_reach_menu.request_in, CliReachMenuCfg, .home = home, .root = args->root, .menu = args->menu,
                      .command = args->command, .request = args->request, .count = args->count, .answer = args->answer,
                      .err_msg = args->err_msg);
}

// the most words a command through the door carries
#define CLI_REACH_WORDS 256u

// A command come through the door, on the stack of the door's thread that read it, every text a place in the body
// that thread received, which stands as it was until the command is done: the next waiting, its id and name, its
// words, whether the window ran it, and its done word, which the door's thread sleeps on
typedef struct CliReachCommand
{
    struct CliReachCommand *next;
    const char *id;
    const char *command;
    const char *const *word;
    unsigned int words;
    int ran;
    embed_atomic_word done;
} CliReachCommand;

// The door: the commands waiting, pushed by the door's threads and taken whole by the window's thread; whether the
// door is closed; what wakes the window's thread; and this window's key, in a cell of its own
typedef struct
{
    _Alignas(64) CliReachCommand *_Atomic waiting;
    _Atomic int closed;
    void (*_Atomic woken)(void);
    char *key;
    // the id of the command whose answer the window holds, copied as its thread wakes, and whether one is held
    char held_id[CLI_NAME_MAX_LEN];
    int held;
} CliReachDoor;

static CliReachDoor s_door;

// the waiting commands taken whole, each marked done without running and its door's thread woken: what the window
// does as it closes, and what a door's thread does that finds the door closed after it pushed
static void reach_refused_all(void)
{
    CliReachCommand *command = atomic_exchange_explicit(&s_door.waiting, NULL, memory_order_acquire);
    while (command != NULL)
    {
        // the next read before the done word is set: the command's stack frame ends once its thread wakes
        CliReachCommand *const next = command->next;
        command->ran = 0;
        embed_atomic_word_store(&command->done, 1u);
        embed_word_wake(&command->done);
        command = next;
    }
}

// From a door's thread: a command read where that thread received it, pushed on the waiting with a compare and swap,
// the window's thread woken, and this thread asleep on the command's done word until the window has run and answered
// it; one not ended by a 0, short of a command or past CLI_REACH_WORDS words let go
static int reach_received(const CliCommsRecvCfg *args)
{
    unsigned int fields = 0u;
    for (unsigned long long at = 0ULL; at < args->size; fields += 1u)
    {
        at += cellul.len(&(CatenaFinitaCfg){.src = args->body + at, .cap = args->size - at}) + 1ULL;
    }
    if ((fields < CLI_REACH_HEAD_FIELDS) || (args->body[args->size - 1ULL] != '\0') ||
        ((fields - CLI_REACH_HEAD_FIELDS) > CLI_REACH_WORDS) ||
        (atomic_load_explicit(&s_door.closed, memory_order_acquire) != 0))
    {
        return 1;
    }
    const char *word[CLI_REACH_WORDS];
    CliReachCommand command;
    command.id = args->body;
    unsigned long long at = cellul.len(&(CatenaFinitaCfg){.src = args->body, .cap = args->size}) + 1ULL;
    at += cellul.len(&(CatenaFinitaCfg){.src = args->body + at, .cap = args->size - at}) + 1ULL;
    command.command = args->body + at;
    at += cellul.len(&(CatenaFinitaCfg){.src = args->body + at, .cap = args->size - at}) + 1ULL;
    command.words = fields - CLI_REACH_HEAD_FIELDS;
    for (unsigned int each = 0u; each < command.words; each += 1u)
    {
        word[each] = args->body + at;
        at += cellul.len(&(CatenaFinitaCfg){.src = args->body + at, .cap = args->size - at}) + 1ULL;
    }
    command.word = word;
    command.ran = 0;
    embed_atomic_word_store(&command.done, 0u);
    CliReachCommand *head = atomic_load_explicit(&s_door.waiting, memory_order_relaxed);
    // the compare and swap is the push: it fails where another door's thread pushed first, and the link is set again
    do
    {
        command.next = head;
    } while (!atomic_compare_exchange_weak_explicit(&s_door.waiting, &head, &command, memory_order_seq_cst,
                                                    memory_order_relaxed));
    // a door closed as this pushed: the waiting taken here, so no command stands on the list with no one to mark it
    if (atomic_load_explicit(&s_door.closed, memory_order_seq_cst) != 0)
    {
        reach_refused_all();
    }
    void (*const woken)(void) = atomic_load_explicit(&s_door.woken, memory_order_acquire);
    if (woken != NULL)
    {
        woken();
    }
    while (embed_atomic_word_load(&command.done) == 0u)
    {
        (void)embed_word_wait(&command.done, 0u, EMBED_WAIT_FOREVER);
    }
    return 1;
}

int cli_reach_open(const char *root, void (*woken)(void), VerbaLinea *err_msg)
{
    char address[CLI_PATH_MAX_LEN];
    char line_key[CLI_COMMS_KEY];
    atomic_store_explicit(&s_door.waiting, NULL, memory_order_relaxed);
    atomic_store_explicit(&s_door.woken, woken, memory_order_relaxed);
    atomic_store_explicit(&s_door.closed, 0, memory_order_release);
    s_door.key = g_cli_guard->persistent_buf_alloc(CLI_COMMS_KEY, g_cli_machine_owner);
    if (s_door.key == NULL)
    {
        mmgr_line_put(err_msg, "the cells cannot meet the door's key");
        return 0;
    }
    static const char *const s_listen[1] = {"command"};
    static const char *const s_send[1] = {"answer/*"};
    unsigned int listener = 0u;
    // the door is open from the start, for every process the container starts to join; the part that takes the commands
    // is one of this process alone, whose key the door never takes, and the key the command line has grants it the
    // command's channel
    return EMBED_CALL(cli_ipc_address.open, CliIpcAddressCfg, .address = address, .err_msg = err_msg) &&
           EMBED_CALL(cli_comms_name.comms_register, CliCommsNameCfg, .name = "orior", .has_process = 0, .process = 0u,
                      .listen = s_listen, .listens = 1u, .send_grant = s_send, .send_grants = 1u, .key_out = s_door.key,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_comms_channel.listen, CliCommsChannelCfg, .key = s_door.key, .channel = "command",
                      .queued = 0, .recv = reach_received, .context = NULL, .listener = &listener,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_comms_name.register_persistent, CliCommsNameCfg, .name = "command line", .listen = s_send,
                      .listens = 1u, .send_grant = s_listen, .send_grants = 1u, .key_out = line_key,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_reach.store, CliReachCfg, .root = root, .address = address, .key = line_key,
                      .err_msg = err_msg);
}

// an answer sent on a command's own channel: the byte `mark`, then `text` where it is not NULL
static void reach_answer_text(const char *id, char mark, const char *text)
{
    char channel[CLI_NAME_MAX_LEN];
    VerbaLinea channel_line = {channel, sizeof(channel), 0ULL};
    mmgr_line_put(&channel_line, "answer/");
    mmgr_line_put(&channel_line, id);
    (void)mmgr_line_finish(&channel_line);
    char sent[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea body = {sent, sizeof(sent), 0ULL};
    mmgr_line_ch(&body, mark);
    if (text != NULL)
    {
        mmgr_line_put(&body, text);
    }
    char quiet[256];
    VerbaLinea err_msg = {quiet, sizeof(quiet), 0ULL};
    unsigned long long delivered = 0ULL;
    (void)EMBED_CALL(cli_comms_channel_err_msg.send, CliCommsChannelErrMsgCfg, .key = s_door.key, .channel = channel,
                     .body = sent, .size = mmgr_line_ok(&body) ? body.at : 1ULL, .delivered = &delivered,
                     .err_msg = &err_msg);
}

// what a command came to, sent on its own channel: `1` where it ran, else `0` and why
static void reach_answer(const CliReachCommand *command, int ran)
{
    if (ran)
    {
        reach_answer_text(command->id, CLI_REACH_DONE, NULL);
        return;
    }
    char why[CLI_NAME_MAX_LEN + 64u];
    VerbaLinea line = {why, sizeof(why), 0ULL};
    mmgr_line_put(&line, command->command);
    mmgr_line_put(&line, " is not a command of the window's");
    (void)mmgr_line_finish(&line);
    reach_answer_text(command->id, CLI_REACH_REFUSED, why);
}

// a command's thread woken: its command done, or its answer held
static void reach_wake(CliReachCommand *command)
{
    embed_atomic_word_store(&command->done, 1u);
    embed_word_wake(&command->done);
}

int cli_reach_take(int (*run)(const char *command, const char *const *words, unsigned int count))
{
    CliReachCommand *waiting = atomic_exchange_explicit(&s_door.waiting, NULL, memory_order_acquire);
    if (waiting == NULL)
    {
        return 0;
    }
    // the list stands newest first: turned over, each runs in the order it came
    CliReachCommand *first = NULL;
    while (waiting != NULL)
    {
        CliReachCommand *const next = waiting->next;
        waiting->next = first;
        first = waiting;
        waiting = next;
    }
    while (first != NULL)
    {
        // the next read before the done word is set: the command's stack frame ends once its thread wakes
        CliReachCommand *const next = first->next;
        first->ran = run(first->command, first->word, first->words);
        // A held command keeps its id, and the held byte tells its requester to wait on. Its thread wakes at once, as
        // the door's answer to the sending process waits on that thread. The window holds one command at a time, so a
        // second hold is refused
        if ((first->ran == CLI_REACH_HOLD) && !s_door.held)
        {
            VerbaLinea id_line = {s_door.held_id, sizeof(s_door.held_id), 0ULL};
            mmgr_line_put(&id_line, first->id);
            s_door.held = mmgr_line_finish(&id_line) != 0ULL;
            reach_answer_text(first->id, s_door.held ? CLI_REACH_HELD : CLI_REACH_REFUSED,
                              s_door.held ? NULL : "the command's id does not fit what the window keeps");
        }
        else
        {
            reach_answer(first, first->ran == 1);
        }
        reach_wake(first);
        first = next;
    }
    return 1;
}

void cli_reach_held_answer(int done, const char *text)
{
    if (!s_door.held)
    {
        return;
    }
    s_door.held = 0;
    reach_answer_text(s_door.held_id, done ? CLI_REACH_DONE : CLI_REACH_REFUSED, text);
}

void cli_reach_close(void)
{
    atomic_store_explicit(&s_door.closed, 1, memory_order_seq_cst);
    cli_reach_held_answer(0, "the window closed before an answer");
    reach_refused_all();
    atomic_store_explicit(&s_door.woken, NULL, memory_order_release);
    g_cli_guard->persistent_buf_release(s_door.key, g_cli_machine_owner);
    s_door.key = NULL;
}
