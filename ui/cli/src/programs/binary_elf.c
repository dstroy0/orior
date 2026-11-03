// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// binary_elf.c: a Linux program's dynamic section read: its needed libraries, its run paths and the glibc versions it
// requires; each library looked for as the Linux loader looks for it, its own run paths first, then the folders the
// loader's settings name, then the system's own library folders; the libraries those load in turn
#include "../command_line/cells.h"
#include "../text/text.h"
#include "binary.h"

// A library to look for: its name, the file that loads it, that file's folder, and the run paths that file names,
// joined by `:`, and whether they are its RUNPATH, which stands before its RPATH where it has one
typedef struct
{
    char name[CLI_NAME_MAX_LEN];
    char by[CLI_NAME_MAX_LEN];
    char origin[CLI_PATH_MAX_LEN];
    char paths[CLI_PATH_MAX_LEN * 2u];
} CliElfQueued;
// A reading of a program's libraries, one cell for the call: the program's dynamic section and a library's, and the
// folders the loader's settings name; and the libraries queued, a cell of `capacity` at their most
typedef struct
{
    CliDynamic *dynamic;
    CliDynamic *more;
    char (*folders)[CLI_PATH_MAX_LEN];
    CliElfQueued *queue;
    unsigned int capacity;
} CliElfReadState;

// the libraries a reading's queue is taken with

typedef struct
{
    const char *a;
    const char *b;
    const unsigned char *bytes;
    unsigned long long size;
    unsigned long long strings;
    unsigned long long offset;
    char *out;
    unsigned long long capacity;
    const char *path;
    CliElfReadState *read_state;
    unsigned int queued;
    const char *name;
    const char *by;
    const char *origin;
    const CliDynamic *dynamic;
    const char *entry;
    unsigned long long length;
    const CliElfQueued *one;
    char *at;
    int *adjacent;
    CliBinaryReport *report;
    const CliElfReadState *read_state_read;
    unsigned int next;
    VerbaLinea *err_msg;
} BinaryElfCtx;
// a glibc version's numbers compared: below 0 where `a` is older
EMBED_INLINE int cli_glibc_order(const BinaryElfCtx *args)
{
    const char *a = args->a;
    const char *b = args->b;
    a += (cellul.starts(&(CatenaFinitaCfg){.src = a, .other = "GLIBC_", .cap = CLI_CELLS_POOL})) ? 6 : 0;
    b += (cellul.starts(&(CatenaFinitaCfg){.src = b, .other = "GLIBC_", .cap = CLI_CELLS_POOL})) ? 6 : 0;
    for (;;)
    {
        unsigned long long left = 0ULL;
        unsigned long long right = 0ULL;
        const int has_left = (a[0] >= '0') && (a[0] <= '9');
        const int has_right = (b[0] >= '0') && (b[0] <= '9');
        unsigned long long end_left = 0ULL;
        left = EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = a, .at = 0ULL,
                          .size = cellul.len(&(CatenaFinitaCfg){.src = a, .cap = CLI_CELLS_POOL}), .end = &end_left);
        a += end_left;
        unsigned long long end_right = 0ULL;
        right = EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = b, .at = 0ULL,
                           .size = cellul.len(&(CatenaFinitaCfg){.src = b, .cap = CLI_CELLS_POOL}), .end = &end_right);
        b += end_right;
        if (!has_left || !has_right)
        {
            return has_left - has_right;
        }
        if (left != right)
        {
            return (left < right) ? -1 : 1;
        }
        a += (a[0] == '.') ? 1 : 0;
        b += (b[0] == '.') ? 1 : 0;
    }
}

// a text of the string table at `offset` of the dynamic section's strings
EMBED_INLINE int cli_string(const BinaryElfCtx *args)
{
    const unsigned char *bytes = args->bytes;
    unsigned long long size = args->size;
    char *out = args->out;
    unsigned long long capacity = args->capacity;
    return EMBED_CALL(cli_binary_bytes.text, CliBinaryBytesCfg, .bytes = bytes, .size = size,
                      .at = args->strings + args->offset, .out = out, .capacity = capacity);
}

