// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// binary.c: a Windows program's imports and delay-loaded imports, each looked for as Windows' loader looks for it, the
// libraries those load in turn; what a report reports; and the libraries found through a PATH copied beside the program
#include "binary.h"
#include "../command_line/cells.h"
#include "../text/text.h"
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
    char name[CLI_NAME_MAX_LEN];
    char by[CLI_NAME_MAX_LEN];
} CliQueued;

static CliQueued s_queue[CLI_BINARY_LIBRARIES];
static char s_seen[CLI_BINARY_LIBRARIES][CLI_NAME_MAX_LEN];
static char s_system_path[CLI_PATH_FOLDERS][CLI_PATH_MAX_LEN];

unsigned long long cli_binary_number(const CliBinaryBytesSizeCfg *args)
{
    if ((args->at + args->width) > args->size)
    {
        return 0ULL;
    }
    unsigned long long value = 0ULL;
    for (unsigned int byte = args->width; byte > 0u; byte -= 1u)
    {
        value = (value << 8u) | args->bytes[args->at + byte - 1u];
    }
    return value;
}

int cli_binary_text(const CliBinaryBytesCfg *args)
{
    const unsigned char *bytes = args->bytes;
    const unsigned char *const zero =
        (args->at < args->size)
            ? memor.chr(&(MemoriaCfg){.src = bytes + args->at, .val = 0, .bytes = args->size - args->at})
            : NULL;
    const unsigned long long end = (zero != NULL) ? (unsigned long long)(zero - bytes) : args->size;
    if (end >= args->size)
    {
        return 0;
    }
    VerbaLinea line = {args->out, args->capacity, 0ULL};
    // the bytes of a name read as the text they write
    mmgr_line_put_n(&line, (const char *)(bytes + args->at), end - args->at);
    return mmgr_line_finish(&line) != 0ULL;
}

typedef struct
{
    const char *name;
    char *out;
    const char *lower;
    const char *const *names;
    unsigned int count;
    const unsigned char *bytes;
    unsigned long long size;
    unsigned long long table;
    unsigned int sections;
    unsigned long long rva;
    unsigned long long *offset;
    unsigned int machine;
    char (*folders)[CLI_PATH_MAX_LEN];
    const char (*folders_callback)[CLI_PATH_MAX_LEN];
    char *at;
    const char *path;
    const char (*orior_path)[CLI_PATH_MAX_LEN];
    unsigned int orior_count;
    CliBinaryReport *report;
    VerbaLinea *err_msg;
} BinaryCtx;
// a name in lower case into `out`, sized for CLI_NAME_MAX_LEN
EMBED_INLINE void cli_lower(const BinaryCtx *args)
{
    char *out = args->out;
    (void)EMBED_CALL(cli_text_out_capacity.lowered_into, CliTextOutCapacityCfg, .out_capacity = out,
                     .capacity = CLI_NAME_MAX_LEN, .text = args->name);
}

EMBED_INLINE int cli_on_list(const BinaryCtx *args)
{
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->lower, .other = args->names[at], .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return 0;
}

// The place in the file of a virtual address of the program, by its sections: 1, or 0 where none contains it
EMBED_INLINE int cli_offset(const BinaryCtx *args)
{
    const unsigned char *bytes = args->bytes;
    unsigned long long size = args->size;
    unsigned long long *offset = args->offset;
    for (unsigned int index = 0u; index < args->sections; index += 1u)
    {
        const unsigned long long at = args->table + (index * 40ULL);
        const unsigned long long virtual_size = EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg,
                                                           .bytes = bytes, .size = size, .at = at + 8u, .width = 4u);
        const unsigned long long raw_size = EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg,
                                                       .bytes = bytes, .size = size, .at = at + 16u, .width = 4u);
        const unsigned long long length = (virtual_size > raw_size) ? virtual_size : raw_size;
        const unsigned long long start = EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg, .bytes = bytes,
                                                    .size = size, .at = at + 12u, .width = 4u);
        const unsigned long long raw = EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg, .bytes = bytes,
                                                  .size = size, .at = at + 20u, .width = 4u);
        if ((at + 24ULL) <= size && (args->rva >= start) && (args->rva < (start + length)))
        {
            *offset = args->rva - start + raw;
            return 1;
        }
    }
    return 0;
}

