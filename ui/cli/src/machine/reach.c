// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// reach.c: the windows stored in orior's home, each in a file of its own, written beside and moved into place whole;
// and a command sent to the window open on a tree, its answer waited for on a channel of the command's own
#include "reach.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../command_line/rings.h"

#include "../comms/comms.h"
#include "../comms/interface.h"

#include "../text/text.h"

// the max windows stored that are read, and how long a command line waits for the window's answer in milliseconds
#define CLI_REACH_WINDOWS 64u
#define CLI_REACH_WAIT 10000ULL
// the milliseconds a held command's answer is waited for: a week
#define CLI_REACH_HELD_WAIT 604800000ULL

// A window stored: its process, the tree it has open, its interface's address, and its key
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
        mmgr_line_put(args->err_msg, "impero has no home folder");
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
    const embed_word length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES});
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
    g_cli_guard->persistent_buf_release((void *)(embed_word)bytes, (g_cli_machine_owner));
}

// whether two paths name one place, as their parts compare, a separator at the end aside
EMBED_INLINE int reach_same(const ReachCtx *args)
{
    const char *one = args->one;
    const char *other = args->other;
    embed_word one_length = cellul.len(&(CatenaFinitaCfg){.src = one, .cap = CLI_POOL_BYTES});
    embed_word other_length = cellul.len(&(CatenaFinitaCfg){.src = other, .cap = CLI_POOL_BYTES});
    one_length = (embed_word)(1u + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg,
                                              .text = one + 1u, .at = one_length - 1u, .set = "/\\"));
    other_length = (embed_word)(1u + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg,
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

// the max words a command through the interface carries
#define CLI_REACH_WORDS 256u

// A command come through the interface, on the stack of the interface's thread that read it, every text a place in the body
// that thread received, which is as it was until the command is done: its id and name, its words, whether the
// window ran it, and its done word, which the interface's thread sleeps on
typedef struct
{
    const char *id;
    const char *command;
    const char *const *word;
    unsigned int words;
    int ran;
    embed_atomic_word done;
} CliReachCommand;

// The interface: the ticket ring the interface's threads push their commands on and the window's thread pops them off in the
// order of their tickets, its turnstiles, its slots the interface's part of the rings block; the next ticket a interface's thread
// takes and the next the window pops; the wake a interface's thread sleeps on while every slot is in flight; whether the
// interface is closed; what wakes the window's thread; and this window's key, in a cell of its own
typedef struct
{
    EmbedTurnstiles turnstiles;
    EmbedTicketRing ring;
    embed_atomic_u32 ticket;
    embed_atomic_u32 taken;
    CliWake room;
    embed_atomic_u32 closed;
    embed_atomic_uptr woken;
    char key[CLI_COMMS_KEY];
    // the id of the command whose answer the window holds, copied as its thread wakes, and whether one is held
    char held_id[CLI_NAME_MAX_LEN];
    int held;
} CliReachInterface;

static CliReachInterface s_interface;

// the wake function read from its word
EMBED_INLINE void (*reach_woken(embed_atomic_uptr_value word))(void)
{
    // Explicit cast reads the word as the function pointer stored in it
    return (void (*)(void))(embed_word)word;
}

// a command's thread woken: its command done, or its answer held
static void reach_wake(CliReachCommand *command)
{
    embed_atomic_word_store(&command->done, 1u);
    embed_word_wake(&command->done);
}

// The next command popped off the ring, into `command_out` with its stamp into `stamp_out`: 1 where the slot of the
// next ticket carries that ticket with its push landed and this pass popped it, else 0, which is where no command
// waits, where the next is granted and not yet pushed, or where another pass popped it first
static int reach_popped(CliReachCommand **command_out, embed_u64 *stamp_out)
{
    if (!cli_ticket_pushed(&s_interface.ring, embed_atomic_u32_load(&s_interface.taken), stamp_out))
    {
        return 0;
    }
    void *job = NULL;
    embed_u64 seen = 0u;
    // Explicit cast carries the interface's address as the pass's entry
    if (!embed_ticket_ring_popped(&s_interface.turnstiles, (embed_atomic_uptr_value)(embed_word)&s_interface, &s_interface.ring,
                                  *stamp_out, &job, &seen))
    {
        return 0;
    }
    *command_out = job;
    return 1;
}

// a popped command's slot released once every pass begun before it has ended, the window's next ticket moved past it
// and a interface's thread waiting for room woken
static void reach_released(embed_u64 stamp)
{
    embed_u64 seen = 0u;
    (void)embed_ticket_ring_released(&s_interface.turnstiles, &s_interface.ring, stamp, &seen);
    embed_atomic_u32_store(&s_interface.taken, embed_ticket_of_stamp(stamp) + 1u);
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_interface.room);
}

