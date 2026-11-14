// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lsp_wire.c: the Language Server Protocol's wire: a message framed with its Content-Length and written, a message
// read back by its length, the members of its top object found, and a file's path and its URI each read as the other
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "lsp.h"

// the max bytes of a text escaped at once as it is written, and the capacity they take escaped
#define CLI_LSP_PIECE 2048u
#define CLI_LSP_ESCAPED 16384u

int cli_lsp_send(const CliLspWriterCfg *args)
{
    const CliLspPart part = {args->body, args->size_value, 0};
    return EMBED_CALL(cli_lsp_writer_lock.send_parts, CliLspWriterLockCfg, .writer = args->writer, .parts = &part,
                      .count = 1u, .err_msg = args->err_msg);
}

typedef struct
{
    const char *text;
    unsigned long long size;
    unsigned long long at;
    const CliLspWriter *writer;
    const CliLspPart *part;
    int *failed;
    CliLspReader *reader;
    char *byte;
    char *into;
    unsigned long long *length;
    const char *body;
    unsigned long long length_value;
    char character;
} LspWireCtx;
// the end of the next piece of a text to escape from `at`, a character never cut
EMBED_INLINE unsigned long long wire_piece_end(const LspWireCtx *args)
{
    unsigned long long end = (args->size - args->at > CLI_LSP_PIECE) ? args->at + CLI_LSP_PIECE : args->size;
    while ((end < args->size) && (end > args->at + 1ULL) && (((unsigned char)args->text[end] & 0xc0u) == 0x80u))
    {
        end -= 1ULL;
    }
    return end;
}

// a part's bytes as they go on the wire, written by `writer` where it is given, else only counted: their count, or the
// count with 1 into `failed` where the writer failed
EMBED_INLINE unsigned long long wire_part(const LspWireCtx *args)
{
    int *failed = args->failed;
    if (args->part->cell != NULL)
    {
        // a part in a movable cell goes on the wire a window at a time, read out of the cell as it goes, a character
        // never cut between windows
        char window[CLI_LSP_PIECE + 4u];
        unsigned long long count = 0ULL;
        unsigned long long at = 0ULL;
        while (at < args->part->size)
        {
            const unsigned long long left = args->part->size - at;
            const unsigned long long bytes = (left < sizeof(window)) ? left : sizeof(window);
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = args->part->cell,
                                                                    .offset = (embed_word)(args->part->offset + at),
                                                                    .dst = window,
                                                                    .bytes = (embed_word)bytes});
            const unsigned long long end =
                EMBED_CALL(wire_piece_end, LspWireCtx, .text = window, .size = bytes, .at = 0ULL);
            const CliLspPart piece = {window, end, args->part->quoted, NULL, 0ULL};
            count += EMBED_CALL(wire_part, LspWireCtx, .writer = args->writer, .part = &piece, .failed = failed);
            at += end;
        }
        return count;
    }
    if (!args->part->quoted)
    {
        *failed = *failed || ((args->writer != NULL) &&
                              !EMBED_CALL(args->writer->write, CliSinkBytesSizeCfg, .sink = args->writer->sink,
                                          .bytes = args->part->bytes, .size = args->part->size));
        return args->part->size;
    }
    char escaped[CLI_LSP_ESCAPED];
    unsigned long long count = 0ULL;
    unsigned long long at = 0ULL;
    while (at < args->part->size)
    {
        const unsigned long long end =
            EMBED_CALL(wire_piece_end, LspWireCtx, .text = args->part->bytes, .size = args->part->size, .at = at);
        VerbaLinea line = {escaped, sizeof(escaped), 0ULL};
        EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = &line,
                   .text_bytes = (const unsigned char *)args->part->bytes + at, .length = end - at);
        *failed = *failed || ((args->writer != NULL) &&
                              !EMBED_CALL(args->writer->write, CliSinkBytesSizeCfg, .sink = args->writer->sink,
                                          .bytes = escaped, .size = line.at));
        count += line.at;
        at = end;
    }
    return count;
}