int cli_pe_imports(const CliBinaryErrMsgCfg *args)
{
    const unsigned char *bytes = args->bytes;
    unsigned long long size = args->size;
    unsigned int *machine = args->machine;
    int count = args->count_i32;
    unsigned int capacity = args->capacity_u32;
    if (size < 0x40ULL)
    {
        mmgr_line_put(args->err_msg, "the file ends inside its headers");
        return -1;
    }
    const unsigned long long pe = EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg, .bytes = bytes,
                                             .size = size, .at = 0x3cu, .width = 4u);
    if (((pe + 4ULL) > size) || (memor.cmp(&(MemoriaCfg){.src = bytes + pe, .other = "PE\0\0", .bytes = 4u}) != 0))
    {
        mmgr_line_put(args->err_msg, "the file is no Windows program");
        return -1;
    }
    // a machine and a count of sections are 16 bits
    *machine = (unsigned int)EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg, .bytes = bytes,
                                        .size = size, .at = pe + 4u, .width = 2u);
    const unsigned int sections = (unsigned int)EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg,
                                                           .bytes = bytes, .size = size, .at = pe + 6u, .width = 2u);
    const unsigned long long optional_size = EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg,
                                                        .bytes = bytes, .size = size, .at = pe + 20u, .width = 2u);
    const unsigned long long optional = pe + 24ULL;
    const unsigned long long magic = EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg, .bytes = bytes,
                                                .size = size, .at = optional, .width = 2u);
    if ((magic != 0x20bu) && (magic != 0x10bu))
    {
        mmgr_line_put(args->err_msg, "the file's optional header is of no kind orior_cli reads");
        return -1;
    }
    const unsigned long long directories = optional + ((magic == 0x20bu) ? 112ULL : 96ULL);
    const unsigned long long table = optional + optional_size;
    // the import directory's entries are 20 bytes with the name at 12; the delay-load directory's 32 with the name at 4
    static const unsigned int s_reads[2][3] = {{1u, 20u, 12u}, {13u, 32u, 4u}};
    for (unsigned int each = 0u; each < 2u; each += 1u)
    {
        const unsigned long long rva =
            EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg, .bytes = bytes, .size = size,
                       .at = directories + (s_reads[each][0] * 8ULL), .width = 4u);
        unsigned long long at = 0ULL;
        if ((rva == 0ULL) || !EMBED_CALL(cli_offset, BinaryCtx, .bytes = bytes, .size = size, .table = table,
                                         .sections = sections, .rva = rva, .offset = &at))
        {
            continue;
        }
        for (;;)
        {
            const unsigned long long step = s_reads[each][1];
            int zeros = (at + step) <= size;
            for (unsigned long long byte = 0ULL; zeros && (byte < step); byte += 1ULL)
            {
                zeros = bytes[at + byte] == 0u;
            }
            const unsigned long long name_rva =
                EMBED_CALL(cli_binary_bytes_size.number, CliBinaryBytesSizeCfg, .bytes = bytes, .size = size,
                           .at = at + s_reads[each][2], .width = 4u);
            if (((at + step) > size) || zeros || (name_rva == 0ULL))
            {
                break;
            }
            unsigned long long name_at = 0ULL;
            char name[CLI_NAME_MAX_LEN];
            if (EMBED_CALL(cli_offset, BinaryCtx, .bytes = bytes, .size = size, .table = table, .sections = sections,
                           .rva = name_rva, .offset = &name_at) &&
                EMBED_CALL(cli_binary_bytes.text, CliBinaryBytesCfg, .bytes = bytes, .size = size, .at = name_at,
                           .out = name, .capacity = sizeof(name)))
            {
                char lower[CLI_NAME_MAX_LEN];
                EMBED_CALL(cli_lower, BinaryCtx, .name = name, .out = lower);
                int known = 0;
                for (int index = 0; !known && (index < count); index += 1)
                {
                    char other[CLI_NAME_MAX_LEN];
                    EMBED_CALL(cli_lower, BinaryCtx, .name = args->names[index], .out = other);
                    known = cellul.eq(&(CatenaFinitaCfg){.src = other, .other = lower, .cap = CLI_CELLS_POOL});
                }
                if (!known && ((unsigned int)count < capacity))
                {
                    memor.cpy(&(MemoriaCfg){
                        .dst = args->names[count],
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
EMBED_INLINE unsigned int cli_windows_folders(const BinaryCtx *args)
{
    char windows[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "SystemRoot",
                    .value_out = windows, .capacity = sizeof(windows)))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = windows,
                          .src = "C:\\Windows",
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = "C:\\Windows", .cap = CLI_CELLS_POOL}) + 1u});
    }
    char wow[CLI_PATH_MAX_LEN];
    VerbaLinea line = {wow, sizeof(wow), 0ULL};
    mmgr_line_put(&line, windows);
    mmgr_line_put(&line, "\\SysWOW64");
    mmgr_line_finish(&line);
    VerbaLinea system = {args->folders[0], CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&system, windows);
    mmgr_line_put(&system, ((args->machine == 0x14cu) &&
                            EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = wow))
                               ? "\\SysWOW64"
                               : "\\System32");
    mmgr_line_finish(&system);
    VerbaLinea old = {args->folders[1], CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&old, windows);
    mmgr_line_put(&old, "\\System");
    mmgr_line_finish(&old);
    memor.cpy(&(MemoriaCfg){.dst = args->folders[2],
                            .src = windows,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = windows, .cap = CLI_CELLS_POOL}) + 1u});
    return 3u;
}

