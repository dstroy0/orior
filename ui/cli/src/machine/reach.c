// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// reach.c: the windows stored in orior's home, each in a file of its own, written beside and moved into place whole;
// and a command sent to the window open on a tree, its answer waited for on a channel of the command's own
#include "reach.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"

#include "../comms/door.h"
#include "../servers/lsp.h"
#include "../text/json_tree.h"

#include "../text/text.h"

// the most windows stored that are read, how long a command line waits for the window's answer in milliseconds, and the
// nodes and bytes of one stored file read
#define CLI_REACH_WINDOWS 64u
#define CLI_REACH_WAIT 10000ull
#define CLI_REACH_NODES 256u
#define CLI_REACH_TEXT 8192u

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
static CliLock s_lock = CLI_LOCK_INIT;

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
    CliDoorClient *client;
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
    VerbaLinea named = {name, sizeof(name), 0ull};
    mmgr_line_put(&named, args->before);
    mmgr_line_decimal(&named, args->process, 1u);
    mmgr_line_put(&named, ".json");
    mmgr_line_finish(&named);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->folder_read, .relative = name, .joined = args->path);
}

int cli_reach_store_in(const CliReachCfg *args)
{
    char folder[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char beside[CLI_PATH_MAX_LEN];
    char text[CLI_PATH_MAX_LEN * 3u];
    EMBED_CALL(reach_folder, ReachCtx, .home = args->home, .folder = folder);
    if (!EMBED_CALL(cli_words_err_msg_internal.private_folder_init, CliWordsErrMsgCtx, .folder = folder))
    {
        mmgr_line_put(args->err_msg, folder);
        mmgr_line_put(args->err_msg, ": the folder could not be made the reader's alone");
        return 0;
    }
    EMBED_CALL(reach_file, ReachCtx, .folder_read = folder, .before = "", .process = args->process, .path = path);
    EMBED_CALL(reach_file, ReachCtx, .folder_read = folder, .before = ".", .process = args->process, .path = beside);
    VerbaLinea out = {text, sizeof(text), 0ull};
    mmgr_line_put(&out, "{\"process\":");
    mmgr_line_decimal(&out, args->process, 1u);
    mmgr_line_put(&out, ",\"root\":");
    if (args->root != NULL)
    {
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &out,
                   .text_bytes = (const unsigned char *)args->root,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        mmgr_line_put(&out, "null");
    }
    mmgr_line_put(&out, ",\"address\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &out,
               .text_bytes = (const unsigned char *)args->address,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->address, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&out, ",\"key\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &out,
               .text_bytes = (const unsigned char *)args->key,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(&out, '}');
    if (!mmgr_line_ok(&out) ||
        !EMBED_CALL(cli_path_bytes_internal.private_file_write, CliPathBytesCtx, .path = beside, .bytes = out.out,
                    .size = out.at) ||
        !EMBED_CALL(cli_err_msg_from_internal.move, CliErrMsgFromCtx, .from_text = beside, .to = path))
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

// one file of the folder read where it names a window stored, ending in .json and not beginning with a dot
static void reach_entry(const CliEachNameIsFolderCfg *args)
{
    const char *name = args->name;
    CliReachRead *const read = (CliReachRead *)args->context;
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    if (args->is_folder || (name[0] == '.') || (length < 5u) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = name + length - 5u, .other = ".json", .cap = CLI_CELLS_POOL})) ||
        (read->count == CLI_REACH_WINDOWS))
    {
        return;
    }
    static CliJsonNode s_nodes[CLI_REACH_NODES];
    static CfgJsonToken s_tokens[CLI_REACH_NODES];
    static char s_text[CLI_REACH_TEXT];
    char path[CLI_PATH_MAX_LEN];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    unsigned long long size = 0ull;
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = read->folder, .relative = name, .joined = path);
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = path,
                                                  .size_pointer = &size, .owner = &g_cli_machine_cells);
    if (bytes == NULL)
    {
        return;
    }
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = s_nodes, .nodes_most = CLI_REACH_NODES, .text_out = s_text,
               .text_most = sizeof(s_text));
    const unsigned int entry_count =
        EMBED_CALL(cli_json_tree_text_size_tokens.json_read, CliJsonTreeTextSizeTokensCfg, .json = &json,
                   .text = (const char *)bytes, .size = size, .tokens = s_tokens, .tokens_most = CLI_REACH_NODES,
                   .err_msg = &err_msg);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_machine_cells);
    const char *const address =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                      .object = entry_count, .name = "address"),
                   .length_pointer = NULL);
    const char *const key =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                      .object = entry_count, .name = "key"),
                   .length_pointer = NULL);
    const char *const root =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                      .object = entry_count, .name = "root"),
                   .length_pointer = NULL);
    const unsigned int process = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                            .object = entry_count, .name = "process");
    const char *const digits = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json,
                                          .node = process, .kind = CFG_JSON_NUMBER)
                                   ? json.text + json.node[process].text
                                   : NULL;
    const unsigned int root_node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = &json, .object = entry_count, .name = "root");
    if ((address == NULL) || (key == NULL) || (digits == NULL) || (digits[0] == '-') ||
        ((root == NULL) && !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json,
                                       .node = root_node, .kind = CFG_JSON_NULL)))
    {
        return;
    }
    CliReachEntry *const one = &read->entries[read->count];
    one->process = 0u;
    // a process's id is no more than an unsigned int stores
    one->process = (unsigned int)EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = digits, .at = 0ull,
                                            .size = json.node[process].length, .end = NULL);
    one->has_root = (root != NULL);
    EMBED_CALL(cli_text_out.copied_or, CliTextOutCfg, .out_room = one->root, .text = root, .otherwise = "");
    memor.cpy(&(MemoriaCfg){.dst = one->address,
                            .src = address,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = address, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){
        .dst = one->key, .src = key, .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u});
    read->count += 1u;
}

