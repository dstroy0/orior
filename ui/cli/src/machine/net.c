// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// net.c: an address reached asked for, refused where orior_cli is held off the network, and kept where it is let on
#include "net.h"
#include "../deploy/devcontainer.h"
#include "../text/json_tree.h"

// the most addresses kept, and the bytes an address's line written is begun with past the file before it grows
#define CLI_NET_KEPT 1000u

#define CLI_NET_LINE_MORE (4ull << 10u)

// Whether the window holds orior_cli off the network: 0 where the window has not said, and settings.json is read; 1
// where it lets orior_cli on; 2 where it holds orior_cli off. Read and set under the lock, which also holds the file of
// addresses
static int s_held;
static CliLock s_lock = CLI_LOCK_INIT;

// the tokens `size` bytes of JSON make, in a cell of their count given back with cli_cell_given, NULL where it is
// refused; their count into `count`
static CfgJsonToken *cli_net_tokens(const char *text, unsigned long long size, unsigned int *count)
{
    *count = cli_json_tokens_counted(text, size) + 1u;
    return cli_cell_taken((unsigned long long)*count * sizeof(CfgJsonToken), "the tokens of a network file");
}

void cli_net_hold(int off)
{
    cli_lock_take(&s_lock);
    s_held = off ? 2 : 1;
    cli_lock_give(&s_lock);
}

