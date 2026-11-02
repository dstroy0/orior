// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sea.c: the modules a script requires from outside Node, the fuse Node reads to know a blob was added, a Windows
// program's signature, and the note a Linux program's blob goes in
#include "sea.h"
#include "../text/text.h"

// the fuse Node reads to know a blob was added, as Node's documentation gives it, and the name the blob is added under
static const char s_fuse[] = "NODE_SEA_FUSE_fce680ab2cc467b6e072b8b5df1996b2:0";

static const char s_resource[] = "NODE_SEA_BLOB";

// the modules Node contains itself, which a single executable can require
static const char *const s_builtin[51] = {"assert",
                                          "async_hooks",
                                          "buffer",
                                          "child_process",
                                          "cluster",
                                          "console",
                                          "constants",
                                          "crypto",
                                          "dgram",
                                          "diagnostics_channel",
                                          "dns",
                                          "domain",
                                          "events",
                                          "fs",
                                          "http",
                                          "http2",
                                          "https",
                                          "inspector",
                                          "module",
                                          "net",
                                          "os",
                                          "path",
                                          "perf_hooks",
                                          "process",
                                          "punycode",
                                          "querystring",
                                          "readline",
                                          "repl",
                                          "stream",
                                          "string_decoder",
                                          "sys",
                                          "timers",
                                          "tls",
                                          "trace_events",
                                          "tty",
                                          "url",
                                          "util",
                                          "v8",
                                          "vm",
                                          "wasi",
                                          "worker_threads",
                                          "zlib",
                                          "fs/promises",
                                          "path/posix",
                                          "path/win32",
                                          "stream/promises",
                                          "stream/web",
                                          "timers/promises",
                                          "util/types",
                                          "dns/promises",
                                          "readline/promises"};

unsigned int cli_sea_outside(const CliSeaCfg *args)
{
    const char *text = args->text;
    static const char *const s_marks[4] = {"require(", "import(", "from ", "import "};
    unsigned int count = 0u;
    for (unsigned int mark = 0u; mark < 4u; mark += 1u)
    {
        const char *rest = text;
        for (const char *at = cellul.find(&(CatenaFinitaCfg){
                 .src = rest, .cap = CLI_CELLS_POOL, .other = s_marks[mark], .other_cap = CLI_CELLS_POOL});
             at != NULL; at = cellul.find(&(CatenaFinitaCfg){
                             .src = rest, .cap = CLI_CELLS_POOL, .other = s_marks[mark], .other_cap = CLI_CELLS_POOL}))
        {
            rest = at + cellul.len(&(CatenaFinitaCfg){.src = s_marks[mark], .cap = CLI_CELLS_POOL});
            const char *trimmed = rest;
            trimmed += EMBED_CALL(
                cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = trimmed, .at = 0ull,
                .size = cellul.len(&(CatenaFinitaCfg){.src = trimmed, .cap = CLI_CELLS_POOL}), .set = CLI_TEXT_WHITE);
            const char quote = trimmed[0];
            if ((quote != '"') && (quote != '\'') && (quote != '`'))
            {
                continue;
            }
            const char *const name = trimmed + 1;
            const char *end = name;
            const char closing[2] = {quote, '\0'};
            end +=
                EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = end, .at = 0ull,
                           .size = cellul.len(&(CatenaFinitaCfg){.src = end, .cap = CLI_CELLS_POOL}), .set = closing);
            const unsigned long long length = (unsigned long long)(end - name);
            int bare = (length >= 5ull) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "node:", .bytes = 5u}) == 0);
            for (unsigned int at_builtin = 0u; !bare && (at_builtin < 51u); at_builtin += 1u)
            {
                bare =
                    (cellul.len(&(CatenaFinitaCfg){.src = s_builtin[at_builtin], .cap = CLI_CELLS_POOL}) == length) &&
                    (memor.cmp(&(MemoriaCfg){.src = s_builtin[at_builtin], .other = name, .bytes = length}) == 0);
            }
            int known = 0;
            for (unsigned int at_found = 0u; !known && (at_found < count); at_found += 1u)
            {
                known =
                    (cellul.len(&(CatenaFinitaCfg){.src = args->found[at_found], .cap = CLI_CELLS_POOL}) == length) &&
                    (memor.cmp(&(MemoriaCfg){.src = args->found[at_found], .other = name, .bytes = length}) == 0);
            }
            if (!bare && (length != 0ull) && !known && (count < args->room))
            {
                VerbaLinea line = {args->found[count], CLI_NAME_MAX_LEN, 0ull};
                mmgr_line_put_n(&line, name, length);
                mmgr_line_finish(&line);
                count += 1u;
            }
        }
    }
    return count;
}

