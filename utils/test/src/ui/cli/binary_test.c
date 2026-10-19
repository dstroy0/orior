// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// binary_test.c: the program reader's check: a Windows program gives its imports and delayed imports, a library only
// orior finds is told and copied beside, and a Linux program gives its needs, run paths and glibc
#include "../../../../../ui/cli/src/programs/binary.h"
#include "cli_check.h"

// the bytes of a program made here
#define BINARY_ROOM 0x1400u

static unsigned char s_bytes[BINARY_ROOM];

// `value` written little-endian, `width` bytes of it, at `at`
static void binary_put(unsigned char *bytes, unsigned int at, unsigned long long value, unsigned int width)
{
    for (unsigned int each = 0u; each < width; each += 1u)
    {
        // a byte of the number, its low eight bits kept
        bytes[at + each] = (unsigned char)((value >> (8u * each)) & 0xffu);
    }
}

// a Windows program of one section that imports the `count` names, the last of them delay-loaded, into s_bytes
static void binary_pe_with(const char *const *names, unsigned int count)
{
    memor.set(&(MemoriaCfg){.dst = s_bytes, .val = 0, .bytes = sizeof(s_bytes)});
    memor.cpy(&(MemoriaCfg){.dst = s_bytes, .src = "MZ", .bytes = 2u});
    binary_put(s_bytes, 0x3Cu, 0x80u, 4u);
    memor.cpy(&(MemoriaCfg){.dst = s_bytes + 0x80u, .src = "PE\0\0", .bytes = 4u});
    binary_put(s_bytes, 0x84u, 0x8664u, 2u);
    binary_put(s_bytes, 0x86u, 1u, 2u);
    binary_put(s_bytes, 0x94u, 240u, 2u);
    binary_put(s_bytes, 0x98u, 0x20bu, 2u);
    const unsigned int section = 0x98u + 240u;
    binary_put(s_bytes, section + 8u, 0x1000u, 4u);
    binary_put(s_bytes, section + 12u, 0x1000u, 4u);
    binary_put(s_bytes, section + 16u, 0x1000u, 4u);
    binary_put(s_bytes, section + 20u, 0x400u, 4u);
    const unsigned int directories = 0x98u + 112u;
    binary_put(s_bytes, directories + 8u, 0x1000u, 4u);
    binary_put(s_bytes, directories + 13u * 8u, 0x1200u, 4u);
    unsigned int name_rva = 0x1300u;
    for (unsigned int index = 0u; index + 1u < count; index += 1u)
    {
        binary_put(s_bytes, 0x400u + index * 20u + 12u, name_rva, 4u);
        memor.cpy(&(MemoriaCfg){.dst = s_bytes + (name_rva - 0x1000u + 0x400u),
                                .src = names[index],
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = names[index], .cap = CLI_CELLS_POOL})});
        // a name of a dozen letters is far below what an unsigned int counts
        name_rva += (unsigned int)cellul.len(&(CatenaFinitaCfg){.src = names[index], .cap = CLI_CELLS_POOL}) + 1u;
    }
    binary_put(s_bytes, 0x600u, 1u, 4u);
    binary_put(s_bytes, 0x604u, name_rva, 4u);
    memor.cpy(&(MemoriaCfg){.dst = s_bytes + (name_rva - 0x1000u + 0x400u),
                            .src = names[count - 1u],
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = names[count - 1u], .cap = CLI_CELLS_POOL})});
}

// a Windows program gives its imports and delayed imports
static void binary_windows_imports(void)
{
    static const char *const s_names[3] = {"KERNEL32.dll", "libstdc++-6.dll", "late.dll"};
    static char s_found[8][CLI_NAME_LONGEST];
    binary_pe_with(s_names, 3u);
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    unsigned int machine = 0u;
    const int count = cli_pe_imports(s_bytes, sizeof(s_bytes), &machine, s_found, 0, 8u, &said);
    cli_check(
        (machine == 0x8664u) && (count == 3) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_found[0], .other = "KERNEL32.dll", .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_found[1], .other = "libstdc++-6.dll", .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_found[2], .other = "late.dll", .cap = CLI_CELLS_POOL})),
        "a Windows program gives its machine, its imports and its delayed imports");
}