int cli_elf_dynamic(const CliBinaryErrMsgBytesCfg *args)
{
    const unsigned char *bytes = args->bytes;
    memor.set(&(MemoriaCfg){.dst = args->found, .val = 0, .bytes = sizeof(*args->found)});
    if ((args->size < 6u) || (bytes[4] != 2u) || (bytes[5] != 1u))
    {
        mmgr_line_put(args->err_msg, "orior_cli reads 64-bit little-endian Linux programs");
        return 0;
    }
    if (args->size < 0x28u)
    {
        mmgr_line_put(args->err_msg, "the file ends inside its headers");
        return 0;
    }
    const unsigned long long phoff = EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg, .bytes = bytes,
                                                .size = args->size, .at = 0x20u, .width = 8u);
    const unsigned long long count = EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg, .bytes = bytes,
                                                .size = args->size, .at = 0x38u, .width = 2u);
    unsigned long long dynamic_start = 0ULL;
    unsigned long long dynamic_size = 0ULL;
    int has_dynamic = 0;
    for (unsigned long long index = 0ULL; index < count; index += 1ULL)
    {
        const unsigned long long header = phoff + (index * 56ULL);
        if (!has_dynamic && (EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg, .bytes = bytes,
                                        .size = args->size, .at = header, .width = 4u) == 2u))
        {
            dynamic_start = EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg, .bytes = bytes,
                                       .size = args->size, .at = header + 8u, .width = 8u);
            dynamic_size = EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg, .bytes = bytes,
                                      .size = args->size, .at = header + 32u, .width = 8u);
            has_dynamic = 1;
        }
    }
    if (!has_dynamic)
    {
        return 1;
    }
    unsigned long long strings = 0ULL;
    int has_strings = 0;
    unsigned long long verneed = 0ULL;
    int has_verneed = 0;
    unsigned long long verneed_count = 0ULL;
    unsigned long long entries = 0ULL;
    for (; (entries < (dynamic_size / 16u)) &&
           (EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg, .bytes = bytes, .size = args->size,
                       .at = dynamic_start + (entries * 16u), .width = 8u) != 0ULL);
         entries += 1ULL)
    {
        const unsigned long long tag =
            EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg, .bytes = bytes, .size = args->size,
                       .at = dynamic_start + (entries * 16u), .width = 8u);
        const unsigned long long value =
            EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg, .bytes = bytes, .size = args->size,
                       .at = dynamic_start + (entries * 16u) + 8u, .width = 8u);
        // the place in the file of a virtual address, by the loaded segments
        for (unsigned long long segment = 0ULL; segment < count; segment += 1ULL)
        {
            const unsigned long long header = phoff + (segment * 56ULL);
            const unsigned long long offset =
                EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg, .bytes = bytes, .size = args->size,
                           .at = header + 8u, .width = 8u);
            const unsigned long long start =
                EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg, .bytes = bytes, .size = args->size,
                           .at = header + 16u, .width = 8u);
            const unsigned long long length =
                EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg, .bytes = bytes, .size = args->size,
                           .at = header + 32u, .width = 8u);
            if ((EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg, .bytes = bytes, .size = args->size,
                            .at = header, .width = 4u) == 1u) &&
                (value >= start) && (value < (start + length)))
            {
                if ((tag == 5ULL) && !has_strings)
                {
                    strings = value - start + offset;
                    has_strings = 1;
                }
                if ((tag == 0x6ffffffeULL) && !has_verneed)
                {
                    verneed = value - start + offset;
                    has_verneed = 1;
                }
            }
        }
        verneed_count = (tag == 0x6fffffffULL) ? value : verneed_count;
    }
    if (!has_strings)
    {
        mmgr_line_put(args->err_msg, "the program names no string table");
        return 0;
    }
    for (unsigned long long index = 0ULL; index < entries; index += 1ULL)
    {
        const unsigned long long tag = EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg, .bytes = bytes,
                                                  .size = args->size, .at = dynamic_start + (index * 16u), .width = 8u);
        const unsigned long long value =
            EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg, .bytes = bytes, .size = args->size,
                       .at = dynamic_start + (index * 16u) + 8u, .width = 8u);
        if ((tag == 1ULL) && (args->found->count < 64u) &&
            EMBED_CALL(cli_string, BinaryElfCtx, .bytes = bytes, .size = args->size, .strings = strings,
                       .offset = value, .out = args->found->needed[args->found->count], .capacity = CLI_NAME_MAX_LEN))
        {
            args->found->count += 1u;
        }
        else if ((tag == 15ULL) || (tag == 29ULL))
        {
            char *const into = (tag == 15ULL) ? args->found->rpath : args->found->runpath;
            char text[CLI_PATH_MAX_LEN * 2u];
            if (EMBED_CALL(cli_string, BinaryElfCtx, .bytes = bytes, .size = args->size, .strings = strings,
                           .offset = value, .out = text, .capacity = sizeof(text)))
            {
                VerbaLinea line = {into, CLI_PATH_MAX_LEN * 2u,
                                   cellul.len(&(CatenaFinitaCfg){.src = into, .cap = CLI_CELLS_POOL})};
                mmgr_line_put(&line, (into[0] != '\0') ? ":" : "");
                mmgr_line_put(&line, text);
                mmgr_line_finish(&line);
            }
        }
    }
    unsigned long long at = verneed;
    for (unsigned long long need = 0ULL; has_verneed && (need < verneed_count); need += 1ULL)
    {
        unsigned long long aux = at + EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg, .bytes = bytes,
                                                 .size = args->size, .at = at + 8u, .width = 4u);
        const unsigned long long aux_count = EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg,
                                                        .bytes = bytes, .size = args->size, .at = at + 2u, .width = 2u);
        for (unsigned long long each = 0ULL; each < aux_count; each += 1ULL)
        {
            char version[CLI_NAME_MAX_LEN];
            if (EMBED_CALL(cli_string, BinaryElfCtx, .bytes = bytes, .size = args->size, .strings = strings,
                           .offset = EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg, .bytes = bytes,
                                                .size = args->size, .at = aux + 8u, .width = 4u),
                           .out = version, .capacity = sizeof(version)) &&
                (cellul.starts(&(CatenaFinitaCfg){.src = version, .other = "GLIBC_2", .cap = CLI_CELLS_POOL})) &&
                (!args->found->has_glibc ||
                 (EMBED_CALL(cli_glibc_order, BinaryElfCtx, .a = version, .b = args->found->glibc) > 0)))
            {
                memor.cpy(
                    &(MemoriaCfg){.dst = args->found->glibc,
                                  .src = version,
                                  .bytes = cellul.len(&(CatenaFinitaCfg){.src = version, .cap = CLI_CELLS_POOL}) + 1u});
                args->found->has_glibc = 1;
            }
            const unsigned long long next =
                EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg, .bytes = bytes, .size = args->size,
                           .at = aux + 12u, .width = 4u);
            if (next == 0ULL)
            {
                break;
            }
            aux += next;
        }
        const unsigned long long next = EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg, .bytes = bytes,
                                                   .size = args->size, .at = at + 12u, .width = 4u);
        if (next == 0ULL)
        {
            break;
        }
        at += next;
    }
    return 1;
}

