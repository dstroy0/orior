// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// reach.c: the windows stored in orior's home, each in a file of its own, written beside and moved into place whole;
// and a command sent to the window open on a tree, its answer waited for on a channel of the command's own
#include "reach.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"

#include "../comms/door.h"

#include "../text/text.h"

// the most windows stored that are read, and how long a command line waits for the window's answer in milliseconds
#define CLI_REACH_WINDOWS 64u
#define CLI_REACH_WAIT 10000ULL

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
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    if (args->is_folder || (name[0] == '.') || (length < 7u) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = name + length - 7u, .other = ".window", .cap = CLI_CELLS_POOL})) ||
        (read->count == CLI_REACH_WINDOWS))
    {
        return;
    }
    char path[CLI_PATH_MAX_LEN];
    unsigned long long size = 0ULL;
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = read->folder, .relative = name, .joined = path);
    const char *const bytes = (const char *)EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                       .size_pointer = &size, .owner = &g_cli_machine_cells);
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
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_machine_cells);
}

// whether two paths name one place, as their parts compare, a separator at the end aside
EMBED_INLINE int reach_same(const ReachCtx *args)
{
    const char *one = args->one;
    const char *other = args->other;
    size_t one_length = cellul.len(&(CatenaFinitaCfg){.src = one, .cap = CLI_CELLS_POOL});
    size_t other_length = cellul.len(&(CatenaFinitaCfg){.src = other, .cap = CLI_CELLS_POOL});
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
