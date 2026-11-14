// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_parse.c: a file of requests read as JetBrains' HTTP Client writes one: each request from its line of `###`
// or the top of the file, its name, its request line and the address that runs on under it, its headers, and its body
// or the file it is read from; and the variables the file sets. Its lines are split as Rust's str::lines splits them
// and trimmed of white letters as Rust's str::trim trims them
#include "../text/json_tree.h"
#include "../text/text.h"
#include "requests_internal.h"

// the most lines of a file read, and the bytes its pool reserves past twice the file's, every text stored being of the
// file and its address the only one stored twice
#define CLI_REQUESTS_LINES (1u << 18u)
#define CLI_REQUESTS_POOL_MORE 64ULL

CliRequestsParsed g_cli_requests_parsed;

// where each line of the file being read starts and ends, cells of its count of lines allocated while it is read
static unsigned long long *s_line_start;
static unsigned long long *s_line_end;

typedef struct
{
    const char *text;
    unsigned long long line;
    unsigned long long start;
    unsigned long long end;
    const char *prefix;
    const char *word_text;
    unsigned long long size;
    CliRequestsParsed *parsed;
    char split;
    CliRequestsText *name;
    CliRequestsText *value;
    CliRequest *request;
    unsigned long long at;
    unsigned long long last;
    unsigned long long first;
} RequestsParseCtx;
// whether line `line` is blank once trimmed
EMBED_INLINE int parse_blank(const RequestsParseCtx *args)
{
    const char *text = args->text;
    unsigned long long start = s_line_start[args->line];
    unsigned long long end = s_line_end[args->line];
    EMBED_CALL(cli_requests_text_start_internal.trim, CliRequestsTextStartCtx, .text = text, .start = &start,
               .end = &end);
    return start == end;
}

// whether `size` bytes from `start` begin with `prefix`
EMBED_INLINE int parse_begins(const RequestsParseCtx *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->prefix, .cap = CLI_POOL_BYTES});
    return (args->end - args->start >= size) &&
           (memor.cmp(
                &(MemoriaCfg){.src = args->text + args->start, .other = args->prefix, .bytes = (embed_word)size}) == 0);
}

// whether a word is all capitals, as a method is
EMBED_INLINE int parse_is_method(const RequestsParseCtx *args)
{
    for (unsigned long long at = 0ULL; at < args->size; at += 1ULL)
    {
        if ((args->word_text[at] < 'A') || (args->word_text[at] > 'Z'))
        {
            return 0;
        }
    }
    return args->size > 0u;
}

// `size` bytes of `text` stored in the parse's pool: where they stand, and the parse full where they do not fit
EMBED_INLINE CliRequestsText parse_store(const RequestsParseCtx *args)
{
    unsigned long long size = args->size;
    CliRequestsText text_span = {0ULL, 0ULL};
    if (args->parsed->used + size + 1u > args->parsed->text_capacity)
    {
        args->parsed->full = 1;
        return text_span;
    }
    text_span.at = args->parsed->used;
    text_span.size = size;
    memor.cpy(
        &(MemoriaCfg){.dst = args->parsed->text + args->parsed->used, .src = args->text, .bytes = (embed_word)size});
    args->parsed->text[args->parsed->used + size] = '\0';
    args->parsed->used += size + 1u;
    return text_span;
}

// a part of a line split at the first `split`, each side trimmed and stored: 1, or 0 where the part contains no `split`
EMBED_INLINE int parse_pair(const RequestsParseCtx *args)
{
    const char *text = args->text;
    unsigned long long start = args->start;
    unsigned long long end = args->end;
    CliRequestsText *name = args->name;
    CliRequestsText *value = args->value;
    const char *const at =
        memor.chr(&(MemoriaCfg){.src = text + start, .val = args->split, .bytes = (embed_word)(end - start)});
    if (at == NULL)
    {
        return 0;
    }
    unsigned long long name_start = start;
    unsigned long long name_end = (unsigned long long)(at - text);
    unsigned long long value_start = name_end + 1u;
    unsigned long long value_end = end;
    EMBED_CALL(cli_requests_text_start_internal.trim, CliRequestsTextStartCtx, .text = text, .start = &name_start,
               .end = &name_end);
    EMBED_CALL(cli_requests_text_start_internal.trim, CliRequestsTextStartCtx, .text = text, .start = &value_start,
               .end = &value_end);
    *name = EMBED_CALL(parse_store, RequestsParseCtx, .parsed = args->parsed, .text = text + name_start,
                       .size = name_end - name_start);
    *value = EMBED_CALL(parse_store, RequestsParseCtx, .parsed = args->parsed, .text = text + value_start,
                        .size = value_end - value_start);
    return 1;
}