// the folder of a path into `out`, sized for CLI_PATH_MAX_LEN, "." where it has none
EMBED_INLINE void cli_folder_get(const BinaryElfCtx *args)
{
    memor.cpy(&(MemoriaCfg){.dst = args->out,
                            .src = args->path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}) + 1u});
    char *const slash = EMBED_CALL(
        cli_text_byte.last, CliTextByteCfg, .text = args->out,
        .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->out, .cap = CLI_CELLS_POOL}), .byte = '/');
    if (slash != NULL)
    {
        slash[0] = '\0';
    }
    else
    {
        memor.cpy(&(MemoriaCfg){.dst = args->out,
                                .src = ".",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = ".", .cap = CLI_CELLS_POOL}) + 1u});
    }
}

// a library queued with the run paths its loader names, its RUNPATH where it has one, else its RPATH
EMBED_INLINE unsigned int cli_on_queue(const BinaryElfCtx *args)
{
    if (args->read_state->capacity == 0u)
    {
        args->read_state->queue = g_cli_cells->temporary_buf_alloc(
            CLI_BINARY_LIBRARIES * sizeof(*args->read_state->queue), &g_cli_programs_cells);
        args->read_state->capacity = (args->read_state->queue != NULL) ? CLI_BINARY_LIBRARIES : 0u;
    }
    if (args->queued >= args->read_state->capacity)
    {
        return args->queued;
    }
    CliElfQueued *const one = &args->read_state->queue[args->queued];
    memor.cpy(&(MemoriaCfg){.dst = one->name,
                            .src = args->name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = one->by,
                            .src = args->by,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->by, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = one->origin,
                            .src = args->origin,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->origin, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){
        .dst = one->paths,
        .src = (args->dynamic->runpath[0] != '\0') ? args->dynamic->runpath : args->dynamic->rpath,
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = (args->dynamic->runpath[0] != '\0') ? args->dynamic->runpath
                                                                                          : args->dynamic->rpath,
                                               .cap = CLI_CELLS_POOL}) +
                 1u});
    return args->queued + 1u;
}