int cli_sea_fuse_at(const CliSeaErrMsgProgramSizeCfg *args)
{
    unsigned long long *at = args->at;
    const unsigned long long length = sizeof(s_fuse) - 1u;
    for (unsigned long long place = 0ull; (place + length) <= args->size; place += 1ull)
    {
        if ((args->program[place] == (unsigned char)s_fuse[0]) &&
            (memor.cmp(&(MemoriaCfg){.src = args->program + place, .other = s_fuse, .bytes = length}) == 0))
        {
            *at = place + length - 1ull;
            return 1;
        }
    }
    mmgr_line_put(args->err_msg, "node contains no single executable's fuse, or contains it turned already");
    return 0;
}

typedef struct
{
    const unsigned char *bytes;
    unsigned long long size;
    unsigned long long at;
    unsigned int width;
    unsigned long long *value;
    unsigned char *bytes_out;
    unsigned long long value_value;
} SeaCtx;
// a little-endian number of `width` bytes at `at`: 1 with it into `value`, or 0 where the bytes end first
EMBED_INLINE int cli_number(const SeaCtx *args)
{
    unsigned long long *value = args->value;
    if ((args->at + args->width) > args->size)
    {
        return 0;
    }
    *value = 0ull;
    for (unsigned int byte = 0u; byte < args->width; byte += 1u)
    {
        *value |= (unsigned long long)args->bytes[args->at + byte] << (8u * byte);
    }
    return 1;
}

// a little-endian number of `width` bytes written at `at`
EMBED_INLINE void cli_put(const SeaCtx *args)
{
    for (unsigned int byte = 0u; byte < args->width; byte += 1u)
    {
        // each byte of the number, its low eight bits
        args->bytes_out[args->at + byte] = (unsigned char)(args->value_value >> (8u * byte));
    }
}

int cli_sea_signature(const CliSeaErrMsgProgramCfg *args)
{
    unsigned long long *entry = args->entry;
    unsigned long long *length = args->length;
    static const char s_ends[] = "the program ends inside its own headers";
    unsigned long long pe = 0ull;
    if ((args->size < 2ull) || (args->program[0] != 'M') || (args->program[1] != 'Z'))
    {
        mmgr_line_put(args->err_msg, "node is no Windows program");
        return -1;
    }
    if (!EMBED_CALL(cli_number, SeaCtx, .bytes = args->program, .size = args->size, .at = 0x3cull, .width = 4u,
                    .value = &pe))
    {
        mmgr_line_put(args->err_msg, s_ends);
        return -1;
    }
    if (((pe + 4ull) > args->size) ||
        (memor.cmp(&(MemoriaCfg){.src = args->program + pe, .other = "PE\0\0", .bytes = 4u}) != 0))
    {
        mmgr_line_put(args->err_msg, "node is no Windows program");
        return -1;
    }
    const unsigned long long optional = pe + 24ull;
    unsigned long long magic = 0ull;
    if (!EMBED_CALL(cli_number, SeaCtx, .bytes = args->program, .size = args->size, .at = optional, .width = 2u,
                    .value = &magic))
    {
        mmgr_line_put(args->err_msg, s_ends);
        return -1;
    }
    if ((magic != 0x20bull) && (magic != 0x10bull))
    {
        mmgr_line_put(args->err_msg, "node's optional header is of the unknown kind 0x");
        mmgr_line_hex(args->err_msg, magic, 1u);
        return -1;
    }
    *entry = optional + ((magic == 0x20bull) ? 112ull : 96ull) + (4ull * 8ull);
    unsigned long long offset = 0ull;
    unsigned long long table = 0ull;
    if (!EMBED_CALL(cli_number, SeaCtx, .bytes = args->program, .size = args->size, .at = *entry, .width = 4u,
                    .value = &offset) ||
        !EMBED_CALL(cli_number, SeaCtx, .bytes = args->program, .size = args->size, .at = *entry + 4ull, .width = 4u,
                    .value = &table))
    {
        mmgr_line_put(args->err_msg, s_ends);
        return -1;
    }
    if ((offset == 0ull) || (table == 0ull))
    {
        *entry = 0ull;
        *length = args->size;
        return 0;
    }
    *length = ((offset + table) == args->size) ? offset : args->size;
    return 1;
}