// a comment or a variable line before a request line, trimmed, read: the request's name, whether it follows
// redirects, or a variable of the file. 1 where the line is one of these
EMBED_INLINE int parse_head_line(const RequestsParseCtx *args)
{
    const char *text = args->text;
    unsigned long long start = args->start;
    unsigned long long end = args->end;
    if ((text[start] == '#') ||
        EMBED_CALL(parse_begins, RequestsParseCtx, .text = text, .start = start, .end = end, .prefix = "//"))
    {
        start += (text[start] == '#') ? 1u : 2u;
        EMBED_CALL(cli_requests_text_start_internal.trim, CliRequestsTextStartCtx, .text = text, .start = &start,
                   .end = &end);
        if (EMBED_CALL(parse_begins, RequestsParseCtx, .text = text, .start = start, .end = end, .prefix = "@name"))
        {
            start += 5u;
            EMBED_CALL(cli_requests_text_start_internal.trim, CliRequestsTextStartCtx, .text = text, .start = &start,
                       .end = &end);
            start = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = text, .at = start,
                               .size = end, .set = "=");
            EMBED_CALL(cli_requests_text_start_internal.trim, CliRequestsTextStartCtx, .text = text, .start = &start,
                       .end = &end);
            args->request->name = EMBED_CALL(parse_store, RequestsParseCtx, .parsed = args->parsed,
                                             .text = text + start, .size = end - start);
        }
        else if ((end - start == 12u) && EMBED_CALL(parse_begins, RequestsParseCtx, .text = text, .start = start,
                                                    .end = end, .prefix = "@no-redirect"))
        {
            args->request->redirect = 0;
        }
        return 1;
    }
    if (text[start] != '@')
    {
        return 0;
    }
    CliRequestsText name = {0ULL, 0ULL};
    CliRequestsText value = {0ULL, 0ULL};
    if (EMBED_CALL(parse_pair, RequestsParseCtx, .parsed = args->parsed, .text = text, .start = start + 1u, .end = end,
                   .split = '=', .name = &name, .value = &value))
    {
        if (args->parsed->variables == CLI_REQUESTS_VARIABLES)
        {
            args->parsed->full = 1;
            return 1;
        }
        args->parsed->variable_name[args->parsed->variables] = name;
        args->parsed->variable_value[args->parsed->variables] = value;
        args->parsed->variables += 1u;
    }
    return 1;
}

// the end of the word at `at` of a line, the start of the next past the white letters after it
EMBED_INLINE unsigned long long parse_word_end(const RequestsParseCtx *args)
{
    const char *text = args->text;
    unsigned long long at = args->at;
    unsigned long long end = args->end;
    while ((at < end) && (EMBED_CALL(cli_requests_text_at_internal.white_at, CliRequestsTextAtCtx, .text = text,
                                     .at = at, .end_value = end) == 0ULL))
    {
        at += 1u;
    }
    return at;
}

EMBED_INLINE unsigned long long parse_word_next(const RequestsParseCtx *args)
{
    const char *text = args->text;
    unsigned long long at = args->at;
    unsigned long long end = args->end;
    unsigned long long reserved = 1ULL;
    while ((at < end) && (reserved > 0ULL))
    {
        reserved = EMBED_CALL(cli_requests_text_at_internal.white_at, CliRequestsTextAtCtx, .text = text, .at = at,
                              .end_value = end);
        at += reserved;
    }
    return at;
}

