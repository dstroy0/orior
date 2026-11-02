// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_binary.c: the program reader: a Windows program gives its imports and delayed imports, on Windows a library only orior_cli finds is
// reported and copied beside the program, and a Linux program gives its needs, run paths and glibc
#include "ui/cli/src/programs/binary.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// the bytes of a program made here
#define BINARY_ROOM 0x1400u

static unsigned char s_bytes[BINARY_ROOM];
static char s_why[512];
static VerbaLinea s_err_msg;
static CliDynamic s_dynamic;
static CliBinaryReport s_report;

// `value` written little-endian, `width` bytes of it, at `at`
static void number_put(unsigned int at, unsigned long long value, unsigned int width)
{
    for (unsigned int each = 0u; each < width; each += 1u)
    {
        // a byte of the number, its low eight bits
        s_bytes[at + each] = (unsigned char)((value >> (8u * each)) & 0xffu);
    }
}

// a Windows program of one section that imports the `count` names, the last of them delay-loaded, into s_bytes
static void windows_program_written(const char *const *names, unsigned int count)
{
    memor.set(&(MemoriaCfg){.dst = s_bytes, .val = 0, .bytes = sizeof(s_bytes)});
    memor.cpy(&(MemoriaCfg){.dst = s_bytes, .src = "MZ", .bytes = 2u});
    number_put(0x3Cu, 0x80u, 4u);
    memor.cpy(&(MemoriaCfg){.dst = s_bytes + 0x80u, .src = "PE\0\0", .bytes = 4u});
    number_put(0x84u, 0x8664u, 2u);
    number_put(0x86u, 1u, 2u);
    number_put(0x94u, 240u, 2u);
    number_put(0x98u, 0x20bu, 2u);
    const unsigned int section = 0x98u + 240u;
    number_put(section + 8u, 0x1000u, 4u);
    number_put(section + 12u, 0x1000u, 4u);
    number_put(section + 16u, 0x1000u, 4u);
    number_put(section + 20u, 0x400u, 4u);
    const unsigned int directories = 0x98u + 112u;
    number_put(directories + 8u, 0x1000u, 4u);
    number_put(directories + 13u * 8u, 0x1200u, 4u);
    unsigned int name_rva = 0x1300u;
    for (unsigned int index = 0u; index + 1u < count; index += 1u)
    {
        number_put(0x400u + index * 20u + 12u, name_rva, 4u);
        memor.cpy(&(MemoriaCfg){.dst = s_bytes + (name_rva - 0x1000u + 0x400u), .src = names[index], .bytes = CLI_UNITY_LENGTH(names[index])});
        // a name of a dozen letters is far below what an unsigned int counts
        name_rva += (unsigned int)CLI_UNITY_LENGTH(names[index]) + 1u;
    }
    number_put(0x600u, 1u, 4u);
    number_put(0x604u, name_rva, 4u);
    memor.cpy(&(MemoriaCfg){.dst = s_bytes + (name_rva - 0x1000u + 0x400u), .src = names[count - 1u], .bytes = CLI_UNITY_LENGTH(names[count - 1u])});
}

// a Linux program of two needs, a run path and two glibc versions, into s_bytes
static void linux_program_written(void)
{
    static const char s_strings[] = "\0libc.so.6\0libfoo.so\0$ORIGIN/../lib:/home/me/build/lib\0GLIBC_2.17\0GLIBC_2.34\0";
    static const unsigned long long s_entries[6][2] = {{1u, 1u}, {1u, 11u}, {29u, 21u}, {5u, 0x300u}, {0x6ffffffeu, 0x380u}, {0x6fffffffu, 1u}};
    static const unsigned int s_heads[2][4] = {{0x40u, 1u, 0u, 0x1000u}, {0x78u, 2u, 0x200u, 0x80u}};
    memor.set(&(MemoriaCfg){.dst = s_bytes, .val = 0, .bytes = 0x1000u});
    memor.cpy(&(MemoriaCfg){.dst = s_bytes,
                            .src = "\x7f"
                                   "ELF\x02\x01",
                            .bytes = 6u});
    number_put(0x20u, 0x40u, 8u);
    number_put(0x38u, 2u, 2u);
    for (unsigned int head = 0u; head < 2u; head += 1u)
    {
        number_put(s_heads[head][0], s_heads[head][1], 4u);
        number_put(s_heads[head][0] + 8u, s_heads[head][2], 8u);
        number_put(s_heads[head][0] + 16u, s_heads[head][2], 8u);
        number_put(s_heads[head][0] + 32u, s_heads[head][3], 8u);
    }
    memor.cpy(&(MemoriaCfg){.dst = s_bytes + 0x300u, .src = s_strings, .bytes = sizeof(s_strings) - 1u});
    for (unsigned int index = 0u; index < 6u; index += 1u)
    {
        number_put(0x200u + index * 16u, s_entries[index][0], 8u);
        number_put(0x208u + index * 16u, s_entries[index][1], 8u);
    }
    number_put(0x380u, 1u, 2u);
    number_put(0x382u, 2u, 2u);
    number_put(0x384u, 1u, 4u);
    number_put(0x388u, 16u, 4u);
    number_put(0x390u + 8u, 55u, 4u);
    number_put(0x390u + 12u, 16u, 4u);
    number_put(0x390u + 16u + 8u, 66u, 4u);
}

