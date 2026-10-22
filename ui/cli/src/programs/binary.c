// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// binary.c: a Windows program's imports and delay-loaded imports, each looked for as Windows' loader looks for it, the
// libraries those load in turn; what a report tells; and the libraries found through a PATH copied beside the program
#include "binary.h"
#include "../toolchains/toolchains.h"

// the Visual C++ runtime's libraries, which its Redistributable installs, and their debug forms, which only Visual
// Studio installs
static const char *const s_redistributable[9] = {"vcruntime140.dll", "vcruntime140_1.dll", "msvcp140.dll",
                                                 "msvcp140_1.dll",   "msvcp140_2.dll",     "msvcp140_atomic_wait.dll",
                                                 "concrt140.dll",    "vccorlib140.dll",    "vcomp140.dll"};

static const char *const s_debug_runtime[7] = {"vcruntime140d.dll", "vcruntime140_1d.dll", "msvcp140d.dll",
                                               "msvcp140_1d.dll",   "msvcp140_2d.dll",     "ucrtbased.dll",
                                               "concrt140d.dll"};

// the queue of libraries to look for, each its name and the file that loads it, under the lock
typedef struct
{
    char name[CLI_NAME_LONGEST];
    char by[CLI_NAME_LONGEST];
} CliQueued;

static CliQueued s_queue[CLI_BINARY_LIBRARIES];
static char s_seen[CLI_BINARY_LIBRARIES][CLI_NAME_LONGEST];
static char s_system_path[CLI_PATH_FOLDERS][CLI_PATH_LONGEST];
static CliLock s_lock = CLI_LOCK_INIT;

unsigned long long cli_binary_number(const unsigned char *bytes, unsigned long long size, unsigned long long at,
                                     unsigned int width)
{
    if ((at + width) > size)
    {
        return 0ull;
    }
    unsigned long long value = 0ull;
    for (unsigned int byte = width; byte > 0u; byte -= 1u)
    {
        value = (value << 8u) | bytes[at + byte - 1u];
    }
    return value;
}

int cli_binary_text(const unsigned char *bytes, unsigned long long size, unsigned long long at, char *out,
                    unsigned long long room)
{
    unsigned long long end = at;
    while ((end < size) && (bytes[end] != 0u))
    {
        end += 1ull;
    }
    if (end >= size)
    {
        return 0;
    }
    CliLine line = {out, room, 0ull};
    // the bytes of a name read as the text they write
    cli_line_counted(&line, (const char *)(bytes + at), end - at);
    return cli_line_closed(&line) != 0ull;
}

// a name in lower case into `out`, which holds CLI_NAME_LONGEST
static void cli_lower(const char *name, char *out)
{
    unsigned long long at = 0ull;
    for (; (name[at] != '\0') && ((at + 1ull) < CLI_NAME_LONGEST); at += 1ull)
    {
        out[at] = ((name[at] >= 'A') && (name[at] <= 'Z')) ? (char)(name[at] + 32) : name[at];
    }
    out[at] = '\0';
}

static int cli_listed(const char *lower, const char *const *names, unsigned int count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = lower, .other = names[at], .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return 0;
}

// The place in the file of a virtual address of the program, by its sections: 1, or 0 where none holds it
static int cli_offset(const unsigned char *bytes, unsigned long long size, unsigned long long table,
                      unsigned int sections, unsigned long long rva, unsigned long long *offset)
{
    for (unsigned int index = 0u; index < sections; index += 1u)
    {
        const unsigned long long at = table + (index * 40ull);
        const unsigned long long virtual_size = cli_binary_number(bytes, size, at + 8u, 4u);
        const unsigned long long raw_size = cli_binary_number(bytes, size, at + 16u, 4u);
        const unsigned long long length = (virtual_size > raw_size) ? virtual_size : raw_size;
        const unsigned long long start = cli_binary_number(bytes, size, at + 12u, 4u);
        const unsigned long long raw = cli_binary_number(bytes, size, at + 20u, 4u);
        if ((at + 24ull) <= size && (rva >= start) && (rva < (start + length)))
        {
            *offset = rva - start + raw;
            return 1;
        }
    }
    return 0;
}