// a run path with $ORIGIN and ${ORIGIN} read as the folder of the file that names it into `out`
EMBED_INLINE void cli_on_expand(const BinaryElfCtx *args)
{
    VerbaLinea line = {args->out, CLI_PATH_MAX_LEN, 0ULL};
    for (unsigned long long at = 0ULL; at < args->length;)
    {
        if (((args->length - at) >= 9u) &&
            (memor.cmp(&(MemoriaCfg){.src = args->entry + at, .other = "${ORIGIN}", .bytes = 9u}) == 0))
        {
            mmgr_line_put(&line, args->origin);
            at += 9ULL;
        }
        else if (((args->length - at) >= 7u) &&
                 (memor.cmp(&(MemoriaCfg){.src = args->entry + at, .other = "$ORIGIN", .bytes = 7u}) == 0))
        {
            mmgr_line_put(&line, args->origin);
            at += 7ULL;
        }
        else
        {
            mmgr_line_ch(&line, args->entry[at]);
            at += 1ULL;
        }
    }
    mmgr_line_finish(&line);
}

// the library found among a queued one's run paths, its path into `at`, and whether that path is beside it by $ORIGIN
EMBED_INLINE int cli_in_paths(const BinaryElfCtx *args)
{
    int *adjacent = args->adjacent;
    const char *from = args->one->paths;
    for (const char *place = args->one->paths;; place += 1)
    {
        if ((place[0] == ':') || (place[0] == '\0'))
        {
            const unsigned long long length = (unsigned long long)(place - from);
            if (length != 0ULL)
            {
                char folder[CLI_PATH_MAX_LEN];
                EMBED_CALL(cli_on_expand, BinaryElfCtx, .entry = from, .length = length, .origin = args->one->origin,
                           .out = folder);
                VerbaLinea line = {args->at, CLI_PATH_MAX_LEN, 0ULL};
                mmgr_line_put(&line, folder);
                mmgr_line_ch(&line, '/');
                mmgr_line_put(&line, args->one->name);
                if ((mmgr_line_finish(&line) != 0ULL) &&
                    EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = args->at))
                {
                    char entry[CLI_PATH_MAX_LEN];
                    VerbaLinea entry_line = {entry, sizeof(entry), 0ULL};
                    mmgr_line_put_n(&entry_line, from, length);
                    mmgr_line_finish(&entry_line);
                    *adjacent =
                        cellul.find(&(CatenaFinitaCfg){
                            .src = entry, .cap = CLI_CELLS_POOL, .other = "ORIGIN", .other_cap = CLI_CELLS_POOL}) !=
                        NULL;
                    return 1;
                }
            }
            if (place[0] == '\0')
            {
                return 0;
            }
            from = place + 1;
        }
    }
}

