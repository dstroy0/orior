// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// net.c: an address reached requested, refused where orior_cli is blocked from the network, and logged where it is let
// on
#include "net.h"
#include "../command_line/cells.h"
#include "../deploy/devcontainer.h"
#include "../text/json_tree.h"
#include "../text/text.h"

// the most addresses logged, and the bytes an address's line written is taken with past the file
#define CLI_NET_LOG_MAX_LINES 1000u

#define CLI_NET_LINE_MORE (4ull << 10u)

// Whether the window blocks orior_cli from the network: 0 where the window has not set it, and settings.json is read; 1
// where it lets orior_cli on; 2 where it blocks orior_cli from it. Read and set under the lock, which also guards the
// file of addresses
static int s_disabled;
static CliLock s_lock = CLI_LOCK_INIT;

// the tokens `size` bytes of JSON make, in a cell of their count given back with g_cli_cells->persistent_buf_release,
// NULL where it is refused; their count into `count`
static CfgJsonToken *cli_net_tokens(const char *text, unsigned long long size, unsigned int *count)
{
    *count = cli_json_tokens_counted(text, size) + 1u;
    return g_cli_cells->persistent_buf_alloc((unsigned long long)*count * sizeof(CfgJsonToken), &g_cli_machine_cells);
}

void cli_net_disable(int off)
{
    cli_lock_take(&s_lock);
    s_disabled = off ? 2 : 1;
    cli_lock_give(&s_lock);
}

// `settings.json` in orior's folder read as JSON with comments: whether its `network` is `off`
static int cli_settings_off(void)
{
    char folder[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    if (!cli_home_folder(folder))
    {
        return 0;
    }
    VerbaLinea line = {path, sizeof(path), 0ull};
    mmgr_line_put(&line, folder);
    mmgr_line_put(&line, "/settings.json");
    unsigned long long size = 0ull;
    const unsigned char *const bytes =
        (mmgr_line_finish(&line) != 0ull) ? cli_file_taken(path, &size, &g_cli_machine_cells) : NULL;
    if (bytes == NULL)
    {
        return 0;
    }
    // a text made plain is never longer than the text, as it only leaves comments and commas out
    char *const plain_text = g_cli_cells->temporary_buf_alloc(size + 1ull, &g_cli_machine_cells);
    VerbaLinea plain = mmgr_line_over(plain_text, size + 1ull);
    cli_plain_json((const char *)bytes, size, &plain);
    cli_file_given(bytes, &g_cli_machine_cells);
    unsigned int count = 0u;
    CfgJsonToken *const tokens = (plain_text != NULL) ? cli_net_tokens(plain_text, plain.at, &count) : NULL;
    CfgJsonParse parse = {0u, 0u, 0u, NULL};
    char value[8];
    const unsigned int network = ((tokens != NULL) && mmgr_line_ok(&plain) &&
                                  cfg_json_parse_metadata(plain_text, plain.at, tokens, count, &parse))
                                     ? cfg_json_member(plain_text, tokens, 0u, "network")
                                     : 0u;
    const int off = (network != 0u) && cfg_json_string(plain_text, &tokens[network], value, sizeof(value)) &&
                    (cellul.eq(&(CatenaFinitaCfg){.src = value, .other = "off", .cap = CLI_CELLS_POOL}));
    g_cli_cells->persistent_buf_release(tokens, &g_cli_machine_cells);
    g_cli_cells->persistent_buf_release(plain_text, &g_cli_machine_cells);
    return off;
}

int cli_net_is_disabled(void)
{
    cli_lock_take(&s_lock);
    const int disabled = (s_disabled == 1) ? 0 : (s_disabled == 2) ? 1 : cli_settings_off();
    cli_lock_give(&s_lock);
    return disabled;
}

void cli_net_host_of(const char *address, char *host)
{
    const char *rest = cellul.find(
        &(CatenaFinitaCfg){.src = address, .cap = CLI_CELLS_POOL, .other = "://", .other_cap = CLI_CELLS_POOL});
    rest = (rest != NULL) ? (rest + 3) : address;
    const char *end = rest;
    end += cli_text_to_any(end, 0ull, cellul.len(&(CatenaFinitaCfg){.src = end, .cap = CLI_CELLS_POOL}), "/?#");
    // after the last `@`, the user before it left off
    for (const char *at = end; at > rest; at -= 1)
    {
        if (at[-1] == '@')
        {
            rest = at;
            break;
        }
    }
    VerbaLinea line = {host, CLI_PATH_MAX_LEN, 0ull};
    if (rest[0] == '[')
    {
        const char *close = rest + 1;
        close += cli_text_to_any(close, 0ull, (unsigned long long)(end - close), "]");
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

int cli_net_local(const char *host)
{
    char lower[CLI_PATH_MAX_LEN];
    const char *from = host;
    const char *to = host + cellul.len(&(CatenaFinitaCfg){.src = host, .cap = CLI_CELLS_POOL});
    from += cli_text_past_any(from, 0ull, (unsigned long long)(to - from), CLI_TEXT_WHITE);
    to = from + cli_text_back_past(from, (unsigned long long)(to - from), CLI_TEXT_WHITE);
    unsigned long long length = 0ull;
    for (const char *at = from; (at < to) && (length + 1ull < sizeof(lower)); at += 1)
    {
        lower[length] = ((at[0] >= 'A') && (at[0] <= 'Z')) ? (char)(at[0] + 32) : at[0];
        length += 1ull;
    }
    lower[length] = '\0';
    return (length == 0ull) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = lower, .other = "localhost", .cap = CLI_CELLS_POOL})) ||
           ((length >= 10u) && (cellul.eq(&(CatenaFinitaCfg){
                                   .src = lower + length - 10u, .other = ".localhost", .cap = CLI_CELLS_POOL}))) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = lower, .other = "::1", .cap = CLI_CELLS_POOL})) ||
           (cellul.starts(&(CatenaFinitaCfg){.src = lower, .other = "127.", .cap = CLI_CELLS_POOL})) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = lower, .other = "0.0.0.0", .cap = CLI_CELLS_POOL}));
}