int cli_pe_imports(const unsigned char *bytes, unsigned long long size, unsigned int *machine,
                   char (*names)[CLI_NAME_LONGEST], int count, unsigned int room, CliLine *said)
{
    if (size < 0x40ull)
    {
        cli_line_text(said, "the file ends inside its headers");
        return -1;
    }
    const unsigned long long pe = cli_binary_number(bytes, size, 0x3cu, 4u);
    if (((pe + 4ull) > size) || (memor.cmp(&(MemoriaCfg){.src = bytes + pe, .other = "PE\0\0", .bytes = 4u}) != 0))
    {
        cli_line_text(said, "the file is no Windows program");
        return -1;
    }
    // a machine and a count of sections are 16 bits
    *machine = (unsigned int)cli_binary_number(bytes, size, pe + 4u, 2u);
    const unsigned int sections = (unsigned int)cli_binary_number(bytes, size, pe + 6u, 2u);
    const unsigned long long optional_size = cli_binary_number(bytes, size, pe + 20u, 2u);
    const unsigned long long optional = pe + 24ull;
    const unsigned long long magic = cli_binary_number(bytes, size, optional, 2u);
    if ((magic != 0x20bu) && (magic != 0x10bu))
    {
        cli_line_text(said, "the file's optional header is of no kind orior_cli reads");
        return -1;
    }
    const unsigned long long directories = optional + ((magic == 0x20bu) ? 112ull : 96ull);
    const unsigned long long table = optional + optional_size;
    // the import directory's entries are 20 bytes with the name at 12; the delay-load directory's 32 with the name at 4
    static const unsigned int s_reads[2][3] = {{1u, 20u, 12u}, {13u, 32u, 4u}};
    for (unsigned int each = 0u; each < 2u; each += 1u)
    {
        const unsigned long long rva = cli_binary_number(bytes, size, directories + (s_reads[each][0] * 8ull), 4u);
        unsigned long long at = 0ull;
        if ((rva == 0ull) || !cli_offset(bytes, size, table, sections, rva, &at))
        {
            continue;
        }
        for (;;)
        {
            const unsigned long long step = s_reads[each][1];
            int zeros = (at + step) <= size;
            for (unsigned long long byte = 0ull; zeros && (byte < step); byte += 1ull)
            {
                zeros = bytes[at + byte] == 0u;
            }
            const unsigned long long name_rva = cli_binary_number(bytes, size, at + s_reads[each][2], 4u);
            if (((at + step) > size) || zeros || (name_rva == 0ull))
            {
                break;
            }
            unsigned long long name_at = 0ull;
            char name[CLI_NAME_LONGEST];
            if (cli_offset(bytes, size, table, sections, name_rva, &name_at) &&
                cli_binary_text(bytes, size, name_at, name, sizeof(name)))
            {
                char lower[CLI_NAME_LONGEST];
                cli_lower(name, lower);
                int known = 0;
                for (int held = 0; !known && (held < count); held += 1)
                {
                    char other[CLI_NAME_LONGEST];
                    cli_lower(names[held], other);
                    known = cellul.eq(&(CatenaFinitaCfg){.src = other, .other = lower, .cap = CLI_CELLS_POOL});
                }
                if (!known && ((unsigned int)count < room))
                {
                    memor.cpy(&(MemoriaCfg){
                        .dst = names[count],
                        .src = name,
                        .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
                    count += 1;
                }
            }
            at += step;
        }
    }
    return count;
}

// The folders Windows itself loads a program's DLLs from: System32, or SysWOW64 for a 32-bit program on a 64-bit
// system, the old 16-bit System folder, and the Windows folder
static unsigned int cli_windows_folders(unsigned int machine, char (*folders)[CLI_PATH_LONGEST])
{
    char windows[CLI_PATH_LONGEST];
    if (!query_environment_read("SystemRoot", windows, sizeof(windows)))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = windows,
                          .src = "C:\\Windows",
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = "C:\\Windows", .cap = CLI_CELLS_POOL}) + 1u});
    }
    char wow[CLI_PATH_LONGEST];
    CliLine line = {wow, sizeof(wow), 0ull};
    cli_line_text(&line, windows);
    cli_line_text(&line, "\\SysWOW64");
    cli_line_closed(&line);
    CliLine system = {folders[0], CLI_PATH_LONGEST, 0ull};
    cli_line_text(&system, windows);
    cli_line_text(&system, ((machine == 0x14cu) && query_is_folder(wow)) ? "\\SysWOW64" : "\\System32");
    cli_line_closed(&system);
    CliLine old = {folders[1], CLI_PATH_LONGEST, 0ull};
    cli_line_text(&old, windows);
    cli_line_text(&old, "\\System");
    cli_line_closed(&old);
    memor.cpy(&(MemoriaCfg){.dst = folders[2],
                            .src = windows,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = windows, .cap = CLI_CELLS_POOL}) + 1u});
    return 3u;
}

