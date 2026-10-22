// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_parse.c: a file of requests read as JetBrains' HTTP Client writes one: each request from its line of `###`
// or the top of the file, its name, its request line and the address that runs on under it, its headers, and its body
// or the file it is read from; and the variables the file sets. Its lines are split as Rust's str::lines splits them
// and trimmed of white letters as Rust's str::trim trims them
#include "../text/json_tree.h"
#include "requests_internal.h"

// the most lines of a file read, and the bytes its pool holds past twice the file's, every text kept being of the
// file and its address the only one kept twice
#define CLI_REQUESTS_LINES (1u << 18u)
#define CLI_REQUESTS_POOL_MORE 64ull

CliLock g_cli_requests_lock = CLI_LOCK_INIT;
CliRequestsParsed g_cli_requests_parsed;

// where each line of the file being read starts and ends, cells of its count of lines held while it is read
static unsigned long long *s_line_start;
static unsigned long long *s_line_end;

// whether line `line` is blank once trimmed
static int parse_blank(const char *text, unsigned long long line)
{
    unsigned long long start = s_line_start[line];
    unsigned long long end = s_line_end[line];
    cli_requests_trimmed(text, &start, &end);
    return start == end;
}

// whether `size` bytes from `start` begin with `prefix`
static int parse_begins(const char *text, unsigned long long start, unsigned long long end, const char *prefix)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = prefix, .cap = CLI_CELLS_POOL});
    return (end - start >= size) &&
           (memor.cmp(&(MemoriaCfg){.src = text + start, .other = prefix, .bytes = (size_t)size}) == 0);
}

// whether a word is all capitals, as a method is
static int parse_is_method(const char *word, unsigned long long size)
{
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        if ((word[at] < 'A') || (word[at] > 'Z'))
        {
            return 0;
        }
    }
    return size > 0u;
}

// `size` bytes of `text` kept in the parse's pool: where they stand, and the parse full where they do not fit
static CliRequestsText parse_kept(CliRequestsParsed *parsed, const char *text, unsigned long long size)
{
    CliRequestsText kept = {0ull, 0ull};
    if (parsed->used + size + 1u > parsed->text_room)
    {
        parsed->full = 1;
        return kept;
    }
    kept.at = parsed->used;
    kept.size = size;
    memor.cpy(&(MemoriaCfg){.dst = parsed->text + parsed->used, .src = text, .bytes = (size_t)size});
    parsed->text[parsed->used + size] = '\0';
    parsed->used += size + 1u;
    return kept;
}

// a part of a line split at the first `split`, each side trimmed and kept: 1, or 0 where the part holds no `split`
static int parse_pair(CliRequestsParsed *parsed, const char *text, unsigned long long start, unsigned long long end,
                      char split, CliRequestsText *name, CliRequestsText *value)
{
    const char *const at = memor.chr(&(MemoriaCfg){.src = text + start, .val = split, .bytes = (size_t)(end - start)});
    if (at == NULL)
    {
        return 0;
    }
    unsigned long long name_start = start;
    unsigned long long name_end = (unsigned long long)(at - text);
    unsigned long long value_start = name_end + 1u;
    unsigned long long value_end = end;
    cli_requests_trimmed(text, &name_start, &name_end);
    cli_requests_trimmed(text, &value_start, &value_end);
    *name = parse_kept(parsed, text + name_start, name_end - name_start);
    *value = parse_kept(parsed, text + value_start, value_end - value_start);
    return 1;
}

// a comment or a variable line before a request line, trimmed, read: the request's name, whether it follows
// redirects, or a variable of the file. 1 where the line is one of these
static int parse_head_line(CliRequestsParsed *parsed, CliRequest *request, const char *text, unsigned long long start,
                           unsigned long long end)
{
    if ((text[start] == '#') || parse_begins(text, start, end, "//"))
    {
        start += (text[start] == '#') ? 1u : 2u;
        cli_requests_trimmed(text, &start, &end);
        if (parse_begins(text, start, end, "@name"))
        {
            start += 5u;
            cli_requests_trimmed(text, &start, &end);
            while ((start < end) && (text[start] == '='))
            {
                start += 1u;
            }
            cli_requests_trimmed(text, &start, &end);
            request->name = parse_kept(parsed, text + start, end - start);
        }
        else if ((end - start == 12u) && parse_begins(text, start, end, "@no-redirect"))
        {
            request->redirect = 0;
        }
        return 1;
    }
    if (text[start] != '@')
    {
        return 0;
    }
    CliRequestsText name = {0ull, 0ull};
    CliRequestsText value = {0ull, 0ull};
    if (parse_pair(parsed, text, start + 1u, end, '=', &name, &value))
    {
        if (parsed->variables == CLI_REQUESTS_VARIABLES)
        {
            parsed->full = 1;
            return 1;
        }
        parsed->variable_name[parsed->variables] = name;
        parsed->variable_value[parsed->variables] = value;
        parsed->variables += 1u;
    }
    return 1;
}

