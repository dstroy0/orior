// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// reach.c: the windows kept in orior's home, each in a file of its own, written beside and moved into place whole; and
// a command said to the window open on a tree, its answer waited for on a channel of the command's own
#include "reach.h"

#include "../comms/door.h"
#include "../servers/lsp.h"
#include "../text/json_tree.h"

#include "../text/text.h"

// the most windows kept that are read, how long a command line waits for the window's answer in milliseconds, and the
// nodes and bytes of one kept file read
#define CLI_REACH_WINDOWS 64u
#define CLI_REACH_WAIT 10000ull
#define CLI_REACH_NODES 256u
#define CLI_REACH_TEXT 8192u

// A window kept: its process, the tree it has open, its door's address, and its key
typedef struct
{
    unsigned int process;
    char root[CLI_PATH_LONGEST];
    int has_root;
    char address[CLI_PATH_LONGEST];
    char key[CLI_NAME_LONGEST];
} CliReachKept;

// The windows kept, read from the folder: where each goes, and how many there are
typedef struct
{
    const char *folder;
    CliReachKept *kept;
    unsigned int count;
} CliReachRead;

static CliReachKept s_kept[CLI_REACH_WINDOWS];
static CliLock s_lock = CLI_LOCK_INIT;

// the folder windows are kept in, under `home`, into `folder`
static void reach_folder(const char *home, char *folder)
{
    cli_full(home, "windows", folder);
}

// the file of the window of `process` in `folder`, its name begun by `before`, into `path`
static void reach_file(const char *folder, const char *before, unsigned int process, char *path)
{
    char name[64];
    CliLine named = {name, sizeof(name), 0ull};
    cli_line_text(&named, before);
    cli_line_decimal(&named, process, 1u);
    cli_line_text(&named, ".json");
    cli_line_closed(&named);
    cli_full(folder, name, path);
}

int cli_reach_keep_in(const char *home, unsigned int process, const char *root, const char *address, const char *key,
                      CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    char beside[CLI_PATH_LONGEST];
    char text[CLI_PATH_LONGEST * 3u];
    reach_folder(home, folder);
    if (!cli_private_folder_made(folder))
    {
        cli_line_text(said, folder);
        cli_line_text(said, ": the folder could not be made the reader's alone");
        return 0;
    }
    reach_file(folder, "", process, path);
    reach_file(folder, ".", process, beside);
    CliLine out = {text, sizeof(text), 0ull};
    cli_line_text(&out, "{\"process\":");
    cli_line_decimal(&out, process, 1u);
    cli_line_text(&out, ",\"root\":");
    if (root != NULL)
    {
        cli_json_quoted_lossy(&out, (const unsigned char *)root,
                              cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        cli_line_text(&out, "null");
    }
    cli_line_text(&out, ",\"address\":");
    cli_json_quoted_lossy(&out, (const unsigned char *)address,
                          cellul.len(&(CatenaFinitaCfg){.src = address, .cap = CLI_CELLS_POOL}));
    cli_line_text(&out, ",\"key\":");
    cli_json_quoted_lossy(&out, (const unsigned char *)key,
                          cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}));
    cli_line_character(&out, '}');
    if (!cli_line_fits(&out) || !cli_private_file_written(beside, out.out, out.at) || !cli_moved(beside, path))
    {
        cli_line_text(said, folder);
        cli_line_text(said, ": the window's file could not be written");
        return 0;
    }
    return 1;
}

int cli_reach_keep(const char *root, const char *address, const char *key, CliLine *said)
{
    char home[CLI_PATH_LONGEST];
    if (!cli_home_folder(home))
    {
        cli_line_text(said, "orior has no home folder");
        return 0;
    }
    return cli_reach_keep_in(home, cli_process_own(), root, address, key, said);
}