// the request line, trimmed, read: a method and an address, or an address alone, sent as a GET
EMBED_INLINE void parse_request_line(const RequestsParseCtx *args)
{
    const unsigned long long first_end =
        EMBED_CALL(parse_word_end, RequestsParseCtx, .text = args->text, .at = args->start, .end = args->end);
    const unsigned long long second =
        EMBED_CALL(parse_word_next, RequestsParseCtx, .text = args->text, .at = first_end, .end = args->end);
    const unsigned long long second_end =
        EMBED_CALL(parse_word_end, RequestsParseCtx, .text = args->text, .at = second, .end = args->end);
    if ((second < args->end) && EMBED_CALL(parse_is_method, RequestsParseCtx, .word_text = args->text + args->start,
                                           .size = first_end - args->start))
    {
        args->request->method = EMBED_CALL(parse_store, RequestsParseCtx, .parsed = args->parsed,
                                           .text = args->text + args->start, .size = first_end - args->start);
        args->request->url = EMBED_CALL(parse_store, RequestsParseCtx, .parsed = args->parsed,
                                        .text = args->text + second, .size = second_end - second);
        return;
    }
    args->request->method =
        EMBED_CALL(parse_store, RequestsParseCtx, .parsed = args->parsed, .text = "GET", .size = 3u);
    args->request->url = EMBED_CALL(parse_store, RequestsParseCtx, .parsed = args->parsed,
                                    .text = args->text + args->start, .size = first_end - args->start);
}

// the address running on in indented lines from line `at`, each trimmed and joined to it: the line after them
EMBED_INLINE unsigned long long parse_url_lines(const RequestsParseCtx *args)
{
    const char *text = args->text;
    unsigned long long at = args->at;
    const unsigned long long first = at;
    unsigned long long size = args->request->url.size;
    while ((at <= args->last) && (s_line_end[at] > s_line_start[at]) &&
           (EMBED_CALL(cli_requests_text_at_internal.white_at, CliRequestsTextAtCtx, .text = text,
                       .at = s_line_start[at], .end_value = s_line_end[at]) > 0ULL) &&
           !EMBED_CALL(parse_blank, RequestsParseCtx, .text = text, .line = at))
    {
        unsigned long long start = s_line_start[at];
        unsigned long long end = s_line_end[at];
        EMBED_CALL(cli_requests_text_start_internal.trim, CliRequestsTextStartCtx, .text = text, .start = &start,
                   .end = &end);
        size += end - start;
        at += 1ULL;
    }
    if ((at == first) || (args->parsed->used + size + 1u > args->parsed->text_capacity))
    {
        args->parsed->full = args->parsed->full || (at != first);
        return at;
    }
    CliRequestsText url = {args->parsed->used, size};
    memor.cpy(&(MemoriaCfg){.dst = args->parsed->text + args->parsed->used,
                            .src = args->parsed->text + args->request->url.at,
                            .bytes = (embed_word)args->request->url.size});
    args->parsed->used += args->request->url.size;
    for (unsigned long long line = first; line < at; line += 1ULL)
    {
        unsigned long long start = s_line_start[line];
        unsigned long long end = s_line_end[line];
        EMBED_CALL(cli_requests_text_start_internal.trim, CliRequestsTextStartCtx, .text = text, .start = &start,
                   .end = &end);
        memor.cpy(&(MemoriaCfg){
            .dst = args->parsed->text + args->parsed->used, .src = text + start, .bytes = (embed_word)(end - start)});
        args->parsed->used += end - start;
    }
    args->parsed->text[args->parsed->used] = '\0';
    args->parsed->used += 1u;
    args->request->url = url;
    return at;
}