int cli_lsp_send_parts(const CliLspWriterLockCfg *args)
{
    int failed = 0;
    unsigned long long length = 0ULL;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        length += EMBED_CALL(wire_part, LspWireCtx, .writer = NULL, .part = &args->parts[at], .failed = &failed);
    }
    char head[64];
    VerbaLinea line = {head, sizeof(head), 0ULL};
    mmgr_line_put(&line, "Content-Length: ");
    mmgr_line_decimal(&line, length, 1u);
    mmgr_line_put(&line, "\r\n\r\n");
    failed = !EMBED_CALL(args->writer->write, CliSinkBytesSizeCfg, .sink = args->writer->sink, .bytes = head,
                         .size = line.at);
    for (unsigned int at = 0u; !failed && (at < args->count); at += 1u)
    {
        (void)EMBED_CALL(wire_part, LspWireCtx, .writer = args->writer, .part = &args->parts[at], .failed = &failed);
    }
    if (failed)
    {
        mmgr_line_put(args->err_msg, "the server stopped: its input could not be written");
        return 0;
    }
    return 1;
}

// the next byte the reader buffers, more read from its source where it buffers none: 1, or 0 at the source's end
EMBED_INLINE int wire_byte(const LspWireCtx *args)
{
    char *byte = args->byte;
    if (args->reader->start == args->reader->end)
    {
        args->reader->start = 0ULL;
        args->reader->end = EMBED_CALL(args->reader->read, CliSourcePieceCapacityCfg, .source = args->reader->source,
                                       .piece = args->reader->buf, .capacity = sizeof(args->reader->buf));
        if (args->reader->end == 0ULL)
        {
            return 0;
        }
    }
    *byte = args->reader->buf[args->reader->start];
    args->reader->start += 1ULL;
    return 1;
}

// `size` bytes read from the reader into `into`, or passed over where it is NULL: 1, or 0 at the source's end
EMBED_INLINE int wire_bytes(const LspWireCtx *args)
{
    unsigned long long done = 0ULL;
    while (done < args->size)
    {
        if (args->reader->start == args->reader->end)
        {
            args->reader->start = 0ULL;
            args->reader->end =
                EMBED_CALL(args->reader->read, CliSourcePieceCapacityCfg, .source = args->reader->source,
                           .piece = args->reader->buf, .capacity = sizeof(args->reader->buf));
            if (args->reader->end == 0ULL)
            {
                return 0;
            }
        }
        const unsigned long long buffered = args->reader->end - args->reader->start;
        const unsigned long long reserved = (buffered < args->size - done) ? buffered : args->size - done;
        if (args->into != NULL)
        {
            memor.cpy(&(MemoriaCfg){
                .dst = args->into + done, .src = args->reader->buf + args->reader->start, .bytes = reserved});
        }
        args->reader->start += reserved;
        done += reserved;
    }
    return 1;
}

// a message's head read, up to the empty line that ends it, its Content-Length into `length`: 1, or 0 at the source's
// end or where the head names no length
static int wire_head_read(const LspWireCtx *args)
{
    unsigned long long *length = args->length;
    int has_length = 0;
    for (;;)
    {
        char line[4096];
        unsigned long long used = 0ULL;
        char byte = '\0';
        int read = 0;
        while (EMBED_CALL(wire_byte, LspWireCtx, .reader = args->reader, .byte = &byte))
        {
            read = 1;
            if (byte == '\n')
            {
                break;
            }
            line[used] = byte;
            used += (used + 1ULL < sizeof(line)) ? 1ULL : 0ULL;
        }
        if (!read)
        {
            return 0;
        }
        while ((used > 0ULL) && (cellul.chr(&(CatenaFinitaCfg){
                                     .src = " \t\r", .cap = CLI_POOL_BYTES, .byte = line[used - 1ULL]}) != NULL))
        {
            used -= 1ULL;
        }
        if (used == 0ULL)
        {
            return has_length;
        }
        static const char s_head[] = "Content-Length:";
        if ((used >= sizeof(s_head) - 1ULL) &&
            (memor.cmp(&(MemoriaCfg){.src = line, .other = s_head, .bytes = sizeof(s_head) - 1ULL}) == 0))
        {
            unsigned long long at = sizeof(s_head) - 1ULL;
            while ((at < used) &&
                   (cellul.chr(&(CatenaFinitaCfg){.src = " \t", .cap = CLI_POOL_BYTES, .byte = line[at]}) != NULL))
            {
                at += 1ULL;
            }
            at += ((at < used) && (line[at] == '+')) ? 1ULL : 0ULL;
            *length = 0ULL;
            has_length = (at < used);
            while (has_length && (at < used))
            {
                const unsigned long long digit = (unsigned long long)(line[at] - '0');
                has_length = (line[at] >= '0') && (line[at] <= '9') && (*length <= (~0ULL - digit) / 10ULL);
                *length = has_length ? (*length * 10ULL) + digit : *length;
                at += 1ULL;
            }
        }
    }
}