// the run paths of the program's dynamic section that name no $ORIGIN, folders of the machine that built it, put in
// the report
EMBED_INLINE void cli_build_paths(const BinaryElfCtx *args)
{
    for (unsigned int list = 0u; list < 2u; list += 1u)
    {
        const char *const text = (list == 0u) ? args->dynamic->rpath : args->dynamic->runpath;
        const char *from = text;
        for (const char *place = text;; place += 1)
        {
            if ((place[0] == ':') || (place[0] == '\0'))
            {
                const unsigned long long length = (unsigned long long)(place - from);
                if ((length != 0ULL) &&
                    (!cellul.starts(&(CatenaFinitaCfg){.src = from, .other = "$ORIGIN", .cap = CLI_CELLS_POOL})) &&
                    (!cellul.starts(&(CatenaFinitaCfg){.src = from, .other = "${ORIGIN}", .cap = CLI_CELLS_POOL})) &&
                    (args->report->build_paths < CLI_BINARY_BUILD_PATHS))
                {
                    VerbaLinea line = {args->report->build_path[args->report->build_paths], CLI_PATH_MAX_LEN, 0ULL};
                    mmgr_line_put_n(&line, from, length);
                    args->report->build_paths += (mmgr_line_finish(&line) != 0ULL) ? 1u : 0u;
                }
                if (place[0] == '\0')
                {
                    break;
                }
                from = place + 1;
            }
        }
    }
}