// the headers from line `at` to the first blank line, a comment passed over: the line after them
EMBED_INLINE unsigned long long parse_headers(const RequestsParseCtx *args)
{
    const char *text = args->text;
    unsigned long long at = args->at;
    while ((at <= args->last) && !EMBED_CALL(parse_blank, RequestsParseCtx, .text = text, .line = at))
    {
        unsigned long long start = s_line_start[at];
        unsigned long long end = s_line_end[at];
        EMBED_CALL(cli_requests_text_start_internal.trim, CliRequestsTextStartCtx, .text = text, .start = &start,
                   .end = &end);
        at += 1ULL;
        if ((text[start] == '#') ||
            EMBED_CALL(parse_begins, RequestsParseCtx, .text = text, .start = start, .end = end, .prefix = "//"))
        {
            continue;
        }
        CliRequestsText name = {0ULL, 0ULL};
        CliRequestsText value = {0ULL, 0ULL};
        if (!EMBED_CALL(parse_pair, RequestsParseCtx, .parsed = args->parsed, .text = text, .start = start, .end = end,
                        .split = ':', .name = &name, .value = &value))
        {
            continue;
        }
        if (args->request->headers == CLI_REQUESTS_HEADERS)
        {
            args->parsed->full = 1;
            continue;
        }
        args->request->header_name[args->request->headers] = name;
        args->request->header_value[args->request->headers] = value;
        args->request->headers += 1u;
    }
    return at;
}

// the body from line `at` to line `last`: its blank lines at the start and at the end passed over, cut at the first
// handler, a line of `> ` or `>> `; a file named by its one line of `< `, else its lines joined
EMBED_INLINE void parse_body(const RequestsParseCtx *args)
{
    const char *text = args->text;
    unsigned long long at = args->at;
    while ((at <= args->last) && EMBED_CALL(parse_blank, RequestsParseCtx, .text = text, .line = at))
    {
        at += 1ULL;
    }
    unsigned long long end = at;
    while ((end <= args->last) &&
           !EMBED_CALL(parse_begins, RequestsParseCtx, .text = text, .start = s_line_start[end], .end = s_line_end[end],
                       .prefix = "> ") &&
           !EMBED_CALL(parse_begins, RequestsParseCtx, .text = text, .start = s_line_start[end], .end = s_line_end[end],
                       .prefix = ">> "))
    {
        end += 1ULL;
    }
    while ((end > at) && EMBED_CALL(parse_blank, RequestsParseCtx, .text = text, .line = end - 1u))
    {
        end -= 1ULL;
    }
    if (end == at)
    {
        return;
    }
    if ((end - at == 1u) && EMBED_CALL(parse_begins, RequestsParseCtx, .text = text, .start = s_line_start[at],
                                       .end = s_line_end[at], .prefix = "< "))
    {
        unsigned long long start = s_line_start[at] + 2u;
        unsigned long long stop = s_line_end[at];
        EMBED_CALL(cli_requests_text_start_internal.trim, CliRequestsTextStartCtx, .text = text, .start = &start,
                   .end = &stop);
        args->request->body_file = EMBED_CALL(parse_store, RequestsParseCtx, .parsed = args->parsed,
                                              .text = text + start, .size = stop - start);
        args->request->has_body_file = 1;
        return;
    }
    unsigned long long size = end - at - 1u;
    for (unsigned long long line = at; line < end; line += 1ULL)
    {
        size += s_line_end[line] - s_line_start[line];
    }
    if (args->parsed->used + size + 1u > args->parsed->text_capacity)
    {
        args->parsed->full = 1;
        return;
    }
    args->request->body.at = args->parsed->used;
    args->request->body.size = size;
    for (unsigned long long line = at; line < end; line += 1ULL)
    {
        if (line > at)
        {
            args->parsed->text[args->parsed->used] = '\n';
            args->parsed->used += 1u;
        }
        memor.cpy(&(MemoriaCfg){.dst = args->parsed->text + args->parsed->used,
                                .src = text + s_line_start[line],
                                .bytes = (embed_word)(s_line_end[line] - s_line_start[line])});
        args->parsed->used += s_line_end[line] - s_line_start[line];
    }
    args->parsed->text[args->parsed->used] = '\0';
    args->parsed->used += 1u;
    args->request->has_body = 1;
}