#if defined(_WIN32)
// the lines a report says, each after the last
static char s_lines[1u << 14u];
static CliLine s_said_lines = {s_lines, sizeof(s_lines), 0ull};

static void binary_line(void *context, const char *text, unsigned long long length)
{
    (void)context;
    cli_line_counted(&s_said_lines, text, length);
    cli_line_character(&s_said_lines, '\n');
}

// the library `name` of the report, NULL where it has none
static const CliLibrary *binary_by_name(const CliBinaryReport *report, const char *name)
{
    for (unsigned int at = 0u; at < report->libraries; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = report->library[at].name, .other = name, .cap = CLI_CELLS_POOL}))
        {
            return &report->library[at];
        }
    }
    return NULL;
}

// whether the library `name` of the report is found as `found`, loaded by `by`
static int binary_found(const CliBinaryReport *report, const char *name, unsigned int found, const char *by)
{
    const CliLibrary *const library = binary_by_name(report, name);
    return (library != NULL) && (library->found == found) &&
           ((by == NULL) || (cellul.eq(&(CatenaFinitaCfg){.src = library->by, .other = by, .cap = CLI_CELLS_POOL})));
}

// the names copied, each after the last
static char s_copied[256];

static void binary_copied(void *context, const char *name)
{
    CliLine *const copied = context;
    cli_line_text(copied, name);
    cli_line_character(copied, '\n');
}
#endif