#if defined(_WIN32)
static char s_lines[1u << 14u];
static VerbaLinea s_report_lines = {s_lines, sizeof(s_lines), 0ull};
static char s_copied[256];
static char s_program[CLI_PATH_MAX_LEN];
static char s_app[CLI_PATH_MAX_LEN];

// a line of the report appended
static void report_line_recorded(const CliEachLineSizeCfg *args)
{
    mmgr_line_put_n(&s_report_lines, args->line, args->size);
    mmgr_line_ch(&s_report_lines, '\n');
}

// a copied name appended
static void name_copied(const CliEachNameCfg *args)
{
    VerbaLinea *const copied = args->context;
    mmgr_line_put(copied, args->name);
    mmgr_line_ch(copied, '\n');
}

// the library `name` of the report is found as `found`, loaded by `by` where that is not NULL
static void assert_found(const char *name, unsigned int found, const char *by)
{
    const CliLibrary *library = NULL;
    for (unsigned int at = 0u; (library == NULL) && (at < s_report.libraries); at += 1u)
    {
        library = cellul.eq(&(CatenaFinitaCfg){.src = s_report.library[at].name, .other = name, .cap = CLI_CELLS_POOL}) ? &s_report.library[at] : NULL;
    }
    TEST_ASSERT_NOT_NULL_MESSAGE(library, name);
    TEST_ASSERT_EQUAL_UINT(found, library->found);
    if (by != NULL)
    {
        TEST_ASSERT_EQUAL_STRING(by, library->by);
    }
}

// a program importing a library in orior's folders that imports a missing one, read into s_report
static void program_read(void)
{
    static const char *const s_main[3] = {"kernel32.dll", "helper.dll", "nowhere.dll"};
    static const char *const s_helper[2] = {"kernel32.dll", "deeper.dll"};
    static char s_dir[CLI_PATH_MAX_LEN];
    static char s_tools[1][CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    cli_unity_folder("binary", s_dir);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_dir, .relative = "app", .joined = s_app);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_dir, .relative = "tools", .joined = s_tools[0]);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = s_app));
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = s_tools[0]));
    windows_program_written(s_helper, 2u);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_tools[0], .relative = "helper.dll", .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = s_bytes, .size = sizeof(s_bytes)));
    windows_program_written(s_main, 3u);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_app, .relative = "main.exe", .joined = s_program);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = s_program, .bytes = s_bytes, .size = sizeof(s_bytes)));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_binary_path_orior_path.read, CliBinaryPathOriorPathCfg, .path = s_program, .orior_path = (const char (*)[CLI_PATH_MAX_LEN])s_tools, .count = 1u, .report = &s_report, .err_msg = &s_err_msg));
}
#endif

void setUp(void)
{
    cli_unity_begun();
    s_err_msg = (VerbaLinea){s_why, sizeof(s_why), 0ull};
    memor.set(&(MemoriaCfg){.dst = &s_report, .val = 0, .bytes = sizeof(s_report)});
}

void tearDown(void)
{
}

void test_a_windows_program_gives_its_machine_imports_and_delayed_imports(void)
{
    static const char *const s_names[3] = {"KERNEL32.dll", "libstdc++-6.dll", "late.dll"};
    static char s_found[8][CLI_NAME_MAX_LEN];
    unsigned int machine = 0u;
    windows_program_written(s_names, 3u);
    TEST_ASSERT_EQUAL_INT(3, EMBED_CALL(cli_binary_err_msg.pe_imports, CliBinaryErrMsgCfg, .bytes = s_bytes, .size = sizeof(s_bytes), .machine = &machine, .names = s_found, .count_i32 = 0, .room_u32 = 8u, .err_msg = &s_err_msg));
    TEST_ASSERT_EQUAL_HEX(0x8664u, machine);
    TEST_ASSERT_EQUAL_STRING("KERNEL32.dll", s_found[0]);
    TEST_ASSERT_EQUAL_STRING("libstdc++-6.dll", s_found[1]);
    TEST_ASSERT_EQUAL_STRING("late.dll", s_found[2]);
}

