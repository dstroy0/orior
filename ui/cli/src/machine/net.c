// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// net.c: an address reached requested, refused where impero is blocked from the network, and logged where it is let
// on
#include "net.h"
#include "../command_line/pool.h"
#include "../deploy/devcontainer.h"
#include "../text/json_tree.h"
#include "../text/text.h"

// the max addresses logged, and the bytes an address's line written is taken with past the file
#define CLI_NET_LOG_MAX_LINES 1000u

#define CLI_NET_LINE_MORE (4ULL << 10u)

// Whether the window blocks impero from the network: 0 where the window has not set it, and settings.json is read; 1
// where it lets impero on; 2 where it blocks impero from it. Read and set under the lock, which also guards the
// file of addresses
static int s_disabled;

typedef struct
{
    const char *text;
    unsigned long long size;
    unsigned int *count;
    const char *address;
    const char *purpose;
} NetCtx;
// the tokens `size` bytes of JSON make, in a temporary cell of their count the caller gives back as its call ends,
// NULL where it is refused; their count into `count`
EMBED_INLINE CfgJsonToken *cli_net_tokens(const NetCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned int *count = args->count;
    *count = EMBED_CALL(cli_json_tree_text.json_tokens_count, CliJsonTreeTextCfg, .text = text, .size = size) + 1u;
    return g_cli_guard->temporary_buf_alloc((unsigned long long)*count * sizeof(CfgJsonToken), g_cli_machine_owner);
}

void cli_net_disable(const CliNetCfg *args)
{
    s_disabled = args->off ? 2 : 1;
}

// `settings.json` in orior's folder read as JSON with comments: whether its `network` is `off`
static int cli_settings_off(void)
{
    char folder[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder))
    {
        return 0;
    }
    VerbaLinea line = {path, sizeof(path), 0ULL};
    mmgr_line_put(&line, folder);
    mmgr_line_put(&line, "/settings.json");
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = (mmgr_line_finish(&line) != 0ULL)
                                           ? EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                        .size_pointer = &size, .owner = g_cli_machine_owner)
                                           : NULL;
    if (bytes == NULL)
    {
        return 0;
    }
    // a text made plain is never longer than the text, as it only leaves comments and commas out
    char *const plain_text = g_cli_guard->temporary_buf_alloc(size + 1ULL, g_cli_machine_owner);
    VerbaLinea plain = mmgr_line_over(plain_text, size + 1ULL);
    EMBED_CALL(cli_devcontainer_text.plain_json, CliDevcontainerTextCfg, .text = (const char *)bytes, .size = size,
               .plain = &plain);
    g_cli_guard->persistent_buf_release((void *)(embed_word)bytes, (g_cli_machine_owner));
    unsigned int count = 0u;
    CfgJsonToken *const tokens =
        (plain_text != NULL) ? EMBED_CALL(cli_net_tokens, NetCtx, .text = plain_text, .size = plain.at, .count = &count)
                             : NULL;
    CfgJsonParse parse = {0u, 0u, 0u, NULL};
    char value[8];
    const unsigned int network = ((tokens != NULL) && mmgr_line_ok(&plain) &&
                                  cfg_json_parse_metadata(plain_text, plain.at, tokens, count, &parse))
                                     ? cfg_json_member(plain_text, tokens, 0u, "network")
                                     : 0u;
    const int off = (network != 0u) && cfg_json_string(plain_text, &tokens[network], value, sizeof(value)) &&
                    (cellul.eq(&(CatenaFinitaCfg){.src = value, .other = "off", .cap = CLI_POOL_BYTES}));
    g_cli_guard->persistent_buf_release(tokens, g_cli_machine_owner);
    g_cli_guard->persistent_buf_release(plain_text, g_cli_machine_owner);
    return off;
}

int cli_net_is_disable(void)
{
    int disabled;
    if (s_disabled == 1)
    {
        disabled = 0;
    }
    else if (s_disabled == 2)
    {
        disabled = 1;
    }
    else
    {
        disabled = cli_settings_off();
    }
    return disabled;
}

void cli_net_host_get(const CliNetAddressHostCfg *args)
{
    const char *rest = cellul.find(
        &(CatenaFinitaCfg){.src = args->address, .cap = CLI_POOL_BYTES, .other = "://", .other_cap = CLI_POOL_BYTES});
    rest = (rest != NULL) ? (rest + 3) : args->address;
    const char *end = rest;
    end += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = end, .at = 0ULL,
                      .size = cellul.len(&(CatenaFinitaCfg){.src = end, .cap = CLI_POOL_BYTES}), .set = "/?#");
    // after the last `@`, the user before it left off
    {
        const char *at = end;
        while (at > rest)
        {
            if (at[-1] == '@')
            {
                rest = at;
                break;
            }
            at -= 1;
        }
    }
    VerbaLinea line = {args->host, CLI_PATH_MAX_LEN, 0ULL};
    if (rest[0] == '[')
    {
        const char *close = rest + 1;
        close += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = close, .at = 0ULL,
                            .size = (unsigned long long)(end - close), .set = "]");
        mmgr_line_put_n(&line, rest + 1, (unsigned long long)(close - rest - 1));
        mmgr_line_finish(&line);
        return;
    }
    unsigned int colons = 0u;
    const char *colon = end;
    for (const char *at = rest; at < end; at += 1)
    {
        colon = ((at[0] == ':') && (colons == 0u)) ? at : colon;
        colons += (at[0] == ':') ? 1u : 0u;
    }
    mmgr_line_put_n(&line, rest, (unsigned long long)(((colons == 1u) ? colon : end) - rest));
    mmgr_line_finish(&line);
}