// whether two paths name one place, as their parts compare, a separator at the end aside
EMBED_INLINE int reach_same(const ReachCtx *args)
{
    const char *one = args->one;
    const char *other = args->other;
    size_t one_length = cellul.len(&(CatenaFinitaCfg){.src = one, .cap = CLI_CELLS_POOL});
    size_t other_length = cellul.len(&(CatenaFinitaCfg){.src = other, .cap = CLI_CELLS_POOL});
    one_length = (size_t)(1u + EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg, .text = one + 1u,
                                          .at = one_length - 1u, .set = "/\\"));
    other_length = (size_t)(1u + EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg,
                                            .text = other + 1u, .at = other_length - 1u, .set = "/\\"));
    return (one_length == other_length) && EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = one,
                                                      .other_pointer = other, .size = one_length);
}

// the command sent to the window `client` joined, and its answer waited for on a channel of the command's own
EMBED_INLINE int reach_window(const ReachCtx *args)
{
    CliDoorClient *client = args->client;
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
    VerbaLinea id_line = {id, sizeof(id), 0ull};
    for (unsigned int at = 0u; at < sizeof(secret); at += 1u)
    {
        mmgr_line_ch(&id_line, "0123456789abcdef"[secret[at] >> 4u]);
        mmgr_line_ch(&id_line, "0123456789abcdef"[secret[at] & 15u]);
    }
    mmgr_line_finish(&id_line);
    VerbaLinea channel_line = {channel, sizeof(channel), 0ull};
    mmgr_line_put(&channel_line, "answer/");
    mmgr_line_put(&channel_line, id);
    mmgr_line_finish(&channel_line);
    if (!EMBED_CALL(cli_door_err_msg.listen, CliDoorErrMsgCfg, .client = client, .channel = channel,
                    .err_msg = err_msg))
    {
        return 0;
    }
    VerbaLinea request_line = {s_request_buf, sizeof(s_request_buf), 0ull};
    mmgr_line_put(&request_line, "{\"id\":\"");
    mmgr_line_put(&request_line, id);
    mmgr_line_put(&request_line, "\",\"menu\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &request_line,
               .text_bytes = (const unsigned char *)args->menu,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->menu, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&request_line, ",\"command\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &request_line,
               .text_bytes = (const unsigned char *)args->command,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->command, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&request_line, ",\"args\":[");
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        mmgr_line_put(&request_line, (at > 0u) ? "," : "");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &request_line,
                   .text_bytes = (const unsigned char *)args->request[at],
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->request[at], .cap = CLI_CELLS_POOL}));
    }
    mmgr_line_put(&request_line, "]}");
    unsigned long long delivered = 0ull;
    if (!EMBED_CALL(cli_door_err_msg.send, CliDoorErrMsgCfg, .client = client, .channel = "command",
                    .body = request_line.out, .size = request_line.at, .delivered = &delivered, .err_msg = err_msg))
    {
        return 0;
    }
    // the count shows where the silence is: 0 where no part of the window listens on command any more, more where the
    // page received and did not answer
    char from[CLI_NAME_MAX_LEN];
    char reply_channel[CLI_NAME_MAX_LEN];
    VerbaLinea body = {s_body, sizeof(s_body), 0ull};
    if (!EMBED_CALL(cli_door_client.next, CliDoorClientCfg, .client = client, .wait = CLI_REACH_WAIT,
                    .channel_out = reply_channel, .from = from, .body_verba_linea_pointer = &body))
    {
        mmgr_line_put(err_msg, "the window gave no answer, received by ");
        mmgr_line_decimal(err_msg, delivered, 1u);
        return 0;
    }
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    args->answer->done = 0;
    args->answer->text[0] = '\0';
    if (EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = body.out, .size_value = body.at, .name = "done",
                   .start = &start, .length_pointer = &length))
    {
        args->answer->done =
            (length == 4ull) && (memor.cmp(&(MemoriaCfg){.src = body.out + start, .other = "true", .bytes = 4u}) == 0);
    }
    if (EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = body.out, .size_value = body.at, .name = "message",
                   .start = &start, .length_pointer = &length))
    {
        (void)EMBED_CALL(cli_lsp_room.string, CliLspRoomCfg, .text = body.out + start, .length = length,
                         .out_room = args->answer->text, .room = sizeof(args->answer->text));
    }
    return 1;
}

int cli_reach_request_in(const CliReachMenuCfg *args)
{
    VerbaLinea *err_msg = args->err_msg;
    char folder[CLI_PATH_MAX_LEN];
    EMBED_CALL(reach_folder, ReachCtx, .home = args->home, .folder = folder);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
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
        VerbaLinea refused = {why, sizeof(why), 0ull};
        CliDoorClient *const client =
            EMBED_CALL(cli_door_err_msg_address.join, CliDoorErrMsgAddressCfg, .address_read = entry->address,
                       .key = entry->key, .err_msg = &refused);
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
        EMBED_CALL(cli_door_client.leave, CliDoorClientCfg, .client = client);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
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