int cli_net_path(const char *address)
{
    const char first = address[0];
    return (first == '/') || (first == '\\') || (first == '.') ||
           (cellul.starts(&(CatenaFinitaCfg){.src = address, .other = "file://", .cap = CLI_CELLS_POOL})) ||
           ((cellul.len(&(CatenaFinitaCfg){.src = address, .cap = CLI_CELLS_POOL}) > 2u) &&
            ((((first | 0x20) >= 'a') && ((first | 0x20) <= 'z'))) && (address[1] == ':') &&
            ((address[2] == '\\') || (address[2] == '/')));
}

// One address reached logged as a line of network.jsonl, the file cut to its last CLI_NET_LOG_MAX_LINES where it
// contains twice as many
static void net_logged(const char *address, const char *purpose)
{
    char folder[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    if (!cli_home_folder(folder))
    {
        return;
    }
    VerbaLinea named = {path, sizeof(path), 0ull};
    mmgr_line_put(&named, folder);
    mmgr_line_put(&named, "/network.jsonl");
    if (mmgr_line_finish(&named) == 0ull)
    {
        return;
    }
    query_folder_made(folder);
    unsigned long long log_size = 0ull;
    (void)cli_file_size(path, &log_size);
    const unsigned long long line_room = log_size + CLI_NET_LINE_MORE;
    char *const line_cell = g_cli_cells->temporary_buf_alloc(line_room, &g_cli_machine_cells);
    VerbaLinea line = mmgr_line_over(line_cell, line_room);
    if (line.out == NULL)
    {
        return;
    }
    unsigned long long size = qry_member_file_read(path, (unsigned char *)line.out, line.capacity - 1ull);
    line.at = (size == QRY_MEMBER_UNREAD) ? 0ull : size;
    mmgr_line_put(&line, "{\"at\":");
    mmgr_line_decimal(&line, cli_wall_milliseconds(), 1u);
    mmgr_line_put(&line, ",\"address\":");
    cli_json_quoted(&line, address, cellul.len(&(CatenaFinitaCfg){.src = address, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&line, ",\"for\":");
    cli_json_quoted(&line, purpose, cellul.len(&(CatenaFinitaCfg){.src = purpose, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&line, "}\n");
    if (!mmgr_line_ok(&line))
    {
        g_cli_cells->persistent_buf_release(line.out, &g_cli_machine_cells);
        return;
    }
    // the lines counted from the end, the file cut from the CLI_NET_LOG_MAX_LINES-th last where it contains more than
    // twice as many
    unsigned long long from = 0ull;
    unsigned int lines = 0u;
    for (unsigned long long at = line.at; at > 0ull; at -= 1ull)
    {
        if ((line.out[at - 1u] == '\n') && (at != line.at))
        {
            lines += 1u;
            from = (lines == CLI_NET_LOG_MAX_LINES) ? at : from;
        }
    }
    lines += 1u;
    query_file_written(path, line.out + ((lines > (2u * CLI_NET_LOG_MAX_LINES)) ? from : 0ull),
                       line.at - ((lines > (2u * CLI_NET_LOG_MAX_LINES)) ? from : 0ull));
    g_cli_cells->persistent_buf_release(line.out, &g_cli_machine_cells);
}

int cli_net_reach(const char *address, const char *purpose, VerbaLinea *err_msg)
{
    char host[CLI_PATH_MAX_LEN];
    cli_net_host_of(address, host);
    if (cli_net_path(address) || cli_net_local(host))
    {
        return 1;
    }
    if (cli_net_is_disabled())
    {
        mmgr_line_put(err_msg, "orior_cli is blocked from the network, and ");
        mmgr_line_put(err_msg, purpose);
        mmgr_line_put(err_msg, " did not reach ");
        mmgr_line_put(err_msg, address);
        mmgr_line_put(err_msg, ": Preferences or Help, Network, lets it on");
        return 0;
    }
    cli_lock_take(&s_lock);
    net_logged(address, purpose);
    cli_lock_give(&s_lock);
    return 1;
}

void cli_net_reached(unsigned int most, VerbaLinea *out)
{
    char folder[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    mmgr_line_ch(out, '[');
    cli_lock_take(&s_lock);
    VerbaLinea named = {path, sizeof(path), 0ull};
    const int homed = cli_home_folder(folder);
    mmgr_line_put(&named, folder);
    mmgr_line_put(&named, "/network.jsonl");
    unsigned long long size = 0ull;
    const unsigned char *const bytes =
        (homed && (mmgr_line_finish(&named) != 0ull)) ? cli_file_taken(path, &size, &g_cli_machine_cells) : NULL;
    const char *const log_text = (const char *)bytes;
    // every line's tokens are at most the file's, and one cell of the file's fits any line's
    unsigned int count = 0u;
    CfgJsonToken *const tokens = (bytes != NULL) ? cli_net_tokens(log_text, size, &count) : NULL;
    unsigned int written = 0u;
    unsigned long long end = (tokens != NULL) ? size : 0ull;
    while ((end > 0ull) && (written < most))
    {
        unsigned long long start = end;
        start = cli_text_back_any(log_text, start, "\n");
        unsigned long long stop = end;
        stop -= ((stop > start) && (log_text[stop - 1u] == '\n')) ? 1ull : 0ull;
        stop -= ((stop > start) && (log_text[stop - 1u] == '\r')) ? 1ull : 0ull;
        // a line read as one address reached where it is one, written again as it reads
        CfgJsonParse parse = {0u, 0u, 0u, NULL};
        char address[CLI_PATH_MAX_LEN];
        char purpose[CLI_PATH_MAX_LEN];
        unsigned long long at = 0ull;
        if ((stop > start) && cfg_json_parse_metadata(log_text + start, stop - start, tokens, count, &parse))
        {
            const unsigned int when = cfg_json_member(log_text + start, tokens, 0u, "at");
            const unsigned int where = cfg_json_member(log_text + start, tokens, 0u, "address");
            const unsigned int why = cfg_json_member(log_text + start, tokens, 0u, "for");
            if ((when != 0u) && (where != 0u) && (why != 0u) &&
                cfg_json_unsigned(log_text + start, &tokens[when], &at) &&
                cfg_json_string(log_text + start, &tokens[where], address, sizeof(address)) &&
                cfg_json_string(log_text + start, &tokens[why], purpose, sizeof(purpose)))
            {
                mmgr_line_put(out, (written != 0u) ? ",{\"at\":" : "{\"at\":");
                mmgr_line_decimal(out, at, 1u);
                mmgr_line_put(out, ",\"address\":");
                cli_json_quoted(out, address, cellul.len(&(CatenaFinitaCfg){.src = address, .cap = CLI_CELLS_POOL}));
                mmgr_line_put(out, ",\"for\":");
                cli_json_quoted(out, purpose, cellul.len(&(CatenaFinitaCfg){.src = purpose, .cap = CLI_CELLS_POOL}));
                mmgr_line_ch(out, '}');
                written += 1u;
            }
        }
        if (start == 0ull)
        {
            break;
        }
        // the line before ends at this one's start, its line end left off
        end = start - 1ull;
    }
    g_cli_cells->persistent_buf_release(tokens, &g_cli_machine_cells);
    cli_file_given(bytes, &g_cli_machine_cells);
    cli_lock_give(&s_lock);
    mmgr_line_ch(out, ']');
}