// the end of the word at `at` of a line, the start of the next past the white letters after it
static unsigned long long parse_word_end(const char *text, unsigned long long at, unsigned long long end)
{
    while ((at < end) && (cli_requests_white_at(text, at, end) == 0ull))
    {
        at += 1u;
    }
    return at;
}

static unsigned long long parse_word_next(const char *text, unsigned long long at, unsigned long long end)
{
    unsigned long long taken = 1ull;
    while ((at < end) && (taken > 0ull))
    {
        taken = cli_requests_white_at(text, at, end);
        at += taken;
    }
    return at;
}

// the request line, trimmed, read: a method and an address, or an address alone, sent as a GET
static void parse_request_line(CliRequestsParsed *parsed, CliRequest *request, const char *text,
                               unsigned long long start, unsigned long long end)
{
    const unsigned long long first_end = parse_word_end(text, start, end);
    const unsigned long long second = parse_word_next(text, first_end, end);
    const unsigned long long second_end = parse_word_end(text, second, end);
    if ((second < end) && parse_is_method(text + start, first_end - start))
    {
        request->method = parse_kept(parsed, text + start, first_end - start);
        request->url = parse_kept(parsed, text + second, second_end - second);
        return;
    }
    request->method = parse_kept(parsed, "GET", 3u);
    request->url = parse_kept(parsed, text + start, first_end - start);
}

// the address running on in indented lines from line `at`, each trimmed and joined to it: the line after them
static unsigned long long parse_url_lines(CliRequestsParsed *parsed, CliRequest *request, const char *text,
                                          unsigned long long at, unsigned long long last)
{
    const unsigned long long first = at;
    unsigned long long size = request->url.size;
    while ((at <= last) && (s_line_end[at] > s_line_start[at]) &&
           (cli_requests_white_at(text, s_line_start[at], s_line_end[at]) > 0ull) && !parse_blank(text, at))
    {
        unsigned long long start = s_line_start[at];
        unsigned long long end = s_line_end[at];
        cli_requests_trimmed(text, &start, &end);
        size += end - start;
        at += 1ull;
    }
    if ((at == first) || (parsed->used + size + 1u > parsed->text_room))
    {
        parsed->full = parsed->full || (at != first);
        return at;
    }
    CliRequestsText url = {parsed->used, size};
    memor.cpy(&(MemoriaCfg){
        .dst = parsed->text + parsed->used, .src = parsed->text + request->url.at, .bytes = (size_t)request->url.size});
    parsed->used += request->url.size;
    for (unsigned long long line = first; line < at; line += 1ull)
    {
        unsigned long long start = s_line_start[line];
        unsigned long long end = s_line_end[line];
        cli_requests_trimmed(text, &start, &end);
        memor.cpy(
            &(MemoriaCfg){.dst = parsed->text + parsed->used, .src = text + start, .bytes = (size_t)(end - start)});
        parsed->used += end - start;
    }
    parsed->text[parsed->used] = '\0';
    parsed->used += 1u;
    request->url = url;
    return at;
}

// the headers from line `at` to the first blank line, a comment passed over: the line after them
static unsigned long long parse_headers(CliRequestsParsed *parsed, CliRequest *request, const char *text,
                                        unsigned long long at, unsigned long long last)
{
    while ((at <= last) && !parse_blank(text, at))
    {
        unsigned long long start = s_line_start[at];
        unsigned long long end = s_line_end[at];
        cli_requests_trimmed(text, &start, &end);
        at += 1ull;
        if ((text[start] == '#') || parse_begins(text, start, end, "//"))
        {
            continue;
        }
        CliRequestsText name = {0ull, 0ull};
        CliRequestsText value = {0ull, 0ull};
        if (!parse_pair(parsed, text, start, end, ':', &name, &value))
        {
            continue;
        }
        if (request->headers == CLI_REQUESTS_HEADERS)
        {
            parsed->full = 1;
            continue;
        }
        request->header_name[request->headers] = name;
        request->header_value[request->headers] = value;
        request->headers += 1u;
    }
    return at;
}