int cli_sea_note(const CliSeaErrMsgCfg *args)
{
    static const char s_ends[] = "the program ends inside its own headers";
    const unsigned long long page = 0x1000ull;
    if ((args->size < 6ull) ||
        (memor.cmp(&(MemoriaCfg){.src = args->program,
                                 .other = "\x7f"
                                          "ELF",
                                 .bytes = 4u}) != 0) ||
        (args->program[4] != 2u) || (args->program[5] != 1u))
    {
        mmgr_line_put(args->err_msg, "node is no 64-bit little-endian Linux program");
        return 0;
    }
    unsigned long long phoff = 0ull;
    unsigned long long entsize = 0ull;
    unsigned long long count = 0ull;
    if (!EMBED_CALL(cli_number, SeaCtx, .bytes = args->program, .size = args->size, .at = 0x20ull, .width = 8u,
                    .value = &phoff) ||
        !EMBED_CALL(cli_number, SeaCtx, .bytes = args->program, .size = args->size, .at = 0x36ull, .width = 2u,
                    .value = &entsize) ||
        !EMBED_CALL(cli_number, SeaCtx, .bytes = args->program, .size = args->size, .at = 0x38ull, .width = 2u,
                    .value = &count))
    {
        mmgr_line_put(args->err_msg, s_ends);
        return 0;
    }
    if (entsize != 56ull)
    {
        mmgr_line_put(args->err_msg, "node's program headers are ");
        mmgr_line_decimal(args->err_msg, entsize, 1u);
        mmgr_line_put(args->err_msg, " bytes each, not 56");
        return 0;
    }
    if (((phoff + (count * 56ull)) > args->size) || (count > CLI_SEA_HEADERS))
    {
        mmgr_line_put(args->err_msg, "node ends inside its program headers");
        return 0;
    }
    unsigned long long loaded_end = 0ull;
    unsigned long long bias = 0ull;
    int has_load = 0;
    unsigned long long last_load = count;
    for (unsigned long long at = 0ull; at < count; at += 1ull)
    {
        const unsigned char *const header = args->program + phoff + (at * 56ull);
        unsigned long long kind = 0ull;
        unsigned long long offset = 0ull;
        unsigned long long vaddr = 0ull;
        unsigned long long memsz = 0ull;
        EMBED_CALL(cli_number, SeaCtx, .bytes = header, .size = 56ull, .at = 0ull, .width = 4u, .value = &kind);
        EMBED_CALL(cli_number, SeaCtx, .bytes = header, .size = 56ull, .at = 8ull, .width = 8u, .value = &offset);
        EMBED_CALL(cli_number, SeaCtx, .bytes = header, .size = 56ull, .at = 16ull, .width = 8u, .value = &vaddr);
        EMBED_CALL(cli_number, SeaCtx, .bytes = header, .size = 56ull, .at = 40ull, .width = 8u, .value = &memsz);
        if (kind != 1ull)
        {
            continue;
        }
        if (!has_load)
        {
            bias = vaddr - offset;
            has_load = 1;
        }
        loaded_end = ((vaddr + memsz) > loaded_end) ? (vaddr + memsz) : loaded_end;
        last_load = at;
    }
    if (!has_load)
    {
        mmgr_line_put(args->err_msg, "node loads no segment");
        return 0;
    }
    loaded_end -= bias;
    const unsigned long long reach = (args->size > loaded_end) ? args->size : loaded_end;
    args->note->region = ((reach + page - 1ull) / page) * page;
    args->note->table_size = (count + 2ull) * 56ull;
    args->note->note_at = args->note->region + (((args->note->table_size + 3ull) / 4ull) * 4ull);
    // the note: its name's size and the blob's, its type 0, its name ended by a 0 and padded to four bytes
    const unsigned long long name_size = sizeof(s_resource);
    if (args->data_size > 0xffffffffull)
    {
        mmgr_line_put(args->err_msg, "the blob is past 4 GB");
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = args->note->head, .val = 0, .bytes = sizeof(args->note->head)});
    EMBED_CALL(cli_put, SeaCtx, .bytes_out = args->note->head, .at = 0ull, .value_value = name_size, .width = 4u);
    EMBED_CALL(cli_put, SeaCtx, .bytes_out = args->note->head, .at = 4ull, .value_value = args->data_size, .width = 4u);
    EMBED_CALL(cli_put, SeaCtx, .bytes_out = args->note->head, .at = 8ull, .value_value = 0ull, .width = 4u);
    memor.cpy(&(MemoriaCfg){.dst = args->note->head + 12, .src = s_resource, .bytes = name_size});
    args->note->head_size = 12ull + (((name_size + 3ull) / 4ull) * 4ull);
    const unsigned long long padded = ((args->data_size + 3ull) / 4ull) * 4ull;
    const unsigned long long length = args->note->note_at - args->note->region + args->note->head_size + padded;
    // the headers as they become: PT_PHDR set to the table's new place, a load of the new segment after the last load,
    // and the note last
    unsigned long long put = 0ull;
    for (unsigned long long at = 0ull; at <= count; at += 1ull)
    {
        unsigned char *const header = args->note->table + (put * 56ull);
        if (at == (last_load + 1ull))
        {
            const unsigned long long fields[7] = {1ull | (4ull << 32u),
                                                  args->note->region,
                                                  bias + args->note->region,
                                                  bias + args->note->region,
                                                  length,
                                                  length,
                                                  page};
            for (unsigned int field = 0u; field < 7u; field += 1u)
            {
                EMBED_CALL(cli_put, SeaCtx, .bytes_out = header, .at = field * 8ull, .value_value = fields[field],
                           .width = 8u);
            }
            put += 1ull;
        }
        if (at == count)
        {
            break;
        }
        unsigned char *const into = args->note->table + (put * 56ull);
        memor.cpy(&(MemoriaCfg){.dst = into, .src = args->program + phoff + (at * 56ull), .bytes = 56u});
        unsigned long long kind = 0ull;
        EMBED_CALL(cli_number, SeaCtx, .bytes = into, .size = 56ull, .at = 0ull, .width = 4u, .value = &kind);
        if (kind == 6ull)
        {
            const unsigned long long fields[7] = {6ull | (4ull << 32u),
                                                  args->note->region,
                                                  bias + args->note->region,
                                                  bias + args->note->region,
                                                  args->note->table_size,
                                                  args->note->table_size,
                                                  8ull};
            for (unsigned int field = 0u; field < 7u; field += 1u)
            {
                EMBED_CALL(cli_put, SeaCtx, .bytes_out = into, .at = field * 8ull, .value_value = fields[field],
                           .width = 8u);
            }
        }
        put += 1ull;
    }
    unsigned char *const last = args->note->table + (put * 56ull);
    const unsigned long long fields[7] = {4ull | (4ull << 32u),
                                          args->note->note_at,
                                          bias + args->note->note_at,
                                          bias + args->note->note_at,
                                          args->note->head_size + padded,
                                          args->note->head_size + padded,
                                          4ull};
    for (unsigned int field = 0u; field < 7u; field += 1u)
    {
        EMBED_CALL(cli_put, SeaCtx, .bytes_out = last, .at = field * 8ull, .value_value = fields[field], .width = 8u);
    }
    return 1;
}