// the library `name` in one of the folders, its path into `at`: 1, or 0
static int cli_in_folders(const char (*folders)[CLI_PATH_LONGEST], unsigned int count, const char *name, char *at)
{
    for (unsigned int each = 0u; each < count; each += 1u)
    {
        CliLine line = {at, CLI_PATH_LONGEST, 0ull};
        cli_line_text(&line, folders[each]);
        cli_line_character(&line, '/');
        cli_line_text(&line, name);
        if ((cli_line_closed(&line) != 0ull) && query_is_file(at))
        {
            return 1;
        }
    }
    return 0;
}

// A Windows program's libraries read into the report, each looked for and those found beside it or on a PATH read for
// their own in turn
static int cli_pe_read(const unsigned char *bytes, unsigned long long size, const char *path,
                       const char (*orior_path)[CLI_PATH_LONGEST], unsigned int orior_count, CliBinaryReport *report,
                       CliLine *said)
{
    static char s_imports[CLI_BINARY_LIBRARIES][CLI_NAME_LONGEST];
    char folder[CLI_PATH_LONGEST];
    char windows[3][CLI_PATH_LONGEST];
    memor.cpy(&(MemoriaCfg){
        .dst = folder, .src = path, .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    char *cut = folder + cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL});
    while ((cut > folder) && (cut[-1] != '/') && (cut[-1] != '\\'))
    {
        cut -= 1;
    }
    const char *const program = path + (cut - folder);
    if (cut > folder)
    {
        cut[-1] = '\0';
    }
    else
    {
        memor.cpy(&(MemoriaCfg){.dst = folder,
                                .src = ".",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = ".", .cap = CLI_CELLS_POOL}) + 1u});
    }
    const unsigned int system_count = cli_toolchains_system_path(s_system_path, CLI_PATH_FOLDERS);
    unsigned int machine = 0u;
    const int imports = cli_pe_imports(bytes, size, &machine, s_imports, 0, CLI_BINARY_LIBRARIES, said);
    if (imports < 0)
    {
        return 0;
    }
    const unsigned int system = cli_windows_folders(machine, windows);
    unsigned int queued = 0u;
    for (int at = 0; (at < imports) && (queued < CLI_BINARY_LIBRARIES); at += 1)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = s_queue[queued].name,
                          .src = s_imports[at],
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_imports[at], .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(&(MemoriaCfg){.dst = s_queue[queued].by,
                                .src = program,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = program, .cap = CLI_CELLS_POOL}) + 1u});
        queued += 1u;
    }
    unsigned int seen = 0u;
    for (unsigned int next = 0u; next < queued; next += 1u)
    {
        const CliQueued *const one = &s_queue[next];
        char lower[CLI_NAME_LONGEST];
        cli_lower(one->name, lower);
        int known = (cellul.starts(&(CatenaFinitaCfg){.src = lower, .other = "api-ms-", .cap = CLI_CELLS_POOL})) ||
                    (cellul.starts(&(CatenaFinitaCfg){.src = lower, .other = "ext-ms-", .cap = CLI_CELLS_POOL}));
        for (unsigned int at = 0u; !known && (at < seen); at += 1u)
        {
            known = cellul.eq(&(CatenaFinitaCfg){.src = s_seen[at], .other = lower, .cap = CLI_CELLS_POOL});
        }
        if (known || (seen >= CLI_BINARY_LIBRARIES) || (report->libraries >= CLI_BINARY_LIBRARIES))
        {
            continue;
        }
        memor.cpy(&(MemoriaCfg){.dst = s_seen[seen],
                                .src = lower,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = lower, .cap = CLI_CELLS_POOL}) + 1u});
        seen += 1u;
        CliLibrary *const library = &report->library[report->libraries];
        report->libraries += 1u;
        memor.cpy(&(MemoriaCfg){.dst = library->name,
                                .src = one->name,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = one->name, .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(&(MemoriaCfg){.dst = library->by,
                                .src = one->by,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = one->by, .cap = CLI_CELLS_POOL}) + 1u});
        char beside[1][CLI_PATH_LONGEST];
        memor.cpy(&(MemoriaCfg){.dst = beside[0],
                                .src = folder,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1u});
        library->has_at = 1;
        if (cli_in_folders((const char (*)[CLI_PATH_LONGEST])beside, 1u, one->name, library->at))
        {
            library->found = CLI_FOUND_BESIDE;
        }
        else if (cli_in_folders((const char (*)[CLI_PATH_LONGEST])windows, system, one->name, library->at))
        {
            library->found = cli_listed(lower, s_debug_runtime, 7u)     ? CLI_FOUND_DEBUG_RUNTIME
                             : cli_listed(lower, s_redistributable, 9u) ? CLI_FOUND_REDISTRIBUTABLE
                                                                        : CLI_FOUND_SYSTEM;
        }
        else if (cli_in_folders((const char (*)[CLI_PATH_LONGEST])s_system_path, system_count, one->name, library->at))
        {
            library->found = CLI_FOUND_SYSTEM_PATH;
        }
        else if (cli_in_folders(orior_path, orior_count, one->name, library->at))
        {
            library->found = CLI_FOUND_ORIOR_PATH;
        }
        else
        {
            library->found = cli_listed(lower, s_debug_runtime, 7u) ? CLI_FOUND_DEBUG_RUNTIME : CLI_FOUND_MISSING;
            library->has_at = 0;
        }
        const int looked =
            library->has_at && ((library->found == CLI_FOUND_BESIDE) || (library->found == CLI_FOUND_SYSTEM_PATH) ||
                                (library->found == CLI_FOUND_ORIOR_PATH));
        unsigned long long more_size = 0ull;
        const unsigned char *const more = looked ? cli_file_held(library->at, &more_size) : NULL;
        if (more != NULL)
        {
            char unsaid[CLI_PATH_LONGEST];
            CliLine ignored = {unsaid, sizeof(unsaid), 0ull};
            unsigned int more_machine = 0u;
            const int found =
                cli_pe_imports(more, more_size, &more_machine, s_imports, 0, CLI_BINARY_LIBRARIES, &ignored);
            for (int at = 0; (at < found) && (queued < CLI_BINARY_LIBRARIES); at += 1)
            {
                memor.cpy(&(MemoriaCfg){
                    .dst = s_queue[queued].name,
                    .src = s_imports[at],
                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_imports[at], .cap = CLI_CELLS_POOL}) + 1u});
                memor.cpy(&(MemoriaCfg){
                    .dst = s_queue[queued].by,
                    .src = library->name,
                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = library->name, .cap = CLI_CELLS_POOL}) + 1u});
                queued += 1u;
            }
            cli_file_let_go(more, more_size);
        }
    }
    return 1;
}