// the body from line `at` to line `last`: its blank lines at the start and at the end passed over, cut at the first
// handler, a line of `> ` or `>> `; a file named by its one line of `< `, else its lines joined
static void parse_body(CliRequestsParsed *parsed, CliRequest *request, const char *text, unsigned long long at,
                       unsigned long long last)
{
    while ((at <= last) && parse_blank(text, at))
    {
        at += 1ull;
    }
    unsigned long long end = at;
    while ((end <= last) && !parse_begins(text, s_line_start[end], s_line_end[end], "> ") &&
           !parse_begins(text, s_line_start[end], s_line_end[end], ">> "))
    {
        end += 1ull;
    }
    while ((end > at) && parse_blank(text, end - 1u))
    {
        end -= 1ull;
    }
    if (end == at)
    {
        return;
    }
    if ((end - at == 1u) && parse_begins(text, s_line_start[at], s_line_end[at], "< "))
    {
        unsigned long long start = s_line_start[at] + 2u;
        unsigned long long stop = s_line_end[at];
        cli_requests_trimmed(text, &start, &stop);
        request->body_file = parse_kept(parsed, text + start, stop - start);
        request->has_body_file = 1;
        return;
    }
    unsigned long long size = end - at - 1u;
    for (unsigned long long line = at; line < end; line += 1ull)
    {
        size += s_line_end[line] - s_line_start[line];
    }
    if (parsed->used + size + 1u > parsed->text_room)
    {
        parsed->full = 1;
        return;
    }
    request->body.at = parsed->used;
    request->body.size = size;
    for (unsigned long long line = at; line < end; line += 1ull)
    {
        if (line > at)
        {
            parsed->text[parsed->used] = '\n';
            parsed->used += 1u;
        }
        memor.cpy(&(MemoriaCfg){.dst = parsed->text + parsed->used,
                                .src = text + s_line_start[line],
                                .bytes = (size_t)(s_line_end[line] - s_line_start[line])});
        parsed->used += s_line_end[line] - s_line_start[line];
    }
    parsed->text[parsed->used] = '\0';
    parsed->used += 1u;
    request->has_body = 1;
}

// a request's part of the file, lines `first` to `last`, read into `request`: 1, or 0 where it has no request line
static int parse_request(CliRequestsParsed *parsed, CliRequest *request, const char *text, unsigned long long first,
                         unsigned long long last)
{
    memor.set(&(MemoriaCfg){.dst = request, .val = 0, .bytes = sizeof(*request)});
    request->start = first;
    request->end = last;
    request->redirect = 1;
    unsigned long long at = first;
    unsigned long long start = parse_word_next(text, s_line_start[at], s_line_end[at]);
    if (parse_begins(text, start, s_line_end[at], "###"))
    {
        unsigned long long end = s_line_end[at];
        start += 3u;
        cli_requests_trimmed(text, &start, &end);
        request->name = parse_kept(parsed, text + start, end - start);
        at += 1ull;
    }
    int found = 0;
    while (!found && (at <= last))
    {
        start = s_line_start[at];
        unsigned long long end = s_line_end[at];
        cli_requests_trimmed(text, &start, &end);
        at += 1ull;
        if ((start < end) && !parse_head_line(parsed, request, text, start, end))
        {
            parse_request_line(parsed, request, text, start, end);
            request->line = at - 1ull;
            found = 1;
        }
    }
    if (!found)
    {
        return 0;
    }
    at = parse_url_lines(parsed, request, text, at, last);
    at = parse_headers(parsed, request, text, at, last);
    parse_body(parsed, request, text, at, last);
    return 1;
}

void cli_requests_parsed_given(CliRequestsParsed *parsed)
{
    cli_cell_given_by(&g_cli_requests_cells, parsed->requests);
    cli_cell_given_by(&g_cli_requests_cells, parsed->text);
    parsed->requests = NULL;
    parsed->requests_room = 0u;
    parsed->text = NULL;
    parsed->text_room = 0ull;
    parsed->count = 0u;
    parsed->variables = 0u;
    parsed->used = 0ull;
    parsed->full = 0;
}

// the lines of `size` bytes of `text`, a newline ending each but the last, CLI_REQUESTS_LINES at the most
static unsigned long long parse_lines_counted(const char *text, unsigned long long size)
{
    unsigned long long lines = 0ull;
    for (unsigned long long at = 0ull; (at < size) && (lines < CLI_REQUESTS_LINES); lines += 1ull)
    {
        const char *const newline =
            memor.chr(&(MemoriaCfg){.src = text + at, .val = '\n', .bytes = (size_t)(size - at)});
        at = (newline != NULL) ? ((unsigned long long)(newline - text) + 1ull) : size;
    }
    return lines;
}