// whether `length` bytes of a body are a JSON body whole, its characters UTF-8 whole
EMBED_INLINE int wire_body_valid(const LspWireCtx *args)
{
    char tiny[1];
    VerbaLinea checked = {tiny, sizeof(tiny), 0ULL};
    return EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg, .bytes = args->body,
                      .size = args->length_value) &&
           EMBED_CALL(cli_json_tree_text.json_compact, CliJsonTreeTextCfg, .text = args->body,
                      .size = args->length_value, .out = &checked);
}

char *cli_lsp_message_alloc(const CliLspReaderSizeCfg *args)
{
    unsigned long long *size = args->size;
    for (;;)
    {
        unsigned long long length = 0ULL;
        if (!EMBED_CALL(wire_head_read, LspWireCtx, .reader = args->reader, .length = &length))
        {
            return NULL;
        }
        // one byte past the body, and an empty body is a cell too; a body the cells cannot meet is passed over whole
        char *const body = g_cli_guard->temporary_buf_alloc(length + 1ULL, args->owner);
        if (!EMBED_CALL(wire_bytes, LspWireCtx, .reader = args->reader, .into = body, .size = length))
        {
            g_cli_guard->persistent_buf_release(body, args->owner);
            return NULL;
        }
        if ((body != NULL) && EMBED_CALL(wire_body_valid, LspWireCtx, .body = body, .length_value = length))
        {
            body[length] = '\0';
            *size = length;
            return body;
        }
        g_cli_guard->persistent_buf_release(body, args->owner);
    }
}

int cli_lsp_read_message(const CliLspReaderCfg *args)
{
    unsigned long long *size = args->size;
    for (;;)
    {
        unsigned long long length = 0ULL;
        if (!EMBED_CALL(wire_head_read, LspWireCtx, .reader = args->reader, .length = &length))
        {
            return 0;
        }
        if (length > args->capacity)
        {
            // a body larger than the capacity is passed over whole, and the next message read
            if (!EMBED_CALL(wire_bytes, LspWireCtx, .reader = args->reader, .into = NULL, .size = length))
            {
                return 0;
            }
            continue;
        }
        if (!EMBED_CALL(wire_bytes, LspWireCtx, .reader = args->reader, .into = args->body_out, .size = length))
        {
            return 0;
        }
        *size = length;
        return EMBED_CALL(wire_body_valid, LspWireCtx, .body = args->body_out, .length_value = length);
    }
}

// where a JSON value that starts at `at` ends
static unsigned long long wire_value_end(const LspWireCtx *args)
{
    unsigned long long at = args->at;
    unsigned long long depth = 0ULL;
    int in_string = 0;
    while (at < args->size)
    {
        const char character = args->text[at];
        if (in_string)
        {
            at += (character == '\\') ? 2ULL : 1ULL;
            in_string = (character != '"') || (character == '\\');
            if ((character == '"') && (depth == 0ULL))
            {
                return at;
            }
            continue;
        }
        if (character == '"')
        {
            in_string = 1;
        }
        else if ((character == '{') || (character == '['))
        {
            depth += 1ULL;
        }
        else if ((character == '}') || (character == ']'))
        {
            if (depth == 0ULL)
            {
                return at;
            }
            depth -= 1ULL;
            if (depth == 0ULL)
            {
                return at + 1ULL;
            }
        }
        else if ((depth == 0ULL) &&
                 (cellul.chr(&(CatenaFinitaCfg){.src = ", \t\r\n", .cap = CLI_POOL_BYTES, .byte = character}) != NULL))
        {
            return at;
        }
        at += 1ULL;
    }
    return at;
}

