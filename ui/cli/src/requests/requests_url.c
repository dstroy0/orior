// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_url.c: an address of a request taken apart, the address a redirect names read from the one it came from,
// a header of an answer found by its name, and the integers a head writes read as Rust reads them
#include "requests_internal.h"

#include "../text/text.h"

// a letter in ASCII's lower case
static char url_lower(char letter)
{
    return ((letter >= 'A') && (letter <= 'Z')) ? (char)(letter - 'A' + 'a') : letter;
}

int cli_requests_number(const char *text, unsigned long long size, unsigned int base, unsigned long long most,
                        unsigned long long *value)
{
    unsigned long long at = ((size > 1u) && (text[0] == '+')) ? 1u : 0u;
    *value = 0ull;
    if (at == size)
    {
        return 0;
    }
    for (; at < size; at += 1u)
    {
        const char letter = url_lower(text[at]);
        const unsigned int digit = ((letter >= '0') && (letter <= '9')) ? (unsigned int)(letter - '0')
                                   : ((base == 16u) && (letter >= 'a') && (letter <= 'f'))
                                       ? (unsigned int)(letter - 'a' + 10)
                                       : 16u;
        if ((digit >= base) || (*value > (most - digit) / base))
        {
            return 0;
        }
        *value = (*value * base) + digit;
    }
    return 1;
}

int cli_requests_holds(const char *text, unsigned long long size, const char *needle)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = needle, .cap = CLI_CELLS_POOL});
    for (unsigned long long at = 0ull; at + length <= size; at += 1ull)
    {
        unsigned long long same = 0ull;
        while ((same < length) && (url_lower(text[at + same]) == needle[same]))
        {
            same += 1ull;
        }
        if (same == length)
        {
            return 1;
        }
    }
    return 0;
}

// the host and the port of an address's authority into `address`, after its user: 1, or 0 with why said
static int url_host(const char *url, const char *authority, unsigned long long size, CliRequestsAddress *address,
                    CliLine *said)
{
    const char *port = NULL;
    unsigned long long port_size = 0ull;
    const char *host = authority;
    unsigned long long host_size = size;
    if ((size > 0u) && (authority[0] == '['))
    {
        const char *const close = memor.chr(&(MemoriaCfg){.src = authority, .val = ']', .bytes = (size_t)size});
        if (close == NULL)
        {
            cli_line_text(said, url);
            cli_line_text(said, " has an address that does not close");
            return 0;
        }
        host = authority + 1;
        host_size = (unsigned long long)(close - host);
        const int has_port = (close + 1 < authority + size) && (close[1] == ':');
        port = has_port ? (close + 2) : NULL;
        port_size = has_port ? (unsigned long long)(authority + size - port) : 0ull;
    }
    else
    {
        for (const char *at = authority; at < authority + size; at += 1)
        {
            port = (*at == ':') ? (at + 1) : port;
        }
        host_size = (port != NULL) ? (unsigned long long)(port - 1 - authority) : size;
        port_size = (port != NULL) ? (unsigned long long)(authority + size - port) : 0ull;
    }
    unsigned long long number = address->secure ? 443u : 80u;
    if ((port != NULL) && !cli_requests_number(port, port_size, 10u, 65535u, &number))
    {
        cli_line_counted(said, port, port_size);
        cli_line_text(said, " is not a port");
        return 0;
    }
    // a port read is at most 65535
    address->port = (unsigned int)number;
    if (host_size == 0u)
    {
        cli_line_text(said, url);
        cli_line_text(said, " names no host");
        return 0;
    }
    CliLine host_line = {address->host, sizeof(address->host), 0ull};
    cli_line_counted(&host_line, host, host_size);
    if (cli_line_closed(&host_line) == 0ull)
    {
        cli_line_text(said, "the request is larger than orior_cli holds");
        return 0;
    }
    return 1;
}

int cli_requests_address(const char *url, CliRequestsAddress *address, CliLine *said)
{
    const char *const scheme_end =
        cellul.find(&(CatenaFinitaCfg){.src = url, .cap = CLI_CELLS_POOL, .other = "://", .other_cap = CLI_CELLS_POOL});
    if (scheme_end == NULL)
    {
        cli_line_text(said, url);
        cli_line_text(said, " names no scheme");
        return 0;
    }
    const unsigned long long scheme = (unsigned long long)(scheme_end - url);
    address->secure = (scheme == 5u) && cli_requests_holds(url, 5u, "https");
    if (!address->secure && !((scheme == 4u) && cli_requests_holds(url, 4u, "http")))
    {
        for (unsigned long long at = 0ull; at < scheme; at += 1ull)
        {
            cli_line_character(said, url_lower(url[at]));
        }
        cli_line_text(said, " is not http or https");
        return 0;
    }
    const char *const rest = scheme_end + 3;
    const unsigned long long authority_end = cli_text_upto(rest, "/?#");
    const char *authority = rest;
    for (const char *at = rest; at < rest + authority_end; at += 1)
    {
        authority = (*at == '@') ? (at + 1) : authority;
    }
    const char *const target = rest + authority_end;
    const unsigned long long target_size = cli_text_upto(target, "#");
    CliLine target_line = {address->target, sizeof(address->target), 0ull};
    cli_line_text(&target_line, ((target_size == 0u) || (target[0] == '?')) ? "/" : "");
    cli_line_counted(&target_line, target, target_size);
    if (cli_line_closed(&target_line) == 0ull)
    {
        cli_line_text(said, "the request is larger than orior_cli holds");
        return 0;
    }
    return url_host(url, authority, (unsigned long long)(rest + authority_end - authority), address, said);
}

void cli_requests_joined(const char *base, const char *location, CliLine *out)
{
    if (cellul.find(&(CatenaFinitaCfg){
            .src = location, .cap = CLI_CELLS_POOL, .other = "://", .other_cap = CLI_CELLS_POOL}) != NULL)
    {
        cli_line_text(out, location);
        return;
    }
    const char *const scheme_end = cellul.find(
        &(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL, .other = "://", .other_cap = CLI_CELLS_POOL});
    const char *const rest = (scheme_end != NULL) ? (scheme_end + 3) : base;
    if (scheme_end != NULL)
    {
        cli_line_counted(out, base, (unsigned long long)(scheme_end - base));
    }
    else
    {
        cli_line_text(out, "http");
    }
    cli_line_text(out, "://");
    if ((location[0] == '/') && (location[1] == '/'))
    {
        cli_line_text(out, location + 2);
        return;
    }
    const unsigned long long authority = cli_text_upto(rest, "/?#");
    cli_line_counted(out, rest, authority);
    if (location[0] != '/')
    {
        const char *const path = rest + authority;
        const unsigned long long path_size = cli_text_upto(path, "?#");
        unsigned long long folder = 0ull;
        for (unsigned long long at = 0ull; at < path_size; at += 1ull)
        {
            folder = (path[at] == '/') ? (at + 1u) : folder;
        }
        if (folder == 0u)
        {
            cli_line_character(out, '/');
        }
        cli_line_counted(out, path, folder);
    }
    cli_line_text(out, location);
}

const CliRequestsText *cli_requests_header(const CliRequestsAnswer *answer, const char *name)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    for (unsigned int at = 0u; at < answer->headers; at += 1u)
    {
        const CliRequestsText *const held = &answer->header_name[at];
        unsigned long long same = 0ull;
        while ((held->size == length) && (same < length) &&
               (url_lower(answer->head[held->at + same]) == url_lower(name[same])))
        {
            same += 1ull;
        }
        if ((held->size == length) && (same == length))
        {
            return &answer->header_value[at];
        }
    }
    return NULL;
}