// a library only orior finds is told and copied beside
static void binary_orior_path(void)
{
    static const char *const s_main[3] = {"kernel32.dll", "helper.dll", "nowhere.dll"};
    static const char *const s_helper[2] = {"kernel32.dll", "deeper.dll"};
    static char s_dir[CLI_PATH_LONGEST];
    static char s_tools[1][CLI_PATH_LONGEST];
    char app[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    cli_check_folder("binary", s_dir);
    cli_full(s_dir, "app", app);
    cli_full(s_dir, "tools", s_tools[0]);
    (void)query_folder_made(app);
    (void)query_folder_made(s_tools[0]);
    binary_pe_with(s_helper, 2u);
    cli_full(s_tools[0], "helper.dll", path);
    (void)query_file_written(path, s_bytes, sizeof(s_bytes));
    binary_pe_with(s_main, 3u);
    cli_full(app, "main.exe", path);
    (void)query_file_written(path, s_bytes, sizeof(s_bytes));
#if defined(_WIN32)
    static CliBinaryReport s_report;
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check(cli_binary_read(path, (const char (*)[CLI_PATH_LONGEST])s_tools, 1u, &s_report, &said),
              "the program reads");
    cli_check(binary_found(&s_report, "kernel32.dll", CLI_FOUND_SYSTEM, "main.exe"),
              "a system library is the system's");
    cli_check(binary_found(&s_report, "helper.dll", CLI_FOUND_ORIOR_PATH, "main.exe"),
              "a library in orior's folders is found through them");
    cli_check(binary_found(&s_report, "deeper.dll", CLI_FOUND_MISSING, "helper.dll"),
              "a library that library loads, found nowhere, is missing");
    cli_check(binary_found(&s_report, "nowhere.dll", CLI_FOUND_MISSING, NULL),
              "a delayed import found nowhere is missing");
    const QuerySink sink = {binary_line, NULL};
    cli_binary_report_said(&s_report, sink);
    (void)cli_line_closed(&s_said_lines);
    cli_check(cellul.find(&(CatenaFinitaCfg){
                  .src = s_lines,
                  .cap = CLI_CELLS_POOL,
                  .other = "helper.dll, which main.exe loads, is found through the folders orior puts on its PATH",
                  .other_cap = CLI_CELLS_POOL}) != NULL,
              "the library only orior finds is told");
    CliLine copied = {s_copied, sizeof(s_copied), 0ull};
    cli_check(cli_binary_copy_beside(path, &s_report, binary_copied, &copied, &said), "the library is copied");
    cli_check_text(s_copied, copied.at, "helper.dll\n", "only the library orior finds is copied");
    cli_full(app, "helper.dll", path);
    cli_check(query_is_file(path), "it stands beside the program");
#else
    cli_check(1, "the loader's search is Windows's, and is not made here");
#endif
}

// a Linux program gives its needs, run paths and glibc
static void binary_linux_needs(void)
{
    static const char s_strings[] =
        "\0libc.so.6\0libfoo.so\0$ORIGIN/../lib:/home/me/build/lib\0GLIBC_2.17\0GLIBC_2.34\0";
    static const unsigned long long s_entries[6][2] = {
        {1u, 1u}, {1u, 11u}, {29u, 21u}, {5u, 0x300u}, {0x6ffffffeu, 0x380u}, {0x6fffffffu, 1u}};
    static CliDynamic s_dynamic;
    memor.set(&(MemoriaCfg){.dst = s_bytes, .val = 0, .bytes = 0x1000u});
    memor.cpy(&(MemoriaCfg){.dst = s_bytes,
                            .src = "\x7f"
                                   "ELF\x02\x01",
                            .bytes = 6u});
    binary_put(s_bytes, 0x20u, 0x40u, 8u);
    binary_put(s_bytes, 0x38u, 2u, 2u);
    const unsigned int heads[2][4] = {{0x40u, 1u, 0u, 0x1000u}, {0x78u, 2u, 0x200u, 0x80u}};
    for (unsigned int head = 0u; head < 2u; head += 1u)
    {
        binary_put(s_bytes, heads[head][0], heads[head][1], 4u);
        binary_put(s_bytes, heads[head][0] + 8u, heads[head][2], 8u);
        binary_put(s_bytes, heads[head][0] + 16u, heads[head][2], 8u);
        binary_put(s_bytes, heads[head][0] + 32u, heads[head][3], 8u);
    }
    memor.cpy(&(MemoriaCfg){.dst = s_bytes + 0x300u, .src = s_strings, .bytes = sizeof(s_strings) - 1u});
    for (unsigned int index = 0u; index < 6u; index += 1u)
    {
        binary_put(s_bytes, 0x200u + index * 16u, s_entries[index][0], 8u);
        binary_put(s_bytes, 0x208u + index * 16u, s_entries[index][1], 8u);
    }
    binary_put(s_bytes, 0x380u, 1u, 2u);
    binary_put(s_bytes, 0x382u, 2u, 2u);
    binary_put(s_bytes, 0x384u, 1u, 4u);
    binary_put(s_bytes, 0x388u, 16u, 4u);
    binary_put(s_bytes, 0x390u + 8u, 55u, 4u);
    binary_put(s_bytes, 0x390u + 12u, 16u, 4u);
    binary_put(s_bytes, 0x390u + 16u + 8u, 66u, 4u);
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check(cli_elf_dynamic(s_bytes, 0x1000u, &s_dynamic, &said), "the dynamic section reads");
    cli_check(
        (s_dynamic.count == 2u) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_dynamic.needed[0], .other = "libc.so.6", .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_dynamic.needed[1], .other = "libfoo.so", .cap = CLI_CELLS_POOL})),
        "a Linux program gives the libraries it needs");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = s_dynamic.runpath, .other = "$ORIGIN/../lib:/home/me/build/lib", .cap = CLI_CELLS_POOL}),
              "and its run paths");
    cli_check(s_dynamic.has_glibc &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_dynamic.glibc, .other = "GLIBC_2.34", .cap = CLI_CELLS_POOL})),
              "and the newest glibc it asks for");
}

int main(void)
{
    binary_windows_imports();
    binary_orior_path();
    binary_linux_needs();
    return cli_checked();
}