// the whitespace JSON allows passed over from `at`
EMBED_INLINE unsigned long long wire_white(const LspWireCtx *args)
{
    unsigned long long at = args->at;
    while ((at < args->size) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r\n", .cap = CLI_POOL_BYTES, .byte = args->text[at]}) != NULL))
    {
        at += 1ULL;
    }
    return at;
}

int cli_lsp_member(const CliLspBodyCfg *args)
{
    unsigned long long *start = args->start;
    unsigned long long *length = args->length_pointer;
    unsigned long long at =
        EMBED_CALL(wire_white, LspWireCtx, .text = args->body, .size = args->size_value, .at = 0ULL);
    if ((at >= args->size_value) || (args->body[at] != '{'))
    {
        return 0;
    }
    at = EMBED_CALL(wire_white, LspWireCtx, .text = args->body, .size = args->size_value, .at = at + 1ULL);
    const unsigned long long required = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES});
    while ((at < args->size_value) && (args->body[at] == '"'))
    {
        const unsigned long long key_end =
            EMBED_CALL(wire_value_end, LspWireCtx, .text = args->body, .size = args->size_value, .at = at);
        const int named =
            (key_end - at == required + 2ULL) &&
            (memor.cmp(&(MemoriaCfg){.src = args->body + at + 1ULL, .other = args->name, .bytes = required}) == 0);
        at = EMBED_CALL(wire_white, LspWireCtx, .text = args->body, .size = args->size_value, .at = key_end);
        at = EMBED_CALL(wire_white, LspWireCtx, .text = args->body, .size = args->size_value, .at = at + 1ULL);
        const unsigned long long value_end =
            EMBED_CALL(wire_value_end, LspWireCtx, .text = args->body, .size = args->size_value, .at = at);
        if (named)
        {
            *start = at;
            *length = value_end - at;
            return 1;
        }
        at = EMBED_CALL(wire_white, LspWireCtx, .text = args->body, .size = args->size_value, .at = value_end);
        at = EMBED_CALL(wire_white, LspWireCtx, .text = args->body, .size = args->size_value, .at = at + 1ULL);
    }
    return 0;
}

void cli_lsp_uri_get(const CliLspCapacityPathCfg *args)
{
    static const char s_hex[17] = "0123456789ABCDEF";
    VerbaLinea out = {args->uri_out, args->capacity, 0ULL};
    mmgr_line_put(&out, ((args->path[0] == '/') || (args->path[0] == '\\')) ? "file://" : "file:///");
    const unsigned long long integer = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES});
    for (unsigned long long at = 0ULL; at < integer;)
    {
        const unsigned long long stop =
            EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = args->path, .at = at,
                       .size = integer, .classes = CLI_TEXT_CHARACTERS | CLI_TEXT_DIGITS, .set = "-._~/:\\");
        const embed_word from = out.at;
        const embed_word reserved = mmgr_line_put_n(&out, args->path + at, (embed_word)(stop - at));
        EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = out.out + from, .size = reserved,
                   .from = '\\', .to = '/');
        if (stop < integer)
        {
            const unsigned char byte = (unsigned char)args->path[stop];
            mmgr_line_ch(&out, '%');
            mmgr_line_ch(&out, s_hex[byte >> 4u]);
            mmgr_line_ch(&out, s_hex[byte & 0x0fu]);
        }
        at = stop + 1ULL;
    }
    mmgr_line_finish(&out);
}

// a hex digit's value, or 16 where it is none
EMBED_INLINE unsigned int wire_hex(const LspWireCtx *args)
{
    if ((args->character >= '0') && (args->character <= '9'))
    {
        return (unsigned int)(args->character - '0');
    }
    if ((args->character >= 'a') && (args->character <= 'f'))
    {
        return (unsigned int)(args->character - 'a' + 10);
    }
    if ((args->character >= 'A') && (args->character <= 'F'))
    {
        return (unsigned int)(args->character - 'A' + 10);
    }
    return 16u;
}