// `settings.json` in orior's folder read as JSON with comments: whether its `network` is `off`
static int cli_settings_off(void)
{
    char folder[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    if (!cli_home_folder(folder))
    {
        return 0;
    }
    CliLine line = {path, sizeof(path), 0ull};
    cli_line_text(&line, folder);
    cli_line_text(&line, "/settings.json");
    unsigned long long size = 0ull;
    const unsigned char *const bytes = (cli_line_closed(&line) != 0ull) ? cli_file_held(path, &size) : NULL;
    if (bytes == NULL)
    {
        return 0;
    }
    // a text made plain is never longer than the text, as it only leaves comments and commas out
    char *const plain_text = cli_cell_taken(size + 1ull, "settings.json made plain");
    CliLine plain = {plain_text, (plain_text != NULL) ? (size + 1ull) : 0ull, 0ull};
    if (plain_text != NULL)
    {
        cli_plain_json((const char *)bytes, size, &plain);
    }
    cli_file_let_go(bytes, size);
    unsigned int count = 0u;
    CfgJsonToken *const tokens = (plain_text != NULL) ? cli_net_tokens(plain_text, plain.at, &count) : NULL;
    CfgJsonParse parse = {0u, 0u, 0u, NULL};
    char value[8];
    const unsigned int network = ((tokens != NULL) && cli_line_fits(&plain) &&
                                  cfg_json_parse_metadata(plain_text, plain.at, tokens, count, &parse))
                                     ? cfg_json_member(plain_text, tokens, 0u, "network")
                                     : 0u;
    const int off = (network != 0u) && cfg_json_string(plain_text, &tokens[network], value, sizeof(value)) &&
                    (cellul.eq(&(CatenaFinitaCfg){.src = value, .other = "off", .cap = CLI_CELLS_POOL}));
    cli_cell_given(tokens);
    cli_cell_given(plain_text);
    return off;
}

int cli_net_held(void)
{
    cli_lock_take(&s_lock);
    const int held = (s_held == 1) ? 0 : (s_held == 2) ? 1 : cli_settings_off();
    cli_lock_give(&s_lock);
    return held;
}

void cli_net_host_of(const char *address, char *host)
{
    const char *rest = cellul.find(
        &(CatenaFinitaCfg){.src = address, .cap = CLI_CELLS_POOL, .other = "://", .other_cap = CLI_CELLS_POOL});
    rest = (rest != NULL) ? (rest + 3) : address;
    const char *end = rest;
    while ((end[0] != '\0') && (end[0] != '/') && (end[0] != '?') && (end[0] != '#'))
    {
        end += 1;
    }
    // after the last `@`, the user before it left off
    for (const char *at = end; at > rest; at -= 1)
    {
        if (at[-1] == '@')
        {
            rest = at;
            break;
        }
    }
    CliLine line = {host, CLI_PATH_LONGEST, 0ull};
    if (rest[0] == '[')
    {
        const char *close = rest + 1;
        while ((close < end) && (close[0] != ']'))
        {
            close += 1;
        }
        cli_line_counted(&line, rest + 1, (unsigned long long)(close - rest - 1));
        cli_line_closed(&line);
        return;
    }
    unsigned int colons = 0u;
    const char *colon = end;
    for (const char *at = rest; at < end; at += 1)
    {
        colon = ((at[0] == ':') && (colons == 0u)) ? at : colon;
        colons += (at[0] == ':') ? 1u : 0u;
    }
    cli_line_counted(&line, rest, (unsigned long long)(((colons == 1u) ? colon : end) - rest));
    cli_line_closed(&line);
}

int cli_net_local(const char *host)
{
    char lower[CLI_PATH_LONGEST];
    const char *from = host;
    const char *to = host + cellul.len(&(CatenaFinitaCfg){.src = host, .cap = CLI_CELLS_POOL});
    while ((from < to) && ((from[0] == ' ') || ((from[0] >= '\t') && (from[0] <= '\r'))))
    {
        from += 1;
    }
    while ((to > from) && ((to[-1] == ' ') || ((to[-1] >= '\t') && (to[-1] <= '\r'))))
    {
        to -= 1;
    }
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

// One address reached kept as a line of network.jsonl, the file cut to its last KEPT where it holds twice as many
static void cli_kept(const char *address, const char *purpose)
{
    char folder[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    if (!cli_home_folder(folder))
    {
        return;
    }
    CliLine named = {path, sizeof(path), 0ull};
    cli_line_text(&named, folder);
    cli_line_text(&named, "/network.jsonl");
    if (cli_line_closed(&named) == 0ull)
    {
        return;
    }
    query_folder_made(folder);
    unsigned long long held = 0ull;
    (void)cli_file_size(path, &held);
    CliLine line;
    if (!cli_line_growing(&line, held + CLI_NET_LINE_MORE, "the addresses reached"))
    {
        return;
    }
    unsigned long long size = qry_member_file_read(path, (unsigned char *)line.out, line.capacity - 1ull);
    line.at = (size == QRY_MEMBER_UNREAD) ? 0ull : size;
    cli_line_text(&line, "{\"at\":");
    cli_line_decimal(&line, cli_wall_milliseconds(), 1u);
    cli_line_text(&line, ",\"address\":");
    cli_json_quoted(&line, address, cellul.len(&(CatenaFinitaCfg){.src = address, .cap = CLI_CELLS_POOL}));
    cli_line_text(&line, ",\"for\":");
    cli_json_quoted(&line, purpose, cellul.len(&(CatenaFinitaCfg){.src = purpose, .cap = CLI_CELLS_POOL}));
    cli_line_text(&line, "}\n");
    if (!cli_line_fits(&line))
    {
        cli_cell_given(line.out);
        return;
    }
    // the lines counted from the end, the file kept from the KEPT-th last where it holds more than twice as many
    unsigned long long from = 0ull;
    unsigned int lines = 0u;
    for (unsigned long long at = line.at; at > 0ull; at -= 1ull)
    {
        if ((line.out[at - 1u] == '\n') && (at != line.at))
        {
            lines += 1u;
            from = (lines == CLI_NET_KEPT) ? at : from;
        }
    }
    lines += 1u;
    query_file_written(path, line.out + ((lines > (2u * CLI_NET_KEPT)) ? from : 0ull),
                       line.at - ((lines > (2u * CLI_NET_KEPT)) ? from : 0ull));
    cli_cell_given(line.out);
}

int cli_net_reach(const char *address, const char *purpose, CliLine *said)
{
    char host[CLI_PATH_LONGEST];
    cli_net_host_of(address, host);
    if (cli_net_path(address) || cli_net_local(host))
    {
        return 1;
    }
    if (cli_net_held())
    {
        cli_line_text(said, "orior_cli is held off the network, and ");
        cli_line_text(said, purpose);
        cli_line_text(said, " did not reach ");
        cli_line_text(said, address);
        cli_line_text(said, ": Preferences or Help, Network, lets it on");
        return 0;
    }
    cli_lock_take(&s_lock);
    cli_kept(address, purpose);
    cli_lock_give(&s_lock);
    return 1;
}

void cli_net_reached(unsigned int most, CliLine *out)
{
    char folder[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    cli_line_character(out, '[');
    cli_lock_take(&s_lock);
    CliLine named = {path, sizeof(path), 0ull};
    const int homed = cli_home_folder(folder);
    cli_line_text(&named, folder);
    cli_line_text(&named, "/network.jsonl");
    unsigned long long size = 0ull;
    const unsigned char *const bytes =
        (homed && (cli_line_closed(&named) != 0ull)) ? cli_file_held(path, &size) : NULL;
    const char *const kept = (const char *)bytes;
    // every line's tokens are no more than the file's, so one cell of the file's holds any line's
    unsigned int count = 0u;
    CfgJsonToken *const tokens = (bytes != NULL) ? cli_net_tokens(kept, size, &count) : NULL;
    unsigned int written = 0u;
    unsigned long long end = (tokens != NULL) ? size : 0ull;
    while ((end > 0ull) && (written < most))
    {
        unsigned long long start = end;
        while ((start > 0ull) && (kept[start - 1u] != '\n'))
        {
            start -= 1ull;
        }
        unsigned long long stop = end;
        stop -= ((stop > start) && (kept[stop - 1u] == '\n')) ? 1ull : 0ull;
        stop -= ((stop > start) && (kept[stop - 1u] == '\r')) ? 1ull : 0ull;
        // a line read as one address reached where it is one, written again as it reads
        CfgJsonParse parse = {0u, 0u, 0u, NULL};
        char address[CLI_PATH_LONGEST];
        char purpose[CLI_PATH_LONGEST];
        unsigned long long at = 0ull;
        if ((stop > start) && cfg_json_parse_metadata(kept + start, stop - start, tokens, count, &parse))
        {
            const unsigned int when = cfg_json_member(kept + start, tokens, 0u, "at");
            const unsigned int where = cfg_json_member(kept + start, tokens, 0u, "address");
            const unsigned int why = cfg_json_member(kept + start, tokens, 0u, "for");
            if ((when != 0u) && (where != 0u) && (why != 0u) && cfg_json_unsigned(kept + start, &tokens[when], &at) &&
                cfg_json_string(kept + start, &tokens[where], address, sizeof(address)) &&
                cfg_json_string(kept + start, &tokens[why], purpose, sizeof(purpose)))
            {
                cli_line_text(out, (written != 0u) ? ",{\"at\":" : "{\"at\":");
                cli_line_decimal(out, at, 1u);
                cli_line_text(out, ",\"address\":");
                cli_json_quoted(out, address, cellul.len(&(CatenaFinitaCfg){.src = address, .cap = CLI_CELLS_POOL}));
                cli_line_text(out, ",\"for\":");
                cli_json_quoted(out, purpose, cellul.len(&(CatenaFinitaCfg){.src = purpose, .cap = CLI_CELLS_POOL}));
                cli_line_character(out, '}');
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
    cli_cell_given(tokens);
    cli_file_let_go(bytes, size);
    cli_lock_give(&s_lock);
    cli_line_character(out, ']');
}
