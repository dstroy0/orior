// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// binary_elf.c: a Linux program's dynamic section read: its needed libraries, its run paths and the glibc versions it
// asks for; each library looked for as the Linux loader looks for it, its own run paths first, then the folders the
// loader's settings name, then the system's own library folders; the libraries those load in turn
#include "../text/text.h"
#include "binary.h"

// A library to look for: its name, the file that loads it, that file's folder, and the run paths that file names,
// joined by `:`, and whether they are its RUNPATH, which stands before its RPATH where it has one
typedef struct
{
    char name[CLI_NAME_LONGEST];
    char by[CLI_NAME_LONGEST];
    char origin[CLI_PATH_LONGEST];
    char paths[CLI_PATH_LONGEST * 2u];
} CliElfQueued;

// the libraries a reading's queue is begun with, doubling as they are found
#define CLI_ELF_QUEUE_FIRST 16u

// A reading of a program's libraries, one cell for the call: the program's dynamic section and a library's, and the
// folders the loader's settings name; and the libraries queued, a cell of `room` grown as they are
typedef struct
{
    CliDynamic *dynamic;
    CliDynamic *more;
    char (*folders)[CLI_PATH_LONGEST];
    CliElfQueued *queue;
    unsigned int room;
} CliElfReading;

// a glibc version's numbers compared: below 0 where `a` is older
static int cli_glibc_order(const char *a, const char *b)
{
    a += (cellul.starts(&(CatenaFinitaCfg){.src = a, .other = "GLIBC_", .cap = CLI_CELLS_POOL})) ? 6 : 0;
    b += (cellul.starts(&(CatenaFinitaCfg){.src = b, .other = "GLIBC_", .cap = CLI_CELLS_POOL})) ? 6 : 0;
    for (;;)
    {
        unsigned long long left = 0ull;
        unsigned long long right = 0ull;
        const int has_left = (a[0] >= '0') && (a[0] <= '9');
        const int has_right = (b[0] >= '0') && (b[0] <= '9');
        while ((a[0] >= '0') && (a[0] <= '9'))
        {
            left = (left * 10ull) + (unsigned long long)(a[0] - '0');
            a += 1;
        }
        while ((b[0] >= '0') && (b[0] <= '9'))
        {
            right = (right * 10ull) + (unsigned long long)(b[0] - '0');
            b += 1;
        }
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
static int cli_string(const unsigned char *bytes, unsigned long long size, unsigned long long strings,
                      unsigned long long offset, char *out, unsigned long long room)
{
    return cli_binary_text(bytes, size, strings + offset, out, room);
}

int cli_elf_dynamic(const unsigned char *bytes, unsigned long long size, CliDynamic *found, CliLine *said)
{
    memor.set(&(MemoriaCfg){.dst = found, .val = 0, .bytes = sizeof(*found)});
    if ((size < 6u) || (bytes[4] != 2u) || (bytes[5] != 1u))
    {
        cli_line_text(said, "orior_cli reads 64-bit little-endian Linux programs");
        return 0;
    }
    if (size < 0x28u)
    {
        cli_line_text(said, "the file ends inside its headers");
        return 0;
    }
    const unsigned long long phoff = cli_binary_number(bytes, size, 0x20u, 8u);
    const unsigned long long count = cli_binary_number(bytes, size, 0x38u, 2u);
    unsigned long long dynamic_start = 0ull;
    unsigned long long dynamic_size = 0ull;
    int has_dynamic = 0;
    for (unsigned long long index = 0ull; index < count; index += 1ull)
    {
        const unsigned long long header = phoff + (index * 56ull);
        if (!has_dynamic && (cli_binary_number(bytes, size, header, 4u) == 2u))
        {
            dynamic_start = cli_binary_number(bytes, size, header + 8u, 8u);
            dynamic_size = cli_binary_number(bytes, size, header + 32u, 8u);
            has_dynamic = 1;
        }
    }
    if (!has_dynamic)
    {
        return 1;
    }
    unsigned long long strings = 0ull;
    int has_strings = 0;
    unsigned long long verneed = 0ull;
    int has_verneed = 0;
    unsigned long long verneed_count = 0ull;
    unsigned long long entries = 0ull;
    for (; (entries < (dynamic_size / 16u)) &&
           (cli_binary_number(bytes, size, dynamic_start + (entries * 16u), 8u) != 0ull);
         entries += 1ull)
    {
        const unsigned long long tag = cli_binary_number(bytes, size, dynamic_start + (entries * 16u), 8u);
        const unsigned long long value = cli_binary_number(bytes, size, dynamic_start + (entries * 16u) + 8u, 8u);
        // the place in the file of a virtual address, by the loaded segments
        for (unsigned long long segment = 0ull; segment < count; segment += 1ull)
        {
            const unsigned long long header = phoff + (segment * 56ull);
            const unsigned long long offset = cli_binary_number(bytes, size, header + 8u, 8u);
            const unsigned long long start = cli_binary_number(bytes, size, header + 16u, 8u);
            const unsigned long long length = cli_binary_number(bytes, size, header + 32u, 8u);
            if ((cli_binary_number(bytes, size, header, 4u) == 1u) && (value >= start) && (value < (start + length)))
            {
                if ((tag == 5ull) && !has_strings)
                {
                    strings = value - start + offset;
                    has_strings = 1;
                }
                if ((tag == 0x6ffffffeull) && !has_verneed)
                {
                    verneed = value - start + offset;
                    has_verneed = 1;
                }
            }
        }
        verneed_count = (tag == 0x6fffffffull) ? value : verneed_count;
    }
    if (!has_strings)
    {
        cli_line_text(said, "the program names no string table");
        return 0;
    }
    for (unsigned long long index = 0ull; index < entries; index += 1ull)
    {
        const unsigned long long tag = cli_binary_number(bytes, size, dynamic_start + (index * 16u), 8u);
        const unsigned long long value = cli_binary_number(bytes, size, dynamic_start + (index * 16u) + 8u, 8u);
        if ((tag == 1ull) && (found->count < 64u) &&
            cli_string(bytes, size, strings, value, found->needed[found->count], CLI_NAME_LONGEST))
        {
            found->count += 1u;
        }
        else if ((tag == 15ull) || (tag == 29ull))
        {
            char *const into = (tag == 15ull) ? found->rpath : found->runpath;
            char text[CLI_PATH_LONGEST * 2u];
            if (cli_string(bytes, size, strings, value, text, sizeof(text)))
            {
                CliLine line = {into, CLI_PATH_LONGEST * 2u,
                                cellul.len(&(CatenaFinitaCfg){.src = into, .cap = CLI_CELLS_POOL})};
                cli_line_text(&line, (into[0] != '\0') ? ":" : "");
                cli_line_text(&line, text);
                cli_line_closed(&line);
            }
        }
    }
    unsigned long long at = verneed;
    for (unsigned long long need = 0ull; has_verneed && (need < verneed_count); need += 1ull)
    {
        unsigned long long aux = at + cli_binary_number(bytes, size, at + 8u, 4u);
        const unsigned long long aux_count = cli_binary_number(bytes, size, at + 2u, 2u);
        for (unsigned long long each = 0ull; each < aux_count; each += 1ull)
        {
            char version[CLI_NAME_LONGEST];
            if (cli_string(bytes, size, strings, cli_binary_number(bytes, size, aux + 8u, 4u), version,
                           sizeof(version)) &&
                (cellul.starts(&(CatenaFinitaCfg){.src = version, .other = "GLIBC_2", .cap = CLI_CELLS_POOL})) &&
                (!found->has_glibc || (cli_glibc_order(version, found->glibc) > 0)))
            {
                memor.cpy(
                    &(MemoriaCfg){.dst = found->glibc,
                                  .src = version,
                                  .bytes = cellul.len(&(CatenaFinitaCfg){.src = version, .cap = CLI_CELLS_POOL}) + 1u});
                found->has_glibc = 1;
            }
            const unsigned long long next = cli_binary_number(bytes, size, aux + 12u, 4u);
            if (next == 0ull)
            {
                break;
            }
            aux += next;
        }
        const unsigned long long next = cli_binary_number(bytes, size, at + 12u, 4u);
        if (next == 0ull)
        {
            break;
        }
        at += next;
    }
    return 1;
}

// the folder of a path into `out`, which holds CLI_PATH_LONGEST, "." where it has none
static void cli_folder_of(const char *path, char *out)
{
    memor.cpy(&(MemoriaCfg){
        .dst = out, .src = path, .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    char *const slash = cli_text_last(out, cellul.len(&(CatenaFinitaCfg){.src = out, .cap = CLI_CELLS_POOL}), '/');
    if (slash != NULL)
    {
        slash[0] = '\0';
    }
    else
    {
        memor.cpy(&(MemoriaCfg){
            .dst = out, .src = ".", .bytes = cellul.len(&(CatenaFinitaCfg){.src = ".", .cap = CLI_CELLS_POOL}) + 1u});
    }
}

// a library queued with the run paths its loader names, its RUNPATH where it has one, else its RPATH
static unsigned int cli_queued(CliElfReading *reading, unsigned int queued, const char *name, const char *by,
                               const char *origin, const CliDynamic *dynamic)
{
    if ((queued == reading->room) && (reading->room < CLI_BINARY_LIBRARIES))
    {
        const unsigned int doubled = (reading->room == 0u) ? CLI_ELF_QUEUE_FIRST : (reading->room * 2u);
        const unsigned int room = (doubled < CLI_BINARY_LIBRARIES) ? doubled : CLI_BINARY_LIBRARIES;
        CliElfQueued *const grown = cli_cell_taken(room * sizeof(*grown), "the libraries a program loads");
        if (grown != NULL)
        {
            if (queued != 0u)
            {
                memor.cpy(&(MemoriaCfg){.dst = grown, .src = reading->queue, .bytes = queued * sizeof(*grown)});
            }
            cli_cell_given(reading->queue);
            reading->queue = grown;
            reading->room = room;
        }
    }
    if (queued >= reading->room)
    {
        return queued;
    }
    CliElfQueued *const one = &reading->queue[queued];
    memor.cpy(&(MemoriaCfg){.dst = one->name,
                            .src = name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){
        .dst = one->by, .src = by, .bytes = cellul.len(&(CatenaFinitaCfg){.src = by, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = one->origin,
                            .src = origin,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = origin, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){
        .dst = one->paths,
        .src = (dynamic->runpath[0] != '\0') ? dynamic->runpath : dynamic->rpath,
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = (dynamic->runpath[0] != '\0') ? dynamic->runpath : dynamic->rpath,
                                               .cap = CLI_CELLS_POOL}) +
                 1u});
    return queued + 1u;
}

// a run path with $ORIGIN and ${ORIGIN} read as the folder of the file that names it into `out`
static void cli_expanded(const char *entry, unsigned long long length, const char *origin, char *out)
{
    CliLine line = {out, CLI_PATH_LONGEST, 0ull};
    for (unsigned long long at = 0ull; at < length;)
    {
        if (((length - at) >= 9u) &&
            (memor.cmp(&(MemoriaCfg){.src = entry + at, .other = "${ORIGIN}", .bytes = 9u}) == 0))
        {
            cli_line_text(&line, origin);
            at += 9ull;
        }
        else if (((length - at) >= 7u) &&
                 (memor.cmp(&(MemoriaCfg){.src = entry + at, .other = "$ORIGIN", .bytes = 7u}) == 0))
        {
            cli_line_text(&line, origin);
            at += 7ull;
        }
        else
        {
            cli_line_character(&line, entry[at]);
            at += 1ull;
        }
    }
    cli_line_closed(&line);
}

// the library found among a queued one's run paths, its path into `at`, and whether that path is beside it by $ORIGIN
static int cli_in_paths(const CliElfQueued *one, char *at, int *beside)
{
    const char *from = one->paths;
    for (const char *place = one->paths;; place += 1)
    {
        if ((place[0] == ':') || (place[0] == '\0'))
        {
            const unsigned long long length = (unsigned long long)(place - from);
            if (length != 0ull)
            {
                char folder[CLI_PATH_LONGEST];
                cli_expanded(from, length, one->origin, folder);
                CliLine line = {at, CLI_PATH_LONGEST, 0ull};
                cli_line_text(&line, folder);
                cli_line_character(&line, '/');
                cli_line_text(&line, one->name);
                if ((cli_line_closed(&line) != 0ull) && query_is_file(at))
                {
                    char entry[CLI_PATH_LONGEST];
                    CliLine kept = {entry, sizeof(entry), 0ull};
                    cli_line_counted(&kept, from, length);
                    cli_line_closed(&kept);
                    *beside =
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
static void cli_build_paths(const CliDynamic *dynamic, CliBinaryReport *report)
{
    for (unsigned int list = 0u; list < 2u; list += 1u)
    {
        const char *const text = (list == 0u) ? dynamic->rpath : dynamic->runpath;
        const char *from = text;
        for (const char *place = text;; place += 1)
        {
            if ((place[0] == ':') || (place[0] == '\0'))
            {
                const unsigned long long length = (unsigned long long)(place - from);
                if ((length != 0ull) &&
                    (!cellul.starts(&(CatenaFinitaCfg){.src = from, .other = "$ORIGIN", .cap = CLI_CELLS_POOL})) &&
                    (!cellul.starts(&(CatenaFinitaCfg){.src = from, .other = "${ORIGIN}", .cap = CLI_CELLS_POOL})) &&
                    (report->build_paths < CLI_BINARY_BUILD_PATHS))
                {
                    CliLine line = {report->build_path[report->build_paths], CLI_PATH_LONGEST, 0ull};
                    cli_line_counted(&line, from, length);
                    report->build_paths += (cli_line_closed(&line) != 0ull) ? 1u : 0u;
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
static int cli_queued_before(const CliElfReading *reading, unsigned int next)
{
    for (unsigned int at = 0u; at < next; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){
                .src = reading->queue[at].name, .other = reading->queue[next].name, .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return 0;
}

// the libraries the program loads read into the report, the reading's cells held: as cli_binary_elf_read says
static int cli_elf_read(CliElfReading *reading, const unsigned char *bytes, unsigned long long size, const char *path,
                        CliBinaryReport *report, CliLine *said)
{
    const unsigned int folders = cli_binary_linux_folders(reading->folders);
    CliDynamic *const dynamic = reading->dynamic;
    if (!cli_elf_dynamic(bytes, size, dynamic, said))
    {
        return 0;
    }
    report->has_glibc = dynamic->has_glibc;
    memor.cpy(
        &(MemoriaCfg){.dst = report->glibc,
                      .src = dynamic->glibc,
                      .bytes = cellul.len(&(CatenaFinitaCfg){.src = dynamic->glibc, .cap = CLI_CELLS_POOL}) + 1u});
    char origin[CLI_PATH_LONGEST];
    cli_folder_of(path, origin);
    const char *const program =
        (cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '/') != NULL)
            ? (cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '/') + 1)
            : path;
    unsigned int queued = 0u;
    for (unsigned int at = 0u; at < dynamic->count; at += 1u)
    {
        queued = cli_queued(reading, queued, dynamic->needed[at], program, origin, dynamic);
    }
    cli_build_paths(dynamic, report);
    for (unsigned int next = 0u; next < queued; next += 1u)
    {
        // the queue is read again at each library, as a library's own may move it into a larger cell
        const CliElfQueued *const one = &reading->queue[next];
        if (cli_queued_before(reading, next) || (report->libraries >= CLI_BINARY_LIBRARIES))
        {
            continue;
        }
        CliLibrary *const library = &report->library[report->libraries];
        report->libraries += 1u;
        memor.cpy(&(MemoriaCfg){.dst = library->name,
                                .src = one->name,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = one->name, .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(&(MemoriaCfg){.dst = library->by,
                                .src = one->by,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = one->by, .cap = CLI_CELLS_POOL}) + 1u});
        library->has_at = 1;
        int beside = 0;
        if (cli_in_paths(one, library->at, &beside))
        {
            library->found = beside ? CLI_FOUND_BESIDE : CLI_FOUND_BUILD_PATH;
        }
        else
        {
            library->has_at = 0;
            library->found = CLI_FOUND_MISSING;
            for (unsigned int at = 0u; !library->has_at && (at < folders); at += 1u)
            {
                CliLine line = {library->at, CLI_PATH_LONGEST, 0ull};
                cli_line_text(&line, reading->folders[at]);
                cli_line_character(&line, '/');
                cli_line_text(&line, one->name);
                library->has_at = (cli_line_closed(&line) != 0ull) && query_is_file(library->at);
            }
            library->found = library->has_at ? CLI_FOUND_SYSTEM : CLI_FOUND_MISSING;
        }
        if (!library->has_at || ((library->found != CLI_FOUND_BESIDE) && (library->found != CLI_FOUND_BUILD_PATH)))
        {
            continue;
        }
        unsigned long long more_size = 0ull;
        const unsigned char *const more = cli_file_held(library->at, &more_size);
        char unsaid[CLI_PATH_LONGEST];
        CliLine ignored = {unsaid, sizeof(unsaid), 0ull};
        CliDynamic *const loaded = reading->more;
        if ((more != NULL) && cli_elf_dynamic(more, more_size, loaded, &ignored))
        {
            if (loaded->has_glibc && (!report->has_glibc || (cli_glibc_order(loaded->glibc, report->glibc) > 0)))
            {
                memor.cpy(&(MemoriaCfg){
                    .dst = report->glibc,
                    .src = loaded->glibc,
                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = loaded->glibc, .cap = CLI_CELLS_POOL}) + 1u});
                report->has_glibc = 1;
            }
            char more_origin[CLI_PATH_LONGEST];
            cli_folder_of(library->at, more_origin);
            for (unsigned int at = 0u; at < loaded->count; at += 1u)
            {
                queued = cli_queued(reading, queued, loaded->needed[at], library->name, more_origin, loaded);
            }
        }
        cli_file_let_go(more, more_size);
    }
    return 1;
}

int cli_binary_elf_read(const unsigned char *bytes, unsigned long long size, const char *path, CliBinaryReport *report,
                        CliLine *said)
{
    // the two dynamic sections on the cell's boundary, then the folders
    const unsigned long long dynamics = 2ull * sizeof(CliDynamic);
    char *const cell = cli_cell_taken(dynamics + ((unsigned long long)CLI_BINARY_LINUX_FOLDERS * CLI_PATH_LONGEST),
                                      "a reading of a program's libraries");
    if (cell == NULL)
    {
        cli_line_text(said, "a reading of a program's libraries could not be given a cell");
        return 0;
    }
    CliElfReading reading = {(CliDynamic *)(void *)cell, ((CliDynamic *)(void *)cell) + 1,
                             (char (*)[CLI_PATH_LONGEST])(void *)(cell + dynamics), NULL, 0u};
    const int read = cli_elf_read(&reading, bytes, size, path, report, said);
    cli_cell_given(reading.queue);
    cli_cell_given(cell);
    return read;
}