// a request's part of the file, lines `first` to `last`, read into `request`: 1, or 0 where it has no request line
EMBED_INLINE int parse_request(const RequestsParseCtx *args)
{
    const char *text = args->text;
    memor.set(&(MemoriaCfg){.dst = args->request, .val = 0, .bytes = sizeof(*args->request)});
    args->request->start = args->first;
    args->request->end = args->last;
    args->request->redirect = 1;
    unsigned long long at = args->first;
    unsigned long long start =
        EMBED_CALL(parse_word_next, RequestsParseCtx, .text = text, .at = s_line_start[at], .end = s_line_end[at]);
    if (EMBED_CALL(parse_begins, RequestsParseCtx, .text = text, .start = start, .end = s_line_end[at],
                   .prefix = "###"))
    {
        unsigned long long end = s_line_end[at];
        start += 3u;
        EMBED_CALL(cli_requests_text_start_internal.trim, CliRequestsTextStartCtx, .text = text, .start = &start,
                   .end = &end);
        args->request->name = EMBED_CALL(parse_store, RequestsParseCtx, .parsed = args->parsed, .text = text + start,
                                         .size = end - start);
        at += 1ULL;
    }
    int found = 0;
    while (!found && (at <= args->last))
    {
        start = s_line_start[at];
        unsigned long long end = s_line_end[at];
        EMBED_CALL(cli_requests_text_start_internal.trim, CliRequestsTextStartCtx, .text = text, .start = &start,
                   .end = &end);
        at += 1ULL;
        if ((start < end) && !EMBED_CALL(parse_head_line, RequestsParseCtx, .parsed = args->parsed,
                                         .request = args->request, .text = text, .start = start, .end = end))
        {
            EMBED_CALL(parse_request_line, RequestsParseCtx, .parsed = args->parsed, .request = args->request,
                       .text = text, .start = start, .end = end);
            args->request->line = at - 1ULL;
            found = 1;
        }
    }
    if (!found)
    {
        return 0;
    }
    at = EMBED_CALL(parse_url_lines, RequestsParseCtx, .parsed = args->parsed, .request = args->request, .text = text,
                    .at = at, .last = args->last);
    at = EMBED_CALL(parse_headers, RequestsParseCtx, .parsed = args->parsed, .request = args->request, .text = text,
                    .at = at, .last = args->last);
    EMBED_CALL(parse_body, RequestsParseCtx, .parsed = args->parsed, .request = args->request, .text = text, .at = at,
               .last = args->last);
    return 1;
}

// the lines of `size` bytes of `text`, a newline ending each but the last, CLI_REQUESTS_LINES at the most
EMBED_INLINE unsigned long long parse_lines_count(const RequestsParseCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned long long lines = 0ULL;
    unsigned long long at = 0ULL;
    while ((at < size) && (lines < CLI_REQUESTS_LINES))
    {
        at = EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = text, .at = at, .size = size).next;
        lines += 1ULL;
    }
    return lines;
}