int cli_lsp_path_get(const CliLspLengthUriCfg *args)
{
    if ((args->length < 7ULL) || (memor.cmp(&(MemoriaCfg){.src = args->uri, .other = "file://", .bytes = 7u}) != 0))
    {
        return 0;
    }
    VerbaLinea out = {args->path_out, CLI_PATH_MAX_LEN, 0ULL};
    unsigned long long at = 7ULL;
    while (at < args->length)
    {
        if ((args->uri[at] == '%') && (at + 3ULL <= args->length))
        {
            // two hex digits, or a + and one, as Rust's u8 reading in base 16 takes them
            const unsigned int high =
                (args->uri[at + 1ULL] == '+') ? 0u : EMBED_CALL(wire_hex, LspWireCtx, .character = args->uri[at + 1ULL]);
            const unsigned int low = EMBED_CALL(wire_hex, LspWireCtx, .character = args->uri[at + 2ULL]);
            if ((high < 16u) && (low < 16u))
            {
                mmgr_line_ch(&out, (char)((high << 4u) | low));
                at += 3ULL;
                continue;
            }
        }
        mmgr_line_ch(&out, args->uri[at]);
        at += 1ULL;
    }
    if ((mmgr_line_finish(&out) == 0ULL) ||
        !EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg, .bytes = args->path_out, .size = out.at))
    {
        return 0;
    }
    // file:///D:/x names D:/x, and file:///x names /x
    if ((out.at > 2ULL) && (args->path_out[0] == '/') && (args->path_out[2] == ':'))
    {
        memor.move_down(&(MemoriaCfg){.dst = args->path_out, .src = args->path_out + 1, .bytes = out.at});
    }
    return 1;
}

void cli_lsp_key_get(const CliLspLengthCfg *args)
{
    if (!EMBED_CALL(cli_lsp_length_uri.path_get, CliLspLengthUriCfg, .uri = args->uri, .length = args->length,
                    .path_out = args->key))
    {
        VerbaLinea out = {args->key, CLI_PATH_MAX_LEN, 0ULL};
        mmgr_line_put_n(&out, args->uri, args->length);
        mmgr_line_finish(&out);
    }
    const unsigned long long key_size = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_POOL_BYTES});
    EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = args->key, .size = key_size, .from = '\\',
               .to = '/');
#if defined(_WIN32)
    EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .text_out = args->key, .size = key_size);
#endif
}

int cli_lsp_string(const CliLspCapacityCfg *args)
{
    CliJsonNode node[1];
    CfgJsonToken token[1];
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = node, .nodes_max = 1u, .text_out = args->out_capacity,
               .text_max = args->capacity);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ULL};
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_tokens.json_read, CliJsonTreeTextSizeTokensCfg, .json = &json,
                   .text = args->text, .size = args->length, .tokens = token, .tokens_max = 1u, .err_msg = &unread);
    unsigned long long size = 0ULL;
    const char *const characters = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                           .json_read = &json, .node = root, .length_pointer = &size);
    if ((characters == NULL) || (size >= args->capacity))
    {
        return 0;
    }
    memor.move_down(&(MemoriaCfg){.dst = args->out_capacity, .src = characters, .bytes = size});
    args->out_capacity[size] = '\0';
    return 1;
}

void cli_lsp_at(const CliLspPathCfg *args)
{
    char uri[CLI_PATH_MAX_LEN * 3u];
    EMBED_CALL(cli_lsp_capacity_path.uri_get, CliLspCapacityPathCfg, .path = args->path, .uri_out = uri,
               .capacity = sizeof(uri));
    mmgr_line_put(args->params_verba_linea_pointer, "{\"textDocument\":{\"uri\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->params_verba_linea_pointer,
               .text = uri, .length = cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_POOL_BYTES}));
    mmgr_line_put(args->params_verba_linea_pointer, "},\"position\":{\"line\":");
    mmgr_line_decimal(args->params_verba_linea_pointer, args->line, 1u);
    mmgr_line_put(args->params_verba_linea_pointer, ",\"character\":");
    mmgr_line_decimal(args->params_verba_linea_pointer, args->col, 1u);
    mmgr_line_ch(args->params_verba_linea_pointer, '}');
    if (args->added != NULL)
    {
        mmgr_line_ch(args->params_verba_linea_pointer, ',');
        mmgr_line_put(args->params_verba_linea_pointer, args->added);
    }
    mmgr_line_ch(args->params_verba_linea_pointer, '}');
}
