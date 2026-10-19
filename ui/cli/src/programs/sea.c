// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sea.c: the modules a script asks for from outside Node, the fuse Node reads to know a blob was added, a Windows
// program's signature, and the note a Linux program's blob goes in
#include "sea.h"

// the fuse Node reads to know a blob was added, as Node's documentation gives it, and the name the blob is added under
static const char s_fuse[] = "NODE_SEA_FUSE_fce680ab2cc467b6e072b8b5df1996b2:0";

static const char s_resource[] = "NODE_SEA_BLOB";

// the modules Node holds itself, which a single executable can require
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

unsigned int cli_sea_outside(const char *text, char (*found)[CLI_NAME_LONGEST], unsigned int room)
{
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
            while ((trimmed[0] == ' ') || ((trimmed[0] >= '\t') && (trimmed[0] <= '\r')))
            {
                trimmed += 1;
            }
            const char quote = trimmed[0];
            if ((quote != '"') && (quote != '\'') && (quote != '`'))
            {
                continue;
            }
            const char *const name = trimmed + 1;
            const char *end = name;
            while ((end[0] != '\0') && (end[0] != quote))
            {
                end += 1;
            }
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
                known = (cellul.len(&(CatenaFinitaCfg){.src = found[at_found], .cap = CLI_CELLS_POOL}) == length) &&
                        (memor.cmp(&(MemoriaCfg){.src = found[at_found], .other = name, .bytes = length}) == 0);
            }
            if (!bare && (length != 0ull) && !known && (count < room))
            {
                CliLine line = {found[count], CLI_NAME_LONGEST, 0ull};
                cli_line_counted(&line, name, length);
                cli_line_closed(&line);
                count += 1u;
            }
        }
    }
    return count;
}

int cli_sea_fuse_at(const unsigned char *program, unsigned long long size, unsigned long long *at, CliLine *said)
{
    const unsigned long long length = sizeof(s_fuse) - 1u;
    for (unsigned long long place = 0ull; (place + length) <= size; place += 1ull)
    {
        if ((program[place] == (unsigned char)s_fuse[0]) &&
            (memor.cmp(&(MemoriaCfg){.src = program + place, .other = s_fuse, .bytes = length}) == 0))
        {
            *at = place + length - 1ull;
            return 1;
        }
    }
    cli_line_text(said, "node holds no single executable's fuse, or holds it turned already");
    return 0;
}

// a little-endian number of `width` bytes at `at`: 1 with it into `value`, or 0 where the bytes end first
static int cli_number(const unsigned char *bytes, unsigned long long size, unsigned long long at, unsigned int width,
                      unsigned long long *value)
{
    if ((at + width) > size)
    {
        return 0;
    }
    *value = 0ull;
    for (unsigned int byte = 0u; byte < width; byte += 1u)
    {
        *value |= (unsigned long long)bytes[at + byte] << (8u * byte);
    }
    return 1;
}

// a little-endian number of `width` bytes written at `at`
static void cli_put(unsigned char *bytes, unsigned long long at, unsigned long long value, unsigned int width)
{
    for (unsigned int byte = 0u; byte < width; byte += 1u)
    {
        // each byte of the number, its low eight bits
        bytes[at + byte] = (unsigned char)(value >> (8u * byte));
    }
}

int cli_sea_signature(const unsigned char *program, unsigned long long size, unsigned long long *entry,
                      unsigned long long *length, CliLine *said)
{
    static const char s_ends[] = "the program ends inside its own headers";
    unsigned long long pe = 0ull;
    if ((size < 2ull) || (program[0] != 'M') || (program[1] != 'Z'))
    {
        cli_line_text(said, "node is no Windows program");
        return -1;
    }
    if (!cli_number(program, size, 0x3cull, 4u, &pe))
    {
        cli_line_text(said, s_ends);
        return -1;
    }
    if (((pe + 4ull) > size) || (memor.cmp(&(MemoriaCfg){.src = program + pe, .other = "PE\0\0", .bytes = 4u}) != 0))
    {
        cli_line_text(said, "node is no Windows program");
        return -1;
    }
    const unsigned long long optional = pe + 24ull;
    unsigned long long magic = 0ull;
    if (!cli_number(program, size, optional, 2u, &magic))
    {
        cli_line_text(said, s_ends);
        return -1;
    }
    if ((magic != 0x20bull) && (magic != 0x10bull))
    {
        cli_line_text(said, "node's optional header is of the unknown kind 0x");
        cli_line_hex(said, magic, 1u);
        return -1;
    }
    *entry = optional + ((magic == 0x20bull) ? 112ull : 96ull) + (4ull * 8ull);
    unsigned long long offset = 0ull;
    unsigned long long table = 0ull;
    if (!cli_number(program, size, *entry, 4u, &offset) || !cli_number(program, size, *entry + 4ull, 4u, &table))
    {
        cli_line_text(said, s_ends);
        return -1;
    }
    if ((offset == 0ull) || (table == 0ull))
    {
        *entry = 0ull;
        *length = size;
        return 0;
    }
    *length = ((offset + table) == size) ? offset : size;
    return 1;
}

