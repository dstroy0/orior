// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lsp_wire.c: the Language Server Protocol's wire: a message framed with its Content-Length and written, a message
// read back by its length, the members of its top object found, and a file's path and its URI each read as the other
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "lsp.h"

// the most bytes of a text escaped at once as it is written, and the room they take escaped
#define CLI_LSP_PIECE 2048u
#define CLI_LSP_ESCAPED 16384u

int cli_lsp_send(const CliLspWriter *writer, CliLock *lock, const char *body, unsigned long long size, CliLine *said)
{
    const CliLspPart part = {body, size, 0};
    return cli_lsp_send_parts(writer, lock, &part, 1u, said);
}

// the end of the next piece of a text to escape from `at`, a letter never cut
static unsigned long long wire_piece_end(const char *text, unsigned long long size, unsigned long long at)
{
    unsigned long long end = (size - at > CLI_LSP_PIECE) ? at + CLI_LSP_PIECE : size;
    while ((end < size) && (end > at + 1ull) && (((unsigned char)text[end] & 0xc0u) == 0x80u))
    {
        end -= 1ull;
    }
    return end;
}

// a part's bytes as they go on the wire, written by `writer` where it is given, else only counted: their count, or the
// count with 1 into `failed` where the writer failed
static unsigned long long wire_part(const CliLspWriter *writer, const CliLspPart *part, int *failed)
{
    if (!part->quoted)
    {
        *failed = *failed || ((writer != NULL) && !writer->write(writer->sink, part->bytes, part->size));
        return part->size;
    }
    char escaped[CLI_LSP_ESCAPED];
    unsigned long long count = 0ull;
    unsigned long long at = 0ull;
    while (at < part->size)
    {
        const unsigned long long end = wire_piece_end(part->bytes, part->size, at);
        CliLine line = {escaped, sizeof(escaped), 0ull};
        cli_json_escaped_lossy(&line, (const unsigned char *)part->bytes + at, end - at);
        *failed = *failed || ((writer != NULL) && !writer->write(writer->sink, escaped, line.at));
        count += line.at;
        at = end;
    }
    return count;
}

int cli_lsp_send_parts(const CliLspWriter *writer, CliLock *lock, const CliLspPart *parts, unsigned int count,
                       CliLine *said)
{
    int failed = 0;
    unsigned long long length = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        length += wire_part(NULL, &parts[at], &failed);
    }
    char head[64];
    CliLine line = {head, sizeof(head), 0ull};
    cli_line_text(&line, "Content-Length: ");
    cli_line_decimal(&line, length, 1u);
    cli_line_text(&line, "\r\n\r\n");
    cli_lock_take(lock);
    failed = !writer->write(writer->sink, head, line.at);
    for (unsigned int at = 0u; !failed && (at < count); at += 1u)
    {
        (void)wire_part(writer, &parts[at], &failed);
    }
    cli_lock_give(lock);
    if (failed)
    {
        cli_line_text(said, "the server stopped: its input could not be written");
        return 0;
    }
    return 1;
}

// the next byte the reader holds, more read from its source where it holds none: 1, or 0 at the source's end
static int wire_byte(CliLspReader *reader, char *byte)
{
    if (reader->start == reader->end)
    {
        reader->start = 0ull;
        reader->end = reader->read(reader->source, reader->held, sizeof(reader->held));
        if (reader->end == 0ull)
        {
            return 0;
        }
    }
    *byte = reader->held[reader->start];
    reader->start += 1ull;
    return 1;
}

// `size` bytes read from the reader into `into`, or passed over where it is NULL: 1, or 0 at the source's end
static int wire_bytes(CliLspReader *reader, char *into, unsigned long long size)
{
    unsigned long long done = 0ull;
    while (done < size)
    {
        if (reader->start == reader->end)
        {
            reader->start = 0ull;
            reader->end = reader->read(reader->source, reader->held, sizeof(reader->held));
            if (reader->end == 0ull)
            {
                return 0;
            }
        }
        const unsigned long long held = reader->end - reader->start;
        const unsigned long long taken = (held < size - done) ? held : size - done;
        if (into != NULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = into + done, .src = reader->held + reader->start, .bytes = taken});
        }
        reader->start += taken;
        done += taken;
    }
    return 1;
}