// the file's lines read into the lines' cells, and its requests read: 1, or 0 where it is larger than `parsed` fits
EMBED_INLINE int parse_read(const RequestsParseCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned long long lines = 0ULL;
    unsigned long long at = 0ULL;
    while ((at < size) && (lines < CLI_REQUESTS_LINES))
    {
        const CliTextLine bounds = EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = text, .at = at, .size = size);
        unsigned long long end = bounds.stop;
        s_line_start[lines] = at;
        at = bounds.next;
        end -= bounds.stop - bounds.cut;
        s_line_end[lines] = end;
        lines += 1ULL;
    }
    if (at < size)
    {
        args->parsed->full = 1;
        return 0;
    }
    // a request is one part of the file, from its top or a line of `###`, so the parts counted are the most of them
    unsigned long long parts = (lines > 0ULL) ? 1ULL : 0ULL;
    for (unsigned long long line = 1ULL; line < lines; line += 1ULL)
    {
        parts += EMBED_CALL(parse_begins, RequestsParseCtx, .text = text,
                            .start = EMBED_CALL(parse_word_next, RequestsParseCtx, .text = text,
                                                .at = s_line_start[line], .end = s_line_end[line]),
                            .end = s_line_end[line], .prefix = "###")
                     ? 1ULL
                     : 0ULL;
    }
    parts = (parts < CLI_REQUESTS_MAX) ? parts : CLI_REQUESTS_MAX;
    args->parsed->requests =
        g_cli_guard->temporary_buf_alloc((parts + 1ULL) * sizeof(*args->parsed->requests), g_cli_requests_owner);
    if (args->parsed->requests == NULL)
    {
        args->parsed->full = 1;
        return 0;
    }
    // a part counted is below CLI_REQUESTS_MAX, which an unsigned int stores
    args->parsed->requests_capacity = (unsigned int)parts;
    static CliRequest s_request;
    unsigned long long first = 0ULL;
    while (first < lines)
    {
        unsigned long long next = first + 1ULL;
        while ((next < lines) && !EMBED_CALL(parse_begins, RequestsParseCtx, .text = text,
                                             .start = EMBED_CALL(parse_word_next, RequestsParseCtx, .text = text,
                                                                 .at = s_line_start[next], .end = s_line_end[next]),
                                             .end = s_line_end[next], .prefix = "###"))
        {
            next += 1ULL;
        }
        if (EMBED_CALL(parse_request, RequestsParseCtx, .parsed = args->parsed, .request = &s_request, .text = text,
                       .first = first, .last = next - 1ULL))
        {
            args->parsed->full = args->parsed->full || (args->parsed->count == args->parsed->requests_capacity);
            if (args->parsed->count < args->parsed->requests_capacity)
            {
                args->parsed->requests[args->parsed->count] = s_request;
                args->parsed->count += 1u;
            }
        }
        first = next;
    }
    return !args->parsed->full;
}

int cli_requests_parse(const CliRequestsTextSizeParsedCtx *args)
{
    const char *text = args->text;
    g_cli_guard->persistent_buf_release(args->parsed->requests, g_cli_requests_owner);
    g_cli_guard->persistent_buf_release(args->parsed->text, g_cli_requests_owner);
    args->parsed->requests = NULL;
    args->parsed->requests_capacity = 0u;
    args->parsed->text = NULL;
    args->parsed->text_capacity = 0ULL;
    args->parsed->count = 0u;
    args->parsed->variables = 0u;
    args->parsed->used = 0ULL;
    args->parsed->full = 0;
    const unsigned long long lines = EMBED_CALL(parse_lines_count, RequestsParseCtx, .text = text, .size = args->size);
    const unsigned long long pool = (2ULL * args->size) + CLI_REQUESTS_POOL_MORE;
    args->parsed->text_capacity = (pool < CLI_REQUESTS_TEXT) ? pool : CLI_REQUESTS_TEXT;
    args->parsed->text = g_cli_guard->temporary_buf_alloc(args->parsed->text_capacity, g_cli_requests_owner);
    s_line_start = (args->parsed->text != NULL)
                       ? g_cli_guard->temporary_buf_alloc((lines + 1ULL) * sizeof(*s_line_start), g_cli_requests_owner)
                       : NULL;
    s_line_end = (s_line_start != NULL)
                     ? g_cli_guard->temporary_buf_alloc((lines + 1ULL) * sizeof(*s_line_end), g_cli_requests_owner)
                     : NULL;
    const int read = (s_line_end != NULL) &&
                     EMBED_CALL(parse_read, RequestsParseCtx, .text = text, .size = args->size, .parsed = args->parsed);
    args->parsed->full = args->parsed->full || (s_line_end == NULL);
    g_cli_guard->persistent_buf_release(s_line_end, g_cli_requests_owner);
    g_cli_guard->persistent_buf_release(s_line_start, g_cli_requests_owner);
    s_line_end = NULL;
    s_line_start = NULL;
    return read;
}

const CliRequest *cli_requests_at(const CliRequestsCtx *args)
{
    for (unsigned int index = args->parsed_read->count; index > 0u; index -= 1u)
    {
        if (args->parsed_read->requests[index - 1u].start <= args->line_u64)
        {
            return &args->parsed_read->requests[index - 1u];
        }
    }
    return (args->parsed_read->count > 0u) ? &args->parsed_read->requests[0] : NULL;
}