// whether a library queued at `next` was queued before it, by its name
EMBED_INLINE int cli_queued_before(const BinaryElfCtx *args)
{
    for (unsigned int at = 0u; at < args->next; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->read_state_read->queue[at].name,
                                         .other = args->read_state_read->queue[args->next].name,
                                         .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return 0;
}

// the libraries the program loads read into the report, the reading's cells allocated: as cli_binary_elf_read specifies
EMBED_INLINE int cli_elf_read(const BinaryElfCtx *args)
{
    const unsigned char *bytes = args->bytes;
    unsigned long long size = args->size;
    const char *path = args->path;
    VerbaLinea *err_msg = args->err_msg;
    const unsigned int folders =
        EMBED_CALL(cli_binary.linux_folders, CliBinaryCfg, .folders = args->read_state->folders);
    CliDynamic *const dynamic = args->read_state->dynamic;
    if (!EMBED_CALL(cli_binary_err_msg_bytes.elf_dynamic, CliBinaryErrMsgBytesCfg, .bytes = bytes, .size = size,
                    .found = dynamic, .err_msg = err_msg))
    {
        return 0;
    }
    args->report->has_glibc = dynamic->has_glibc;
    memor.cpy(
        &(MemoriaCfg){.dst = args->report->glibc,
                      .src = dynamic->glibc,
                      .bytes = cellul.len(&(CatenaFinitaCfg){.src = dynamic->glibc, .cap = CLI_CELLS_POOL}) + 1u});
    char origin[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_folder_get, BinaryElfCtx, .path = path, .out = origin);
    const char *const program =
        (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = path,
                    .length_value = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}),
                    .byte = '/') != NULL)
            ? (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = path,
                          .length_value = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}),
                          .byte = '/') +
               1)
            : path;
    unsigned int queued = 0u;
    for (unsigned int at = 0u; at < dynamic->count; at += 1u)
    {
        queued = EMBED_CALL(cli_on_queue, BinaryElfCtx, .read_state = args->read_state, .queued = queued,
                            .name = dynamic->needed[at], .by = program, .origin = origin, .dynamic = dynamic);
    }
    EMBED_CALL(cli_build_paths, BinaryElfCtx, .dynamic = dynamic, .report = args->report);
    for (unsigned int next = 0u; next < queued; next += 1u)
    {
        const CliElfQueued *const one = &args->read_state->queue[next];
        if (EMBED_CALL(cli_queued_before, BinaryElfCtx, .read_state_read = args->read_state, .next = next) ||
            (args->report->libraries >= CLI_BINARY_LIBRARIES))
        {
            continue;
        }
        CliLibrary *const library = &args->report->library[args->report->libraries];
        args->report->libraries += 1u;
        memor.cpy(&(MemoriaCfg){.dst = library->name,
                                .src = one->name,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = one->name, .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(&(MemoriaCfg){.dst = library->by,
                                .src = one->by,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = one->by, .cap = CLI_CELLS_POOL}) + 1u});
        library->has_at = 1;
        int adjacent = 0;
        if (EMBED_CALL(cli_in_paths, BinaryElfCtx, .one = one, .at = library->at, .adjacent = &adjacent))
        {
            library->found = adjacent ? CLI_FOUND_ADJACENT : CLI_FOUND_BUILD_PATH;
        }
        else
        {
            library->has_at = 0;
            library->found = CLI_FOUND_MISSING;
            for (unsigned int at = 0u; !library->has_at && (at < folders); at += 1u)
            {
                VerbaLinea line = {library->at, CLI_PATH_MAX_LEN, 0ULL};
                mmgr_line_put(&line, args->read_state->folders[at]);
                mmgr_line_ch(&line, '/');
                mmgr_line_put(&line, one->name);
                library->has_at =
                    (mmgr_line_finish(&line) != 0ULL) &&
                    EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = library->at);
            }
            library->found = library->has_at ? CLI_FOUND_SYSTEM : CLI_FOUND_MISSING;
        }
        if (!library->has_at || ((library->found != CLI_FOUND_ADJACENT) && (library->found != CLI_FOUND_BUILD_PATH)))
        {
            continue;
        }
        unsigned long long more_size = 0ULL;
        const unsigned char *const more = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = library->at,
                                                     .size_pointer = &more_size, .owner = &g_cli_programs_cells);
        char err_buf[CLI_PATH_MAX_LEN];
        VerbaLinea ignored = {err_buf, sizeof(err_buf), 0ULL};
        CliDynamic *const loaded = args->read_state->more;
        if ((more != NULL) && EMBED_CALL(cli_binary_err_msg_bytes.elf_dynamic, CliBinaryErrMsgBytesCfg, .bytes = more,
                                         .size = more_size, .found = loaded, .err_msg = &ignored))
        {
            if (loaded->has_glibc &&
                (!args->report->has_glibc ||
                 (EMBED_CALL(cli_glibc_order, BinaryElfCtx, .a = loaded->glibc, .b = args->report->glibc) > 0)))
            {
                memor.cpy(&(MemoriaCfg){
                    .dst = args->report->glibc,
                    .src = loaded->glibc,
                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = loaded->glibc, .cap = CLI_CELLS_POOL}) + 1u});
                args->report->has_glibc = 1;
            }
            char more_origin[CLI_PATH_MAX_LEN];
            EMBED_CALL(cli_folder_get, BinaryElfCtx, .path = library->at, .out = more_origin);
            for (unsigned int at = 0u; at < loaded->count; at += 1u)
            {
                queued = EMBED_CALL(cli_on_queue, BinaryElfCtx, .read_state = args->read_state, .queued = queued,
                                    .name = loaded->needed[at], .by = library->name, .origin = more_origin,
                                    .dynamic = loaded);
            }
        }
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = more,
                   .owner = &g_cli_programs_cells);
    }
    return 1;
}

int cli_binary_elf_read(const CliBinaryPathReportCfg *args)
{
    // the two dynamic sections on the cell's boundary, then the folders
    const unsigned long long dynamics = 2ULL * sizeof(CliDynamic);
    char *const cell = g_cli_cells->temporary_buf_alloc(
        dynamics + ((unsigned long long)CLI_BINARY_LINUX_FOLDERS * CLI_PATH_MAX_LEN), &g_cli_programs_cells);
    if (cell == NULL)
    {
        mmgr_line_put(args->err_msg, "a reading of a program's libraries could not be given a cell");
        return 0;
    }
    CliElfReadState read_state = {(CliDynamic *)(void *)cell, ((CliDynamic *)(void *)cell) + 1,
                                  (char (*)[CLI_PATH_MAX_LEN])(void *)(cell + dynamics), NULL, 0u};
    const int read =
        EMBED_CALL(cli_elf_read, BinaryElfCtx, .read_state = &read_state, .bytes = args->bytes, .size = args->size,
                   .path = args->path, .report = args->report, .err_msg = args->err_msg);
    g_cli_cells->persistent_buf_release(read_state.queue, &g_cli_programs_cells);
    g_cli_cells->persistent_buf_release(cell, &g_cli_programs_cells);
    return read;
}