// the library `name` in one of the folders, its path into `at`: 1, or 0
EMBED_INLINE int cli_in_folders(const BinaryCtx *args)
{
    for (unsigned int each = 0u; each < args->count; each += 1u)
    {
        VerbaLinea line = {args->at, CLI_PATH_MAX_LEN, 0ULL};
        mmgr_line_put(&line, args->folders_callback[each]);
        mmgr_line_ch(&line, '/');
        mmgr_line_put(&line, args->name);
        if ((mmgr_line_finish(&line) != 0ULL) &&
            EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = args->at))
        {
            return 1;
        }
    }
    return 0;
}

// A Windows program's libraries read into the report, each looked for and those found beside it or on a PATH read for
// their own in turn
EMBED_INLINE int cli_pe_read(const BinaryCtx *args)
{
    const unsigned char *bytes = args->bytes;
    unsigned long long size = args->size;
    const char *path = args->path;
    static char s_imports[CLI_BINARY_LIBRARIES][CLI_NAME_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    char windows[3][CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){
        .dst = folder, .src = path, .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    char *cut = folder + cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL});
    cut = folder + EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = folder,
                              .at = (unsigned long long)(cut - folder), .set = "/\\");
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
    const unsigned int system_count = EMBED_CALL(cli_toolchains_count.system_path, CliToolchainsCountCfg,
                                                 .folders = s_system_path, .capacity = CLI_PATH_FOLDERS);
    unsigned int machine = 0u;
    const int imports =
        EMBED_CALL(cli_binary_err_msg.pe_imports, CliBinaryErrMsgCfg, .bytes = bytes, .size = size, .machine = &machine,
                   .names = s_imports, .count_i32 = 0, .capacity_u32 = CLI_BINARY_LIBRARIES, .err_msg = args->err_msg);
    if (imports < 0)
    {
        return 0;
    }
    const unsigned int system = EMBED_CALL(cli_windows_folders, BinaryCtx, .machine = machine, .folders = windows);
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
        char lower[CLI_NAME_MAX_LEN];
        EMBED_CALL(cli_lower, BinaryCtx, .name = one->name, .out = lower);
        int known = (cellul.starts(&(CatenaFinitaCfg){.src = lower, .other = "api-ms-", .cap = CLI_CELLS_POOL})) ||
                    (cellul.starts(&(CatenaFinitaCfg){.src = lower, .other = "ext-ms-", .cap = CLI_CELLS_POOL}));
        for (unsigned int at = 0u; !known && (at < seen); at += 1u)
        {
            known = cellul.eq(&(CatenaFinitaCfg){.src = s_seen[at], .other = lower, .cap = CLI_CELLS_POOL});
        }
        if (known || (seen >= CLI_BINARY_LIBRARIES) || (args->report->libraries >= CLI_BINARY_LIBRARIES))
        {
            continue;
        }
        memor.cpy(&(MemoriaCfg){.dst = s_seen[seen],
                                .src = lower,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = lower, .cap = CLI_CELLS_POOL}) + 1u});
        seen += 1u;
        CliLibrary *const library = &args->report->library[args->report->libraries];
        args->report->libraries += 1u;
        memor.cpy(&(MemoriaCfg){.dst = library->name,
                                .src = one->name,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = one->name, .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(&(MemoriaCfg){.dst = library->by,
                                .src = one->by,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = one->by, .cap = CLI_CELLS_POOL}) + 1u});
        char adjacent[1][CLI_PATH_MAX_LEN];
        memor.cpy(&(MemoriaCfg){.dst = adjacent[0],
                                .src = folder,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1u});
        library->has_at = 1;
        if (EMBED_CALL(cli_in_folders, BinaryCtx, .folders_callback = (const char (*)[CLI_PATH_MAX_LEN])adjacent,
                       .count = 1u, .name = one->name, .at = library->at))
        {
            library->found = CLI_FOUND_ADJACENT;
        }
        else if (EMBED_CALL(cli_in_folders, BinaryCtx, .folders_callback = (const char (*)[CLI_PATH_MAX_LEN])windows,
                            .count = system, .name = one->name, .at = library->at))
        {
            library->found =
                EMBED_CALL(cli_on_list, BinaryCtx, .lower = lower, .names = s_debug_runtime, .count = 7u)
                    ? CLI_FOUND_DEBUG_RUNTIME
                : EMBED_CALL(cli_on_list, BinaryCtx, .lower = lower, .names = s_redistributable, .count = 9u)
                    ? CLI_FOUND_REDISTRIBUTABLE
                    : CLI_FOUND_SYSTEM;
        }
        else if (EMBED_CALL(cli_in_folders, BinaryCtx,
                            .folders_callback = (const char (*)[CLI_PATH_MAX_LEN])s_system_path, .count = system_count,
                            .name = one->name, .at = library->at))
        {
            library->found = CLI_FOUND_SYSTEM_PATH;
        }
        else if (EMBED_CALL(cli_in_folders, BinaryCtx, .folders_callback = args->orior_path, .count = args->orior_count,
                            .name = one->name, .at = library->at))
        {
            library->found = CLI_FOUND_ORIOR_PATH;
        }
        else
        {
            library->found = EMBED_CALL(cli_on_list, BinaryCtx, .lower = lower, .names = s_debug_runtime, .count = 7u)
                                 ? CLI_FOUND_DEBUG_RUNTIME
                                 : CLI_FOUND_MISSING;
            library->has_at = 0;
        }
        const int looked =
            library->has_at && ((library->found == CLI_FOUND_ADJACENT) || (library->found == CLI_FOUND_SYSTEM_PATH) ||
                                (library->found == CLI_FOUND_ORIOR_PATH));
        unsigned long long more_size = 0ULL;
        const unsigned char *const more =
            looked ? EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = library->at,
                                .size_pointer = &more_size, .owner = &g_cli_programs_cells)
                   : NULL;
        if (more != NULL)
        {
            char err_buf[CLI_PATH_MAX_LEN];
            VerbaLinea ignored = {err_buf, sizeof(err_buf), 0ULL};
            unsigned int more_machine = 0u;
            const int found = EMBED_CALL(cli_binary_err_msg.pe_imports, CliBinaryErrMsgCfg, .bytes = more,
                                         .size = more_size, .machine = &more_machine, .names = s_imports,
                                         .count_i32 = 0, .capacity_u32 = CLI_BINARY_LIBRARIES, .err_msg = &ignored);
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
            EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = more,
                       .owner = &g_cli_programs_cells);
        }
    }
    return 1;
}