void test_windows_a_system_library_is_the_systems(void)
{
#if defined(_WIN32)
    program_read();
    assert_found("kernel32.dll", CLI_FOUND_SYSTEM, "main.exe");
#else
    TEST_IGNORE_MESSAGE("the Windows loader's search");
#endif
}

void test_windows_a_library_in_oriors_folders_is_found_through_them(void)
{
#if defined(_WIN32)
    program_read();
    assert_found("helper.dll", CLI_FOUND_ORIOR_PATH, "main.exe");
#else
    TEST_IGNORE_MESSAGE("the Windows loader's search");
#endif
}

void test_windows_a_library_a_library_loads_found_nowhere_is_missing(void)
{
#if defined(_WIN32)
    program_read();
    assert_found("deeper.dll", CLI_FOUND_MISSING, "helper.dll");
#else
    TEST_IGNORE_MESSAGE("the Windows loader's search");
#endif
}

void test_windows_a_delayed_import_found_nowhere_is_missing(void)
{
#if defined(_WIN32)
    program_read();
    assert_found("nowhere.dll", CLI_FOUND_MISSING, NULL);
#else
    TEST_IGNORE_MESSAGE("the Windows loader's search");
#endif
}

void test_windows_the_library_only_orior_cli_finds_is_reported(void)
{
#if defined(_WIN32)
    program_read();
    const QuerySink sink = {report_line_recorded, NULL};
    s_report_lines.at = 0ull;
    EMBED_CALL(cli_binary_report.report_print, CliBinaryReportCfg, .report_read = &s_report, .sink = sink);
    (void)mmgr_line_finish(&s_report_lines);
    CLI_TEST_ASSERT_CONTAINS("helper.dll, which main.exe loads, is found through the folders orior_cli puts on its PATH", s_lines);
#else
    TEST_IGNORE_MESSAGE("the Windows loader's search");
#endif
}

void test_windows_only_the_library_orior_cli_finds_is_copied_beside_the_program(void)
{
#if defined(_WIN32)
    char path[CLI_PATH_MAX_LEN];
    program_read();
    VerbaLinea copied = {s_copied, sizeof(s_copied), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_binary_path.copy_beside, CliBinaryPathCfg, .path = s_program, .report_read = &s_report, .copied = name_copied, .context = &copied, .err_msg = &s_err_msg));
    CLI_TEST_ASSERT_TEXT("helper.dll\n", s_copied, copied.at);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_app, .relative = "helper.dll", .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path));
#else
    TEST_IGNORE_MESSAGE("the Windows loader's search");
#endif
}

void test_a_linux_program_gives_the_libraries_it_needs(void)
{
    linux_program_written();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_binary_err_msg_bytes.elf_dynamic, CliBinaryErrMsgBytesCfg, .bytes = s_bytes, .size = 0x1000u, .found = &s_dynamic, .err_msg = &s_err_msg));
    TEST_ASSERT_EQUAL_UINT(2u, s_dynamic.count);
    TEST_ASSERT_EQUAL_STRING("libc.so.6", s_dynamic.needed[0]);
    TEST_ASSERT_EQUAL_STRING("libfoo.so", s_dynamic.needed[1]);
}

void test_a_linux_program_gives_its_run_paths(void)
{
    linux_program_written();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_binary_err_msg_bytes.elf_dynamic, CliBinaryErrMsgBytesCfg, .bytes = s_bytes, .size = 0x1000u, .found = &s_dynamic, .err_msg = &s_err_msg));
    TEST_ASSERT_EQUAL_STRING("$ORIGIN/../lib:/home/me/build/lib", s_dynamic.runpath);
}

void test_a_linux_program_gives_the_newest_glibc_it_requires(void)
{
    linux_program_written();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_binary_err_msg_bytes.elf_dynamic, CliBinaryErrMsgBytesCfg, .bytes = s_bytes, .size = 0x1000u, .found = &s_dynamic, .err_msg = &s_err_msg));
    TEST_ASSERT_TRUE(s_dynamic.has_glibc);
    TEST_ASSERT_EQUAL_STRING("GLIBC_2.34", s_dynamic.glibc);
}

void test_a_programs_libraries_are_read_each_once_with_the_builders_run_paths(void)
{
    linux_program_written();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_binary_path_report.elf_read, CliBinaryPathReportCfg, .bytes = s_bytes, .size = 0x1000u, .path = "/nowhere/program", .report = &s_report, .err_msg = &s_err_msg));
    TEST_ASSERT_EQUAL_UINT(2u, s_report.libraries);
    TEST_ASSERT_EQUAL_STRING("libfoo.so", s_report.library[1].name);
    TEST_ASSERT_EQUAL_UINT(1u, s_report.build_paths);
    TEST_ASSERT_EQUAL_STRING("/home/me/build/lib", s_report.build_path[0]);
}