void cli_reach_forget(void)
{
    char home[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    if (cli_home_folder(home))
    {
        reach_folder(home, folder);
        reach_file(folder, "", cli_process_own(), path);
        (void)query_removed(path);
    }
}

// one file of the folder read where it names a window kept, ending in .json and not beginning with a dot
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
    char path[CLI_PATH_LONGEST];
    char why[CLI_SAID_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    unsigned long long size = 0ull;
    cli_full(read->folder, name, path);
    const unsigned char *const bytes = qry_member_file_mapped(path, &size);
    if (bytes == NULL)
    {
        return;
    }
    CliJson json;
    cli_json_begun(&json, s_nodes, CLI_REACH_NODES, s_text, sizeof(s_text));
    const unsigned int kept = cli_json_read(&json, (const char *)bytes, size, s_tokens, CLI_REACH_NODES, &said);
    qry_member_file_unmapped(bytes, size);
    const char *const address = cli_json_string(&json, cli_json_member(&json, kept, "address"), NULL);
    const char *const key = cli_json_string(&json, cli_json_member(&json, kept, "key"), NULL);
    const char *const root = cli_json_string(&json, cli_json_member(&json, kept, "root"), NULL);
    const unsigned int process = cli_json_member(&json, kept, "process");
    const char *const digits =
        cli_json_is(&json, process, CFG_JSON_NUMBER) ? json.text + json.node[process].text : NULL;
    const unsigned int root_node = cli_json_member(&json, kept, "root");
    if ((address == NULL) || (key == NULL) || (digits == NULL) || (digits[0] == '-') ||
        ((root == NULL) && !cli_json_is(&json, root_node, CFG_JSON_NULL)))
    {
        return;
    }
    CliReachKept *const one = &read->kept[read->count];
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

// the command said to the window `client` joined, and its answer waited for on a channel of the command's own
static int reach_window(CliDoorClient *client, const char *menu, const char *command, const char *const *args,
                        unsigned int count, CliReachAnswer *answer, CliLine *said)
{
    static char s_asked[1u << 16u];
    static char s_body[1u << 16u];
    unsigned char secret[8];
    char id[24];
    char channel[64];
    if (!cli_random_bytes(secret, sizeof(secret)))
    {
        cli_line_text(said, "the system gave no secret");
        return 0;
    }
    CliLine id_line = {id, sizeof(id), 0ull};
    for (unsigned int at = 0u; at < sizeof(secret); at += 1u)
    {
        cli_line_character(&id_line, "0123456789abcdef"[secret[at] >> 4u]);
        cli_line_character(&id_line, "0123456789abcdef"[secret[at] & 15u]);
    }
    cli_line_closed(&id_line);
    CliLine channel_line = {channel, sizeof(channel), 0ull};
    cli_line_text(&channel_line, "answer/");
    cli_line_text(&channel_line, id);
    cli_line_closed(&channel_line);
    if (!cli_door_listened(client, channel, said))
    {
        return 0;
    }
    CliLine asked = {s_asked, sizeof(s_asked), 0ull};
    cli_line_text(&asked, "{\"id\":\"");
    cli_line_text(&asked, id);
    cli_line_text(&asked, "\",\"menu\":");
    cli_json_quoted_lossy(&asked, (const unsigned char *)menu,
                          cellul.len(&(CatenaFinitaCfg){.src = menu, .cap = CLI_CELLS_POOL}));
    cli_line_text(&asked, ",\"command\":");
    cli_json_quoted_lossy(&asked, (const unsigned char *)command,
                          cellul.len(&(CatenaFinitaCfg){.src = command, .cap = CLI_CELLS_POOL}));
    cli_line_text(&asked, ",\"args\":[");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(&asked, (at > 0u) ? "," : "");
        cli_json_quoted_lossy(&asked, (const unsigned char *)args[at],
                              cellul.len(&(CatenaFinitaCfg){.src = args[at], .cap = CLI_CELLS_POOL}));
    }
    cli_line_text(&asked, "]}");
    unsigned long long heard = 0ull;
    if (!cli_door_said(client, "command", asked.out, asked.at, &heard, said))
    {
        return 0;
    }
    // the count says where the silence is: 0 where no part of the window listens on command any more, more where the
    // page heard and did not answer
    char from[CLI_NAME_LONGEST];
    char heard_on[CLI_NAME_LONGEST];
    CliLine body = {s_body, sizeof(s_body), 0ull};
    if (!cli_door_next(client, CLI_REACH_WAIT, heard_on, from, &body))
    {
        cli_line_text(said, "the window gave no answer, heard by ");
        cli_line_decimal(said, heard, 1u);
        return 0;
    }
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    answer->done = 0;
    answer->said[0] = '\0';
    if (cli_lsp_member(body.out, body.at, "done", &start, &length))
    {
        answer->done =
            (length == 4ull) && (memor.cmp(&(MemoriaCfg){.src = body.out + start, .other = "true", .bytes = 4u}) == 0);
    }
    if (cli_lsp_member(body.out, body.at, "said", &start, &length))
    {
        (void)cli_lsp_string(body.out + start, length, answer->said, sizeof(answer->said));
    }
    return 1;
}

int cli_reach_ask_in(const char *home, const char *root, const char *menu, const char *command, const char *const *args,
                     unsigned int count, CliReachAnswer *answer, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    reach_folder(home, folder);
    cli_lock_take(&s_lock);
    CliReachRead read = {folder, s_kept, 0u};
    (void)query_folder_each(folder, reach_entry, &read);
    int answered = -1;
    for (unsigned int at = 0u; (at < read.count) && (answered < 0); at += 1u)
    {
        const CliReachKept *const kept = &s_kept[at];
        // a command for a tree goes to the windows open on it; with no tree named, to the one window where there is
        // only one
        const int goes = (root != NULL) ? (kept->has_root && reach_same(kept->root, root)) : (read.count == 1u);
        if (!goes)
        {
            continue;
        }
        char why[CLI_SAID_LONGEST];
        CliLine refused = {why, sizeof(why), 0ull};
        CliDoorClient *const client = cli_door_joined(kept->address, kept->key, &refused);
        if (client == NULL)
        {
            char path[CLI_PATH_LONGEST];
            reach_file(folder, "", kept->process, path);
            (void)query_removed(path);
            continue;
        }
        answered = reach_window(client, menu, command, args, count, answer, said);
        cli_door_left(client);
    }
    cli_lock_give(&s_lock);
    return answered;
}

int cli_reach_ask(const char *root, const char *menu, const char *command, const char *const *args, unsigned int count,
                  CliReachAnswer *answer, CliLine *said)
{
    char home[CLI_PATH_LONGEST];
    if (!cli_home_folder(home))
    {
        return -1;
    }
    return cli_reach_ask_in(home, root, menu, command, args, count, answer, said);
}
