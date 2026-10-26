// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// reach.c: the windows stored in orior's home, each in a file of its own, written beside and moved into place whole;
// and a command sent to the window open on a tree, its answer waited for on a channel of the command's own
#include "reach.h"

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

// the folder windows are stored in, under `home`, into `folder`
static void reach_folder(const char *home, char *folder)
{
    cli_full(home, "windows", folder);
}

// the file of the window of `process` in `folder`, its name begun by `before`, into `path`
static void reach_file(const char *folder, const char *before, unsigned int process, char *path)
{
    char name[64];
    VerbaLinea named = {name, sizeof(name), 0ull};
    mmgr_line_put(&named, before);
    mmgr_line_decimal(&named, process, 1u);
    mmgr_line_put(&named, ".json");
    mmgr_line_finish(&named);
    cli_full(folder, name, path);
}

int cli_reach_store_in(const char *home, unsigned int process, const char *root, const char *address, const char *key,
                       VerbaLinea *err_msg)
{
    char folder[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char beside[CLI_PATH_MAX_LEN];
    char text[CLI_PATH_MAX_LEN * 3u];
    reach_folder(home, folder);
    if (!cli_private_folder_made(folder))
    {
        mmgr_line_put(err_msg, folder);
        mmgr_line_put(err_msg, ": the folder could not be made the reader's alone");
        return 0;
    }
    reach_file(folder, "", process, path);
    reach_file(folder, ".", process, beside);
    VerbaLinea out = {text, sizeof(text), 0ull};
    mmgr_line_put(&out, "{\"process\":");
    mmgr_line_decimal(&out, process, 1u);
    mmgr_line_put(&out, ",\"root\":");
    if (root != NULL)
    {
        cli_json_quoted_lossy(&out, (const unsigned char *)root,
                              cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        mmgr_line_put(&out, "null");
    }
    mmgr_line_put(&out, ",\"address\":");
    cli_json_quoted_lossy(&out, (const unsigned char *)address,
                          cellul.len(&(CatenaFinitaCfg){.src = address, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&out, ",\"key\":");
    cli_json_quoted_lossy(&out, (const unsigned char *)key,
                          cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(&out, '}');
    if (!mmgr_line_ok(&out) || !cli_private_file_written(beside, out.out, out.at) || !cli_moved(beside, path))
    {
        mmgr_line_put(err_msg, folder);
        mmgr_line_put(err_msg, ": the window's file could not be written");
        return 0;
    }
    return 1;
}

int cli_reach_store(const char *root, const char *address, const char *key, VerbaLinea *err_msg)
{
    char home[CLI_PATH_MAX_LEN];
    if (!cli_home_folder(home))
    {
        mmgr_line_put(err_msg, "orior_cli has no home folder");
        return 0;
    }
    return cli_reach_store_in(home, cli_process_own(), root, address, key, err_msg);
}

void cli_reach_forget(void)
{
    char home[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    if (cli_home_folder(home))
    {
        reach_folder(home, folder);
        reach_file(folder, "", cli_process_own(), path);
        (void)query_removed(path);
    }
}

// one file of the folder read where it names a window stored, ending in .json and not beginning with a dot
static void reach_entry(void *context, const char *name, int is_folder)
{
    CliReachRead *const read = (CliReachRead *)context;
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    if (is_folder || (name[0] == '.') || (length < 5u) ||
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
    cli_full(read->folder, name, path);
    const unsigned char *const bytes = cli_file_read(path, &size);
    if (bytes == NULL)
    {
        return;
    }
    CliJson json;
    cli_json_begun(&json, s_nodes, CLI_REACH_NODES, s_text, sizeof(s_text));
    const unsigned int entry_count =
        cli_json_read(&json, (const char *)bytes, size, s_tokens, CLI_REACH_NODES, &err_msg);
    cli_file_let_go(bytes, size);
    const char *const address = cli_json_string(&json, cli_json_member(&json, entry_count, "address"), NULL);
    const char *const key = cli_json_string(&json, cli_json_member(&json, entry_count, "key"), NULL);
    const char *const root = cli_json_string(&json, cli_json_member(&json, entry_count, "root"), NULL);
    const unsigned int process = cli_json_member(&json, entry_count, "process");
    const char *const digits =
        cli_json_is(&json, process, CFG_JSON_NUMBER) ? json.text + json.node[process].text : NULL;
    const unsigned int root_node = cli_json_member(&json, entry_count, "root");
    if ((address == NULL) || (key == NULL) || (digits == NULL) || (digits[0] == '-') ||
        ((root == NULL) && !cli_json_is(&json, root_node, CFG_JSON_NULL)))
    {
        return;
    }
    CliReachEntry *const one = &read->entries[read->count];
    one->process = 0u;
    for (unsigned long long at = 0ull; (at < json.node[process].length) && (digits[at] >= '0') && (digits[at] <= '9');
         at += 1ull)
    {
        one->process = (one->process * 10u) + (unsigned int)(digits[at] - '0');
    }
    one->has_root = (root != NULL);
    memor.cpy(&(MemoriaCfg){
        .dst = one->root,
        .src = (root != NULL) ? root : "",
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = (root != NULL) ? root : "", .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = one->address,
                            .src = address,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = address, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){
        .dst = one->key, .src = key, .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u});
    read->count += 1u;
}

// whether two paths name one place, as their parts compare, a separator at the end aside
static int reach_same(const char *one, const char *other)
{
    size_t one_length = cellul.len(&(CatenaFinitaCfg){.src = one, .cap = CLI_CELLS_POOL});
    size_t other_length = cellul.len(&(CatenaFinitaCfg){.src = other, .cap = CLI_CELLS_POOL});
    while ((one_length > 1u) && ((one[one_length - 1u] == '/') || (one[one_length - 1u] == '\\')))
    {
        one_length -= 1u;
    }
    while ((other_length > 1u) && ((other[other_length - 1u] == '/') || (other[other_length - 1u] == '\\')))
    {
        other_length -= 1u;
    }
    return (one_length == other_length) && cli_text_same(one, other, one_length);
}

// the command sent to the window `client` joined, and its answer waited for on a channel of the command's own
static int reach_window(CliDoorClient *client, const char *menu, const char *command, const char *const *args,
                        unsigned int count, CliReachAnswer *answer, VerbaLinea *err_msg)
{
    static char s_request_buf[1u << 16u];
    static char s_body[1u << 16u];
    unsigned char secret[8];
    char id[24];
    char channel[64];
    if (!cli_random_bytes(secret, sizeof(secret)))
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
    if (!cli_door_listened(client, channel, err_msg))
    {
        return 0;
    }
    VerbaLinea request_line = {s_request_buf, sizeof(s_request_buf), 0ull};
    mmgr_line_put(&request_line, "{\"id\":\"");
    mmgr_line_put(&request_line, id);
    mmgr_line_put(&request_line, "\",\"menu\":");
    cli_json_quoted_lossy(&request_line, (const unsigned char *)menu,
                          cellul.len(&(CatenaFinitaCfg){.src = menu, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&request_line, ",\"command\":");
    cli_json_quoted_lossy(&request_line, (const unsigned char *)command,
                          cellul.len(&(CatenaFinitaCfg){.src = command, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&request_line, ",\"args\":[");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(&request_line, (at > 0u) ? "," : "");
        cli_json_quoted_lossy(&request_line, (const unsigned char *)args[at],
                              cellul.len(&(CatenaFinitaCfg){.src = args[at], .cap = CLI_CELLS_POOL}));
    }
    mmgr_line_put(&request_line, "]}");
    unsigned long long delivered = 0ull;
    if (!cli_door_send(client, "command", request_line.out, request_line.at, &delivered, err_msg))
    {
        return 0;
    }
    // the count shows where the silence is: 0 where no part of the window listens on command any more, more where the
    // page received and did not answer
    char from[CLI_NAME_MAX_LEN];
    char reply_channel[CLI_NAME_MAX_LEN];
    VerbaLinea body = {s_body, sizeof(s_body), 0ull};
    if (!cli_door_next(client, CLI_REACH_WAIT, reply_channel, from, &body))
    {
        mmgr_line_put(err_msg, "the window gave no answer, received by ");
        mmgr_line_decimal(err_msg, delivered, 1u);
        return 0;
    }
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    answer->done = 0;
    answer->text[0] = '\0';
    if (cli_lsp_member(body.out, body.at, "done", &start, &length))
    {
        answer->done =
            (length == 4ull) && (memor.cmp(&(MemoriaCfg){.src = body.out + start, .other = "true", .bytes = 4u}) == 0);
    }
    if (cli_lsp_member(body.out, body.at, "message", &start, &length))
    {
        (void)cli_lsp_string(body.out + start, length, answer->text, sizeof(answer->text));
    }
    return 1;
}

int cli_reach_request_in(const char *home, const char *root, const char *menu, const char *command,
                         const char *const *args, unsigned int count, CliReachAnswer *answer, VerbaLinea *err_msg)
{
    char folder[CLI_PATH_MAX_LEN];
    reach_folder(home, folder);
    cli_lock_take(&s_lock);
    CliReachRead read = {folder, s_entries, 0u};
    (void)query_folder_each(folder, reach_entry, &read);
    int answered = -1;
    for (unsigned int at = 0u; (at < read.count) && (answered < 0); at += 1u)
    {
        const CliReachEntry *const entry = &s_entries[at];
        // a command for a tree goes to the windows open on it; with no tree named, to the one window where there is
        // only one
        const int goes = (root != NULL) ? (entry->has_root && reach_same(entry->root, root)) : (read.count == 1u);
        if (!goes)
        {
            continue;
        }
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea refused = {why, sizeof(why), 0ull};
        CliDoorClient *const client = cli_door_joined(entry->address, entry->key, &refused);
        if (client == NULL)
        {
            char path[CLI_PATH_MAX_LEN];
            reach_file(folder, "", entry->process, path);
            (void)query_removed(path);
            continue;
        }
        answered = reach_window(client, menu, command, args, count, answer, err_msg);
        cli_door_left(client);
    }
    cli_lock_give(&s_lock);
    return answered;
}

int cli_reach_request(const char *root, const char *menu, const char *command, const char *const *args,
                      unsigned int count, CliReachAnswer *answer, VerbaLinea *err_msg)
{
    char home[CLI_PATH_MAX_LEN];
    if (!cli_home_folder(home))
    {
        return -1;
    }
    return cli_reach_request_in(home, root, menu, command, args, count, answer, err_msg);
}