// the file's lines read into the lines' cells, and its requests read: 1, or 0 where it is larger than `parsed` holds
static int parse_read(const char *text, unsigned long long size, CliRequestsParsed *parsed)
{
    unsigned long long lines = 0ull;
    unsigned long long at = 0ull;
    while ((at < size) && (lines < CLI_REQUESTS_LINES))
    {
        const char *const newline =
            memor.chr(&(MemoriaCfg){.src = text + at, .val = '\n', .bytes = (size_t)(size - at)});
        unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - text) : size;
        s_line_start[lines] = at;
        at = (newline != NULL) ? end + 1u : size;
        end -= ((newline != NULL) && (end > s_line_start[lines]) && (text[end - 1u] == '\r')) ? 1u : 0u;
        s_line_end[lines] = end;
        lines += 1ull;
    }
    if (at < size)
    {
        parsed->full = 1;
        return 0;
    }
    // a request is one part of the file, from its top or a line of `###`, so the parts counted are the most of them
    unsigned long long parts = (lines > 0ull) ? 1ull : 0ull;
    for (unsigned long long line = 1ull; line < lines; line += 1ull)
    {
        parts +=
            parse_begins(text, parse_word_next(text, s_line_start[line], s_line_end[line]), s_line_end[line], "###")
                ? 1ull
                : 0ull;
    }
    parts = (parts < CLI_REQUESTS_MOST) ? parts : CLI_REQUESTS_MOST;
    parsed->requests = cli_cell_taken_by(&g_cli_requests_cells, (parts + 1ull) * sizeof(*parsed->requests),
                                         "the requests of a file read");
    if (parsed->requests == NULL)
    {
        parsed->full = 1;
        return 0;
    }
    // a part counted is below CLI_REQUESTS_MOST, which an unsigned int holds
    parsed->requests_room = (unsigned int)parts;
    static CliRequest s_request;
    unsigned long long first = 0ull;
    while (first < lines)
    {
        unsigned long long next = first + 1ull;
        while ((next < lines) && !parse_begins(text, parse_word_next(text, s_line_start[next], s_line_end[next]),
                                               s_line_end[next], "###"))
        {
            next += 1ull;
        }
        if (parse_request(parsed, &s_request, text, first, next - 1ull))
        {
            parsed->full = parsed->full || (parsed->count == parsed->requests_room);
            if (parsed->count < parsed->requests_room)
            {
                parsed->requests[parsed->count] = s_request;
                parsed->count += 1u;
            }
        }
        first = next;
    }
    return !parsed->full;
}

int cli_requests_parse(const char *text, unsigned long long size, CliRequestsParsed *parsed)
{
    cli_requests_parsed_given(parsed);
    const unsigned long long lines = parse_lines_counted(text, size);
    const unsigned long long pool = (2ull * size) + CLI_REQUESTS_POOL_MORE;
    parsed->text_room = (pool < CLI_REQUESTS_TEXT) ? pool : CLI_REQUESTS_TEXT;
    parsed->text = cli_cell_taken_by(&g_cli_requests_cells, parsed->text_room, "the texts of a file of requests read");
    s_line_start = (parsed->text != NULL)
                       ? cli_cell_taken_by(&g_cli_requests_cells, (lines + 1ull) * sizeof(*s_line_start),
                                           "where the lines of a file of requests start")
                       : NULL;
    s_line_end = (s_line_start != NULL) ? cli_cell_taken_by(&g_cli_requests_cells, (lines + 1ull) * sizeof(*s_line_end),
                                                            "where the lines of a file of requests end")
                                        : NULL;
    const int read = (s_line_end != NULL) && parse_read(text, size, parsed);
    parsed->full = parsed->full || (s_line_end == NULL);
    cli_cell_given_by(&g_cli_requests_cells, s_line_end);
    cli_cell_given_by(&g_cli_requests_cells, s_line_start);
    s_line_end = NULL;
    s_line_start = NULL;
    return read;
}

const CliRequest *cli_requests_at(const CliRequestsParsed *parsed, unsigned long long line)
{
    for (unsigned int index = parsed->count; index > 0u; index -= 1u)
    {
        if (parsed->requests[index - 1u].start <= line)
        {
            return &parsed->requests[index - 1u];
        }
    }
    return (parsed->count > 0u) ? &parsed->requests[0] : NULL;
}