// a message's head read, up to the empty line that ends it, its Content-Length into `length`: 1, or 0 at the source's
// end or where the head names no length
static int wire_head_read(CliLspReader *reader, unsigned long long *length)
{
    int has_length = 0;
    for (;;)
    {
        char line[4096];
        unsigned long long used = 0ull;
        char byte = '\0';
        int read = 0;
        while (wire_byte(reader, &byte))
        {
            read = 1;
            if (byte == '\n')
            {
                break;
            }
            line[used] = byte;
            used += (used + 1ull < sizeof(line)) ? 1ull : 0ull;
        }
        if (!read)
        {
            return 0;
        }
        while ((used > 0ull) && (cellul.chr(&(CatenaFinitaCfg){
                                     .src = " \t\r", .cap = CLI_CELLS_POOL, .byte = line[used - 1ull]}) != NULL))
        {
            used -= 1ull;
        }
        if (used == 0ull)
        {
            return has_length;
        }
        static const char s_head[] = "Content-Length:";
        if ((used >= sizeof(s_head) - 1ull) &&
            (memor.cmp(&(MemoriaCfg){.src = line, .other = s_head, .bytes = sizeof(s_head) - 1ull}) == 0))
        {
            unsigned long long at = sizeof(s_head) - 1ull;
            while ((at < used) &&
                   (cellul.chr(&(CatenaFinitaCfg){.src = " \t", .cap = CLI_CELLS_POOL, .byte = line[at]}) != NULL))
            {
                at += 1ull;
            }
            at += ((at < used) && (line[at] == '+')) ? 1ull : 0ull;
            *length = 0ull;
            has_length = (at < used);
            for (; has_length && (at < used); at += 1ull)
            {
                const unsigned long long digit = (unsigned long long)(line[at] - '0');
                has_length = (line[at] >= '0') && (line[at] <= '9') && (*length <= (~0ull - digit) / 10ull);
                *length = has_length ? (*length * 10ull) + digit : *length;
            }
        }
    }
}

// whether `length` bytes of a body are a JSON body whole, its letters UTF-8 whole
static int wire_body_whole(const char *body, unsigned long long length)
{
    char tiny[1];
    CliLine checked = {tiny, sizeof(tiny), 0ull};
    return cli_utf8_whole(body, length) && cli_json_compacted(body, length, &checked);
}

char *cli_lsp_message_taken(CliLspReader *reader, unsigned long long *size)
{
    for (;;)
    {
        unsigned long long length = 0ull;
        if (!wire_head_read(reader, &length))
        {
            return NULL;
        }
        // one byte past the body, so an empty body is a cell too; a body the cells cannot meet is passed over whole
        char *const body = cli_cell_taken(length + 1ull, "a message read");
        if (!wire_bytes(reader, body, length))
        {
            cli_cell_given(body);
            return NULL;
        }
        if ((body != NULL) && wire_body_whole(body, length))
        {
            body[length] = '\0';
            *size = length;
            return body;
        }
        cli_cell_given(body);
    }
}

int cli_lsp_read_message(CliLspReader *reader, char *body, unsigned long long room, unsigned long long *size)
{
    for (;;)
    {
        unsigned long long length = 0ull;
        if (!wire_head_read(reader, &length))
        {
            return 0;
        }
        if (length > room)
        {
            // a body larger than the room is passed over whole, and the next message read
            if (!wire_bytes(reader, NULL, length))
            {
                return 0;
            }
            continue;
        }
        if (!wire_bytes(reader, body, length))
        {
            return 0;
        }
        *size = length;
        return wire_body_whole(body, length);
    }
}