// every command pushed popped and marked done without running, its interface's thread woken: what the window does as it
// closes, and what a interface's thread does that finds the interface closed after it pushed
static void reach_refused_all(void)
{
    CliReachCommand *command = NULL;
    embed_u64 stamp = 0u;
    while (reach_popped(&command, &stamp))
    {
        command->ran = 0;
        reach_released(stamp);
        reach_wake(command);
    }
}

// From a interface's thread: a command read where that thread received it, its ticket taken and the slot of the ticket
// granted once the window has released the command CLI_REACH_RING_SLOTS tickets back, the command pushed, the
// window's thread woken, and this thread asleep on the command's done word until the window has run and answered
// it; one not ended by a 0, short of a command or past CLI_REACH_WORDS words let go
static int reach_received(const CliCommsRecvCfg *args)
{
    unsigned int fields = 0u;
    for (unsigned long long at = 0ULL; at < args->size; fields += 1u)
    {
        at += cellul.len(&(CatenaFinitaCfg){.src = args->body + at, .cap = args->size - at}) + 1ULL;
    }
    if ((fields < CLI_REACH_HEAD_FIELDS) || (args->body[args->size - 1ULL] != '\0') ||
        ((fields - CLI_REACH_HEAD_FIELDS) > CLI_REACH_WORDS) || (embed_atomic_u32_load(&s_interface.closed) != 0u))
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
    const embed_u32 ticket = EMBED_ATOMIC_ADD_32(&s_interface.ticket, 1u);
    const embed_u64 stamp = embed_ticket_stamp(cli_ticket_epoch(), ticket);
    // Explicit cast carries the command's address as the pass's entry
    const embed_atomic_uptr_value address = (embed_atomic_uptr_value)(embed_word)&command;
    embed_u64 seen = 0u;
    for (;;)
    {
        const unsigned int room = EMBED_CALL(cli_wake_internal.wake_read, CliWakeCtx, .wake = &s_interface.room);
        if (cli_ticket_room(&s_interface.ring, ticket) &&
            embed_ticket_ring_granted(&s_interface.turnstiles, address, &s_interface.ring, stamp, &seen))
        {
            break;
        }
        if (embed_atomic_u32_load(&s_interface.closed) != 0u)
        {
            return 1;
        }
        EMBED_CALL(cli_wake_internal.wake_wait_seen, CliWakeCtx, .wake = &s_interface.room, .seen = room);
    }
    (void)embed_ticket_ring_pushed(&s_interface.turnstiles, address, &s_interface.ring, stamp, &command, &seen);
    // a interface closed as this pushed: the ring popped here, so no command is on it with no one to mark it
    if (embed_atomic_u32_load(&s_interface.closed) != 0u)
    {
        reach_refused_all();
    }
    void (*const woken)(void) = reach_woken(embed_atomic_uptr_load(&s_interface.woken));
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
    memor.set(&(MemoriaCfg){.dst = &s_interface.turnstiles, .val = 0, .bytes = sizeof(s_interface.turnstiles)});
    EmbedTicketRingSlot *const slots =
        (EmbedTicketRingSlot *)(void *)EMBED_CALL(cli_rings.part, CliRingsPartCfg, .part = CLI_RINGS_REACH);
    memor.set(&(MemoriaCfg){.dst = slots, .val = 0, .bytes = CLI_REACH_RING_BYTES});
    embed_ticket_ring_begin(&s_interface.ring, slots, CLI_REACH_RING_SHIFT);
    embed_atomic_u32_store(&s_interface.ticket, 0u);
    embed_atomic_u32_store(&s_interface.taken, 0u);
    s_interface.room = (CliWake)CLI_WAKE_INIT;
    // Explicit cast stores the function pointer as the word that holds it
    embed_atomic_uptr_store(&s_interface.woken, (embed_atomic_uptr_value)(embed_word)woken);
    embed_atomic_u32_store(&s_interface.closed, 0u);
    s_interface.key[0] = '\0';
    static const char *const s_listen[1] = {"command"};
    static const char *const s_send[1] = {"answer/*"};
    unsigned int listener = 0u;
    // the interface is open from the start, for every process the container starts to join; the part that takes the commands
    // is one of this process alone, whose key the interface never takes, and the key the command line has grants it the
    // command's channel
    return EMBED_CALL(cli_ipc_address.open, CliIpcAddressCfg, .address = address, .err_msg = err_msg) &&
           EMBED_CALL(cli_comms_name.comms_register, CliCommsNameCfg, .name = "orior", .has_process = 0, .process = 0u,
                      .listen = s_listen, .listens = 1u, .send_grant = s_send, .send_grants = 1u, .key_out = s_interface.key,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_comms_channel.listen, CliCommsChannelCfg, .key = s_interface.key, .channel = "command",
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
    (void)EMBED_CALL(cli_comms_channel_err_msg.send, CliCommsChannelErrMsgCfg, .key = s_interface.key, .channel = channel,
                     .body = sent, .size = mmgr_line_ok(&body) ? body.at : 1ULL, .delivered = &delivered,
                     .err_msg = &err_msg);
}

// what a command came to, sent on its own channel: `1` where it ran, else `0` and why: the window has no such command,
// or the window refused it
static void reach_answer(const CliReachCommand *command)
{
    if (command->ran == 1)
    {
        reach_answer_text(command->id, CLI_REACH_DONE, NULL);
        return;
    }
    char why[CLI_NAME_MAX_LEN + 64u];
    VerbaLinea line = {why, sizeof(why), 0ULL};
    if (command->ran == CLI_REACH_NONE)
    {
        mmgr_line_put(&line, command->command);
        mmgr_line_put(&line, " is not a command of the window's");
    }
    else
    {
        mmgr_line_put(&line, "the window refused ");
        mmgr_line_put(&line, command->command);
    }
    (void)mmgr_line_finish(&line);
    reach_answer_text(command->id, CLI_REACH_REFUSED, why);
}

int cli_reach_take(int (*run)(const char *command, const char *const *words, unsigned int count))
{
    int taken = 0;
    CliReachCommand *command = NULL;
    embed_u64 stamp = 0u;
    // each command runs in the order of its ticket, and its slot goes back before its thread wakes, as the command's
    // stack frame ends once it does
    while (reach_popped(&command, &stamp))
    {
        taken = 1;
        command->ran = run(command->command, command->word, command->words);
        // A held command retains its id, and the held byte tells its requester to wait on. Its thread wakes at once, as
        // the interface's answer to the sending process waits on that thread. The window holds one command at a time, so a
        // second hold is refused
        if ((command->ran == CLI_REACH_HOLD) && !s_interface.held)
        {
            VerbaLinea id_line = {s_interface.held_id, sizeof(s_interface.held_id), 0ULL};
            mmgr_line_put(&id_line, command->id);
            s_interface.held = mmgr_line_finish(&id_line) != 0ULL;
            reach_answer_text(command->id, s_interface.held ? CLI_REACH_HELD : CLI_REACH_REFUSED,
                              s_interface.held ? NULL : "the command's id does not fit what the window retains");
        }
        else
        {
            reach_answer(command);
        }
        reach_released(stamp);
        reach_wake(command);
    }
    return taken;
}

void cli_reach_held_answer(int done, const char *text)
{
    if (!s_interface.held)
    {
        return;
    }
    s_interface.held = 0;
    reach_answer_text(s_interface.held_id, done ? CLI_REACH_DONE : CLI_REACH_REFUSED, text);
}

void cli_reach_close(void)
{
    embed_atomic_u32_store(&s_interface.closed, 1u);
    EMBED_ATOMIC_ORDERED();
    cli_reach_held_answer(0, "the window closed before an answer");
    reach_refused_all();
    embed_atomic_uptr_store(&s_interface.woken, 0u);
    s_interface.key[0] = '\0';
}