int cli_binary_read(const CliBinaryPathOriorPathCfg *args)
{
    const char *path = args->path;
    const char (*orior_path)[CLI_PATH_MAX_LEN] = args->orior_path;
    CliBinaryReport *report = args->report;
    VerbaLinea *err_msg = args->err_msg;
    report->libraries = 0u;
    report->build_paths = 0u;
    report->has_glibc = 0;
    report->glibc[0] = '\0';
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                  .size_pointer = &size, .owner = &g_cli_programs_cells);
    if (bytes == NULL)
    {
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, ": it could not be read");
        return 0;
    }
    int read = 0;
    if ((size >= 2u) && (bytes[0] == 'M') && (bytes[1] == 'Z'))
    {
        read = EMBED_CALL(cli_pe_read, BinaryCtx, .bytes = bytes, .size = size, .path = path, .orior_path = orior_path,
                          .orior_count = args->count, .report = report, .err_msg = err_msg);
    }
    else if ((size >= 4u) && (memor.cmp(&(MemoriaCfg){.src = bytes,
                                                      .other = "\x7f"
                                                               "ELF",
                                                      .bytes = 4u}) == 0))
    {
        read = EMBED_CALL(cli_binary_path_report.elf_read, CliBinaryPathReportCfg, .bytes = bytes, .size = size,
                          .path = path, .report = report, .err_msg = err_msg);
    }
    else if ((size >= 4u) && ((memor.cmp(&(MemoriaCfg){.src = bytes, .other = "\xcf\xfa\xed\xfe", .bytes = 4u}) == 0) ||
                              (memor.cmp(&(MemoriaCfg){.src = bytes, .other = "\xca\xfe\xba\xbe", .bytes = 4u}) == 0)))
    {
        mmgr_line_put(err_msg,
                      "orior_cli reads the libraries of Windows and Linux programs; a macOS program's are read with "
                      "otool -L");
    }
    else
    {
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, " is no program orior_cli reads");
    }
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_programs_cells);
    return read;
}