// where a JSON value that starts at `at` ends
static unsigned long long wire_value_end(const char *text, unsigned long long size, unsigned long long at)
{
    unsigned long long depth = 0ull;
    int in_string = 0;
    while (at < size)
    {
        const char letter = text[at];
        if (in_string)
        {
            at += (letter == '\\') ? 2ull : 1ull;
            in_string = (letter != '"') || (letter == '\\');
            if ((letter == '"') && (depth == 0ull))
            {
                return at;
            }
            continue;
        }
        if (letter == '"')
        {
            in_string = 1;
        }
        else if ((letter == '{') || (letter == '['))
        {
            depth += 1ull;
        }
        else if ((letter == '}') || (letter == ']'))
        {
            if (depth == 0ull)
            {
                return at;
            }
            depth -= 1ull;
            if (depth == 0ull)
            {
                return at + 1ull;
            }
        }
        else if ((depth == 0ull) &&
                 (cellul.chr(&(CatenaFinitaCfg){.src = ", \t\r\n", .cap = CLI_CELLS_POOL, .byte = letter}) != NULL))
        {
            return at;
        }
        at += 1ull;
    }
    return at;
}

// the whitespace JSON allows passed over from `at`
static unsigned long long wire_white(const char *text, unsigned long long size, unsigned long long at)
{
    while ((at < size) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r\n", .cap = CLI_CELLS_POOL, .byte = text[at]}) != NULL))
    {
        at += 1ull;
    }
    return at;
}

int cli_lsp_member(const char *body, unsigned long long size, const char *name, unsigned long long *start,
                   unsigned long long *length)
{
    unsigned long long at = wire_white(body, size, 0ull);
    if ((at >= size) || (body[at] != '{'))
    {
        return 0;
    }
    at = wire_white(body, size, at + 1ull);
    const unsigned long long wanted = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    while ((at < size) && (body[at] == '"'))
    {
        const unsigned long long key_end = wire_value_end(body, size, at);
        const int named = (key_end - at == wanted + 2ull) &&
                          (memor.cmp(&(MemoriaCfg){.src = body + at + 1ull, .other = name, .bytes = wanted}) == 0);
        at = wire_white(body, size, key_end);
        at = wire_white(body, size, at + 1ull);
        const unsigned long long value_end = wire_value_end(body, size, at);
        if (named)
        {
            *start = at;
            *length = value_end - at;
            return 1;
        }
        at = wire_white(body, size, value_end);
        at = wire_white(body, size, at + 1ull);
    }
    return 0;
}

void cli_lsp_uri_of(const char *path, char *uri, unsigned long long room)
{
    static const char s_hex[17] = "0123456789ABCDEF";
    CliLine out = {uri, room, 0ull};
    cli_line_text(&out, ((path[0] == '/') || (path[0] == '\\')) ? "file://" : "file:///");
    for (const char *at = path; *at != '\0'; at += 1)
    {
        const unsigned char byte = (unsigned char)((*at == '\\') ? '/' : *at);
        if (((byte >= 'a') && (byte <= 'z')) || ((byte >= 'A') && (byte <= 'Z')) || ((byte >= '0') && (byte <= '9')) ||
            (cellul.chr(&(CatenaFinitaCfg){.src = "-._~/:", .cap = CLI_CELLS_POOL, .byte = byte}) != NULL &&
             (byte != 0u)))
        {
            cli_line_character(&out, (char)byte);
            continue;
        }
        cli_line_character(&out, '%');
        cli_line_character(&out, s_hex[byte >> 4u]);
        cli_line_character(&out, s_hex[byte & 0x0fu]);
    }
    cli_line_closed(&out);
}

// a hex digit's value, or 16 where it is none
static unsigned int wire_hex(char letter)
{
    return ((letter >= '0') && (letter <= '9'))   ? (unsigned int)(letter - '0')
           : ((letter >= 'a') && (letter <= 'f')) ? (unsigned int)(letter - 'a' + 10)
           : ((letter >= 'A') && (letter <= 'F')) ? (unsigned int)(letter - 'A' + 10)
                                                  : 16u;
}

int cli_lsp_path_of(const char *uri, unsigned long long length, char *path)
{
    if ((length < 7ull) || (memor.cmp(&(MemoriaCfg){.src = uri, .other = "file://", .bytes = 7u}) != 0))
    {
        return 0;
    }
    CliLine out = {path, CLI_PATH_LONGEST, 0ull};
    unsigned long long at = 7ull;
    while (at < length)
    {
        if ((uri[at] == '%') && (at + 3ull <= length))
        {
            // two hex digits, or a + and one, as Rust's u8 reading in base 16 takes them
            const unsigned int high = (uri[at + 1ull] == '+') ? 0u : wire_hex(uri[at + 1ull]);
            const unsigned int low = wire_hex(uri[at + 2ull]);
            if ((high < 16u) && (low < 16u))
            {
                cli_line_character(&out, (char)((high << 4u) | low));
                at += 3ull;
                continue;
            }
        }
        cli_line_character(&out, uri[at]);
        at += 1ull;
    }
    if ((cli_line_closed(&out) == 0ull) || !cli_utf8_whole(path, out.at))
    {
        return 0;
    }
    // file:///D:/x names D:/x, and file:///x names /x
    if ((out.at > 2ull) && (path[0] == '/') && (path[2] == ':'))
    {
        memor.move_down(&(MemoriaCfg){.dst = path, .src = path + 1, .bytes = out.at});
    }
    return 1;
}

void cli_lsp_key_of(const char *uri, unsigned long long length, char *key)
{
    if (!cli_lsp_path_of(uri, length, key))
    {
        CliLine out = {key, CLI_PATH_LONGEST, 0ull};
        cli_line_counted(&out, uri, length);
        cli_line_closed(&out);
    }
    for (char *letter = key; *letter != '\0'; letter += 1)
    {
        *letter = (*letter == '\\') ? '/' : *letter;
#if defined(_WIN32)
        *letter = ((*letter >= 'A') && (*letter <= 'Z')) ? (char)(*letter - 'A' + 'a') : *letter;
#endif
    }
}

int cli_lsp_string(const char *text, unsigned long long length, char *out, unsigned long long room)
{
    CliJsonNode node[1];
    CfgJsonToken token[1];
    CliJson json;
    cli_json_begun(&json, node, 1u, out, room);
    char why[CLI_SAID_LONGEST];
    CliLine unread = {why, sizeof(why), 0ull};
    const unsigned int root = cli_json_read(&json, text, length, token, 1u, &unread);
    unsigned long long size = 0ull;
    const char *const letters = cli_json_string(&json, root, &size);
    if ((letters == NULL) || (size >= room))
    {
        return 0;
    }
    memor.move_down(&(MemoriaCfg){.dst = out, .src = letters, .bytes = size});
    out[size] = '\0';
    return 1;
}

void cli_lsp_at(const char *path, unsigned int line, unsigned int col, const char *added, CliLine *params)
{
    char uri[CLI_PATH_LONGEST * 3u];
    cli_lsp_uri_of(path, uri, sizeof(uri));
    cli_line_text(params, "{\"textDocument\":{\"uri\":");
    cli_json_quoted(params, uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}));
    cli_line_text(params, "},\"position\":{\"line\":");
    cli_line_decimal(params, line, 1u);
    cli_line_text(params, ",\"character\":");
    cli_line_decimal(params, col, 1u);
    cli_line_character(params, '}');
    if (added != NULL)
    {
        cli_line_character(params, ',');
        cli_line_text(params, added);
    }
    cli_line_character(params, '}');
}