int cli_binary_read(const char *path, const char (*orior_path)[CLI_PATH_LONGEST], unsigned int count,
                    CliBinaryReport *report, CliLine *said)
{
    report->libraries = 0u;
    report->build_paths = 0u;
    report->has_glibc = 0;
    report->glibc[0] = '\0';
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_held(path, &size);
    if (bytes == NULL)
    {
        cli_line_text(said, path);
        cli_line_text(said, ": it could not be read");
        return 0;
    }
    cli_lock_take(&s_lock);
    int read = 0;
    if ((size >= 2u) && (bytes[0] == 'M') && (bytes[1] == 'Z'))
    {
        read = cli_pe_read(bytes, size, path, orior_path, count, report, said);
    }
    else if ((size >= 4u) && (memor.cmp(&(MemoriaCfg){.src = bytes,
                                                      .other = "\x7f"
                                                               "ELF",
                                                      .bytes = 4u}) == 0))
    {
        read = cli_binary_elf_read(bytes, size, path, report, said);
    }
    else if ((size >= 4u) && ((memor.cmp(&(MemoriaCfg){.src = bytes, .other = "\xcf\xfa\xed\xfe", .bytes = 4u}) == 0) ||
                              (memor.cmp(&(MemoriaCfg){.src = bytes, .other = "\xca\xfe\xba\xbe", .bytes = 4u}) == 0)))
    {
        cli_line_text(said, "orior_cli reads the libraries of Windows and Linux programs; a macOS program's are read with "
                            "otool -L");
    }
    else
    {
        cli_line_text(said, path);
        cli_line_text(said, " is no program orior_cli reads");
    }
    cli_lock_give(&s_lock);
    cli_file_let_go(bytes, size);
    return read;
}