int cli_sea_note(const unsigned char *program, unsigned long long size, unsigned long long data_size, CliSeaNote *note,
                 CliLine *said)
{
    static const char s_ends[] = "the program ends inside its own headers";
    const unsigned long long page = 0x1000ull;
    if ((size < 6ull) ||
        (memor.cmp(&(MemoriaCfg){.src = program,
                                 .other = "\x7f"
                                          "ELF",
                                 .bytes = 4u}) != 0) ||
        (program[4] != 2u) || (program[5] != 1u))
    {
        cli_line_text(said, "node is no 64-bit little-endian Linux program");
        return 0;
    }
    unsigned long long phoff = 0ull;
    unsigned long long entsize = 0ull;
    unsigned long long count = 0ull;
    if (!cli_number(program, size, 0x20ull, 8u, &phoff) || !cli_number(program, size, 0x36ull, 2u, &entsize) ||
        !cli_number(program, size, 0x38ull, 2u, &count))
    {
        cli_line_text(said, s_ends);
        return 0;
    }
    if (entsize != 56ull)
    {
        cli_line_text(said, "node's program headers are ");
        cli_line_decimal(said, entsize, 1u);
        cli_line_text(said, " bytes each, not 56");
        return 0;
    }
    if (((phoff + (count * 56ull)) > size) || (count > CLI_SEA_HEADERS))
    {
        cli_line_text(said, "node ends inside its program headers");
        return 0;
    }
    unsigned long long loaded_end = 0ull;
    unsigned long long bias = 0ull;
    int has_load = 0;
    unsigned long long last_load = count;
    for (unsigned long long at = 0ull; at < count; at += 1ull)
    {
        const unsigned char *const header = program + phoff + (at * 56ull);
        unsigned long long kind = 0ull;
        unsigned long long offset = 0ull;
        unsigned long long vaddr = 0ull;
        unsigned long long memsz = 0ull;
        cli_number(header, 56ull, 0ull, 4u, &kind);
        cli_number(header, 56ull, 8ull, 8u, &offset);
        cli_number(header, 56ull, 16ull, 8u, &vaddr);
        cli_number(header, 56ull, 40ull, 8u, &memsz);
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
        cli_line_text(said, "node loads no segment");
        return 0;
    }
    loaded_end -= bias;
    const unsigned long long reach = (size > loaded_end) ? size : loaded_end;
    note->region = ((reach + page - 1ull) / page) * page;
    note->table_size = (count + 2ull) * 56ull;
    note->note_at = note->region + (((note->table_size + 3ull) / 4ull) * 4ull);
    // the note: its name's size and the blob's, its type 0, its name ended by a 0 and padded to four bytes
    const unsigned long long name_size = sizeof(s_resource);
    if (data_size > 0xffffffffull)
    {
        cli_line_text(said, "the blob is past 4 GB");
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = note->head, .val = 0, .bytes = sizeof(note->head)});
    cli_put(note->head, 0ull, name_size, 4u);
    cli_put(note->head, 4ull, data_size, 4u);
    cli_put(note->head, 8ull, 0ull, 4u);
    memor.cpy(&(MemoriaCfg){.dst = note->head + 12, .src = s_resource, .bytes = name_size});
    note->head_size = 12ull + (((name_size + 3ull) / 4ull) * 4ull);
    const unsigned long long padded = ((data_size + 3ull) / 4ull) * 4ull;
    const unsigned long long length = note->note_at - note->region + note->head_size + padded;
    // the headers as they become: PT_PHDR set to the table's new place, a load of the new segment after the last load,
    // and the note last
    unsigned long long put = 0ull;
    for (unsigned long long at = 0ull; at <= count; at += 1ull)
    {
        unsigned char *const header = note->table + (put * 56ull);
        if (at == (last_load + 1ull))
        {
            const unsigned long long fields[7] = {
                1ull | (4ull << 32u), note->region, bias + note->region, bias + note->region, length, length, page};
            for (unsigned int field = 0u; field < 7u; field += 1u)
            {
                cli_put(header, field * 8ull, fields[field], 8u);
            }
            put += 1ull;
        }
        if (at == count)
        {
            break;
        }
        unsigned char *const into = note->table + (put * 56ull);
        memor.cpy(&(MemoriaCfg){.dst = into, .src = program + phoff + (at * 56ull), .bytes = 56u});
        unsigned long long kind = 0ull;
        cli_number(into, 56ull, 0ull, 4u, &kind);
        if (kind == 6ull)
        {
            const unsigned long long fields[7] = {
                6ull | (4ull << 32u), note->region, bias + note->region, bias + note->region, note->table_size,
                note->table_size,     8ull};
            for (unsigned int field = 0u; field < 7u; field += 1u)
            {
                cli_put(into, field * 8ull, fields[field], 8u);
            }
        }
        put += 1ull;
    }
    unsigned char *const last = note->table + (put * 56ull);
    const unsigned long long fields[7] = {
        4ull | (4ull << 32u),     note->note_at, bias + note->note_at, bias + note->note_at, note->head_size + padded,
        note->head_size + padded, 4ull};
    for (unsigned int field = 0u; field < 7u; field += 1u)
    {
        cli_put(last, field * 8ull, fields[field], 8u);
    }
    return 1;
}