int cli_net_local(const CliNetHostCfg *args)
{
    char lower[CLI_PATH_MAX_LEN];
    const char *from = args->host_read;
    const char *to = args->host_read + cellul.len(&(CatenaFinitaCfg){.src = args->host_read, .cap = CLI_POOL_BYTES});
    from += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = from, .at = 0ULL,
                       .size = (unsigned long long)(to - from), .set = CLI_TEXT_WHITE);
    to = from + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = from,
                           .at = (unsigned long long)(to - from), .set = CLI_TEXT_WHITE);
    unsigned long long length = 0ULL;
    {
        const char *at = from;
        while ((at < to) && (length + 1ULL < sizeof(lower)))
        {
            lower[length] = ((at[0] >= 'A') && (at[0] <= 'Z')) ? (char)(at[0] + 32) : at[0];
            length += 1ULL;
            at += 1;
        }
    }
    lower[length] = '\0';
    return (length == 0ULL) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = lower, .other = "localhost", .cap = CLI_POOL_BYTES})) ||
           ((length >= 10u) && (cellul.eq(&(CatenaFinitaCfg){
                                   .src = lower + length - 10u, .other = ".localhost", .cap = CLI_POOL_BYTES}))) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = lower, .other = "::1", .cap = CLI_POOL_BYTES})) ||
           (cellul.starts(&(CatenaFinitaCfg){.src = lower, .other = "127.", .cap = CLI_POOL_BYTES})) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = lower, .other = "0.0.0.0", .cap = CLI_POOL_BYTES}));
}

int cli_net_path(const CliNetAddressCfg *args)
{
    const char first = args->address[0];
    return (first == '/') || (first == '\\') || (first == '.') ||
           (cellul.starts(&(CatenaFinitaCfg){.src = args->address, .other = "file://", .cap = CLI_POOL_BYTES})) ||
           ((cellul.len(&(CatenaFinitaCfg){.src = args->address, .cap = CLI_POOL_BYTES}) > 2u) &&
            (((first | 0x20) >= 'a') && ((first | 0x20) <= 'z')) && (args->address[1] == ':') &&
            ((args->address[2] == '\\') || (args->address[2] == '/')));
}

// One address reached logged as a line of network.jsonl, the file cut to its last CLI_NET_LOG_MAX_LINES where it
// contains twice as many
EMBED_INLINE void net_log(const NetCtx *args)
{
    char folder[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder))
    {
        return;
    }
    VerbaLinea named = {path, sizeof(path), 0ULL};
    mmgr_line_put(&named, folder);
    mmgr_line_put(&named, "/network.jsonl");
    if (mmgr_line_finish(&named) == 0ULL)
    {
        return;
    }
    EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder);
    unsigned long long log_size = 0ULL;
    (void)EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = path, .size_pointer = &log_size);
    const unsigned long long line_capacity = log_size + CLI_NET_LINE_MORE;
    char *const line_cell = g_cli_guard->temporary_buf_alloc(line_capacity, g_cli_machine_owner);
    VerbaLinea line = mmgr_line_over(line_cell, line_capacity);
    if (line.out == NULL)
    {
        return;
    }
    unsigned long long size = qry_member_file_read(path, (unsigned char *)line.out, line.capacity - 1ULL);
    line.at = (size == QRY_MEMBER_UNREAD) ? 0ULL : size;
    mmgr_line_put(&line, "{\"at\":");
    mmgr_line_decimal(&line, cli_wall_milliseconds(), 1u);
    mmgr_line_put(&line, ",\"address\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &line, .text = args->address,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->address, .cap = CLI_POOL_BYTES}));
    mmgr_line_put(&line, ",\"for\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &line, .text = args->purpose,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->purpose, .cap = CLI_POOL_BYTES}));
    mmgr_line_put(&line, "}\n");
    if (!mmgr_line_ok(&line))
    {
        g_cli_guard->persistent_buf_release(line.out, g_cli_machine_owner);
        return;
    }
    // the lines counted from the end, the file cut from the CLI_NET_LOG_MAX_LINES-th last where it contains more than
    // twice as many
    unsigned long long from = 0ULL;
    unsigned int lines = 0u;
    for (unsigned long long at = line.at; at > 0ULL; at -= 1ULL)
    {
        if ((line.out[at - 1u] == '\n') && (at != line.at))
        {
            lines += 1u;
            from = (lines == CLI_NET_LOG_MAX_LINES) ? at : from;
        }
    }
    lines += 1u;
    EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path,
               .bytes = line.out + ((lines > (2u * CLI_NET_LOG_MAX_LINES)) ? from : 0ULL),
               .size = line.at - ((lines > (2u * CLI_NET_LOG_MAX_LINES)) ? from : 0ULL));
    g_cli_guard->persistent_buf_release(line.out, g_cli_machine_owner);
}

int cli_net_reach(const CliNetAddressCfg *args)
{
    char host[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_net_address_host.host_get, CliNetAddressHostCfg, .address = args->address, .host = host);
    if (cli_net_address.path(args) || EMBED_CALL(cli_net_host.local, CliNetHostCfg, .host_read = host))
    {
        return 1;
    }
    if (cli_net_is_disable())
    {
        mmgr_line_put(args->err_msg, "impero is blocked from the network, and ");
        mmgr_line_put(args->err_msg, args->purpose);
        mmgr_line_put(args->err_msg, " did not reach ");
        mmgr_line_put(args->err_msg, args->address);
        mmgr_line_put(args->err_msg, ": Preferences or Help, Network, lets it on");
        return 0;
    }
    EMBED_CALL(net_log, NetCtx, .address = args->address, .purpose = args->purpose);
    return 1;
}
