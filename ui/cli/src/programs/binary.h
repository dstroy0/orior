// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// binary.h: what a built program loads as it starts, read from the file itself (binary.c reads a Windows program's
// imports, binary_elf.c a Linux program's needs): a Windows program's imports and its delay-loaded imports, and a Linux
// program's needed libraries, its run paths and the glibc versions it requires. Each library is looked for as the
// system's loader looks for it where the program is opened on its own, and the libraries those load in turn, past the
// system's own. A library found only through the folders orior_cli puts on its jobs' PATH, one found nowhere, one found
// by a path of the machine that built the program, and Visual Studio's debug runtime are each reported, as the things
// that start a program here and prevent it from starting elsewhere
#ifndef BINARY_H
#define BINARY_H
typedef struct CliBinaryErrMsgCfg CliBinaryErrMsgCfg;
typedef struct CliBinaryPathCfg CliBinaryPathCfg;
typedef struct CliBinaryPathReportCfg CliBinaryPathReportCfg;
typedef struct CliBinaryPathOriorPathCfg CliBinaryPathOriorPathCfg;
typedef struct CliBinaryBytesCfg CliBinaryBytesCfg;
typedef struct CliBinaryBytesSizeCfg CliBinaryBytesSizeCfg;
typedef struct CliBinaryPathOriorPathCountCfg CliBinaryPathOriorPathCountCfg;
typedef struct CliBinaryErrMsgBytesCfg CliBinaryErrMsgBytesCfg;
typedef struct CliBinaryReportCfg CliBinaryReportCfg;
typedef struct CliBinaryCfg CliBinaryCfg;

#include "../command_line/callback_cfg.h"
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

// the most libraries a report contains, and the most run paths of the machine that built the program
#define CLI_BINARY_LIBRARIES 512u

#define CLI_BINARY_BUILD_PATHS 32u

// the most folders the Linux loader looks in after a program's own run paths
#define CLI_BINARY_LINUX_FOLDERS 256u

// Where a library a program loads is found: in the program's own folder or by a run path from it; in the system's own
// folders or one Windows resolves itself; there, put by the Visual C++ Redistributable, which a machine without it
// lacks; Visual Studio's debug runtime, which only a machine with Visual Studio has; through the PATH the system gives
// a program opened on its own; only through the folders orior_cli puts on its jobs' PATH; by a run path that names a
// folder of the machine that built the program; or nowhere
enum
{
    CLI_FOUND_ADJACENT,
    CLI_FOUND_SYSTEM,
    CLI_FOUND_REDISTRIBUTABLE,
    CLI_FOUND_DEBUG_RUNTIME,
    CLI_FOUND_SYSTEM_PATH,
    CLI_FOUND_ORIOR_PATH,
    CLI_FOUND_BUILD_PATH,
    CLI_FOUND_MISSING
};

// A library: its name, where it is found and the file found where it is, and the file that loads it, the program or a
// library the program loads
typedef struct
{
    char name[CLI_NAME_MAX_LEN];
    unsigned int found;
    char at[CLI_PATH_MAX_LEN];
    int has_at;
    char by[CLI_NAME_MAX_LEN];
} CliLibrary;

// What a program loads: its libraries, the run paths of a Linux program that name folders of the machine that built
// it, and the newest glibc version a Linux program requires, as GLIBC_2.38
typedef struct
{
    CliLibrary library[CLI_BINARY_LIBRARIES];
    unsigned int libraries;
    char build_path[CLI_BINARY_BUILD_PATHS][CLI_PATH_MAX_LEN];
    unsigned int build_paths;
    char glibc[CLI_NAME_MAX_LEN];
    int has_glibc;
} CliBinaryReport;

// what the program at `path` loads read into `report`, each library looked for as the system's loader looks for it
// where the program is opened on its own, and then, where it is not found so, through `orior_path`, `count` folders
// orior's jobs have on their PATH: 1, or 0 with why given
int cli_binary_read(const CliBinaryPathOriorPathCfg *args);
// what a report reports of the program, each line handed to `sink`: each library found other than beside it or in the
// system's own folders, each run path of the machine that built it, and the newest glibc it requires
void cli_binary_report_print(const CliBinaryReportCfg *args);
// the program at `path` read and what it loads reported line by line to `sink`: 1, or 0 where it could not be read
int cli_binary_print(const CliBinaryPathOriorPathCountCfg *args);
// each library the program loads through a PATH, this machine's or orior's, copied beside it, each name handed to
// `copied`: 1, or 0 with why given
int cli_binary_copy_adjacent(const CliBinaryPathCfg *args);

// what binary.c and binary_elf.c share: a little-endian number read from the bytes at `at`, 0 past their end; the text
// ended by a 0 at `at` into `out`, sized for `capacity`: 1, or 0 where it has no end; a Linux program's libraries read
// into the report; and the folders the Linux loader looks in after a program's own run paths into `folders`, which
// contain CLI_BINARY_LINUX_FOLDERS, those /etc/ld.so.conf names and the files it includes, then the system's own
// library folders: their count
unsigned long long cli_binary_number(const CliBinaryBytesSizeCfg *args);
int cli_binary_text(const CliBinaryBytesCfg *args);
int cli_binary_elf_read(const CliBinaryPathReportCfg *args);
unsigned int cli_binary_linux_folders(const CliBinaryCfg *args);

// A Windows program's machine and the DLLs it imports, the delay-loaded ones among them, each put once, case aside,
// after the names from `count`, which are sized for `capacity`: the count after, or -1 with why given (binary.c)
int cli_pe_imports(const CliBinaryErrMsgCfg *args);

// What a dynamic section gives: its needed libraries, its RPATH and RUNPATH, and the newest glibc it requires
typedef struct
{
    char needed[64][CLI_NAME_MAX_LEN];
    unsigned int count;
    char rpath[CLI_PATH_MAX_LEN * 2u];
    char runpath[CLI_PATH_MAX_LEN * 2u];
    char glibc[CLI_NAME_MAX_LEN];
    int has_glibc;
} CliDynamic;

// The dynamic section of a 64-bit little-endian program read into `found`: 1, or 0 with why given (binary_elf.c)
int cli_elf_dynamic(const CliBinaryErrMsgBytesCfg *args);

struct CliBinaryErrMsgCfg
{
    VerbaLinea *err_msg;
    const unsigned char *bytes;
    unsigned long long size;
    unsigned int *machine;
    char (*names)[CLI_NAME_MAX_LEN];
    int count_i32;
    unsigned int capacity_u32;
};

struct CliBinaryPathCfg
{
    const char *path;
    VerbaLinea *err_msg;
    const CliBinaryReport *report_read;
    void (*copied)(const CliEachNameCfg *args);
    void *context;
};

struct CliBinaryPathReportCfg
{
    const char *path;
    CliBinaryReport *report;
    VerbaLinea *err_msg;
    const unsigned char *bytes;
    unsigned long long size;
};

struct CliBinaryPathOriorPathCfg
{
    const char *path;
    const char (*orior_path)[CLI_PATH_MAX_LEN];
    unsigned int count;
    CliBinaryReport *report;
    VerbaLinea *err_msg;
};

struct CliBinaryBytesCfg
{
    const unsigned char *bytes;
    unsigned long long size;
    unsigned long long at;
    char *out;
    unsigned long long capacity;
};

struct CliBinaryBytesSizeCfg
{
    const unsigned char *bytes;
    unsigned long long size;
    unsigned long long at;
    unsigned int width;
};

struct CliBinaryPathOriorPathCountCfg
{
    const char *path;
    const char (*orior_path)[CLI_PATH_MAX_LEN];
    unsigned int count;
    QuerySink sink;
};

struct CliBinaryErrMsgBytesCfg
{
    VerbaLinea *err_msg;
    const unsigned char *bytes;
    unsigned long long size;
    CliDynamic *found;
};

struct CliBinaryReportCfg
{
    const CliBinaryReport *report_read;
    QuerySink sink;
};

struct CliBinaryCfg
{
    char (*folders)[CLI_PATH_MAX_LEN];
};

typedef struct
{
    int (*read)(const CliBinaryPathOriorPathCfg *args);
} CliBinaryPathOriorPathNs;
EMBED_TABLE_LAYOUT(CliBinaryPathOriorPathNs, read);
EMBED_TABLE_STORAGE CliBinaryPathOriorPathNs cli_binary_path_orior_path EMBED_UNUSED = {
    .read = cli_binary_read,
};

typedef struct
{
    void (*report_print)(const CliBinaryReportCfg *args);
} CliBinaryReportNs;
EMBED_TABLE_LAYOUT(CliBinaryReportNs, report_print);
EMBED_TABLE_STORAGE CliBinaryReportNs cli_binary_report EMBED_UNUSED = {
    .report_print = cli_binary_report_print,
};

typedef struct
{
    int (*print)(const CliBinaryPathOriorPathCountCfg *args);
} CliBinaryPathOriorPathCountNs;
EMBED_TABLE_LAYOUT(CliBinaryPathOriorPathCountNs, print);
EMBED_TABLE_STORAGE CliBinaryPathOriorPathCountNs cli_binary_path_orior_path_count EMBED_UNUSED = {
    .print = cli_binary_print,
};

typedef struct
{
    int (*copy_adjacent)(const CliBinaryPathCfg *args);
} CliBinaryPathNs;
EMBED_TABLE_LAYOUT(CliBinaryPathNs, copy_adjacent);
EMBED_TABLE_STORAGE CliBinaryPathNs cli_binary_path EMBED_UNUSED = {
    .copy_adjacent = cli_binary_copy_adjacent,
};

typedef struct
{
    unsigned long long (*number)(const CliBinaryBytesSizeCfg *args);
} CliBinaryBytesSizeNs;
EMBED_TABLE_LAYOUT(CliBinaryBytesSizeNs, number);
EMBED_TABLE_STORAGE CliBinaryBytesSizeNs cli_binary_bytes_size EMBED_UNUSED = {
    .number = cli_binary_number,
};

typedef struct
{
    int (*text)(const CliBinaryBytesCfg *args);
} CliBinaryBytesNs;
EMBED_TABLE_LAYOUT(CliBinaryBytesNs, text);
EMBED_TABLE_STORAGE CliBinaryBytesNs cli_binary_bytes EMBED_UNUSED = {
    .text = cli_binary_text,
};

typedef struct
{
    int (*elf_read)(const CliBinaryPathReportCfg *args);
} CliBinaryPathReportNs;
EMBED_TABLE_LAYOUT(CliBinaryPathReportNs, elf_read);
EMBED_TABLE_STORAGE CliBinaryPathReportNs cli_binary_path_report EMBED_UNUSED = {
    .elf_read = cli_binary_elf_read,
};

typedef struct
{
    unsigned int (*linux_folders)(const CliBinaryCfg *args);
} CliBinaryNs;
EMBED_TABLE_LAYOUT(CliBinaryNs, linux_folders);
EMBED_TABLE_STORAGE CliBinaryNs cli_binary EMBED_UNUSED = {
    .linux_folders = cli_binary_linux_folders,
};

typedef struct
{
    int (*pe_imports)(const CliBinaryErrMsgCfg *args);
} CliBinaryErrMsgNs;
EMBED_TABLE_LAYOUT(CliBinaryErrMsgNs, pe_imports);
EMBED_TABLE_STORAGE CliBinaryErrMsgNs cli_binary_err_msg EMBED_UNUSED = {
    .pe_imports = cli_pe_imports,
};

typedef struct
{
    int (*elf_dynamic)(const CliBinaryErrMsgBytesCfg *args);
} CliBinaryErrMsgBytesNs;
EMBED_TABLE_LAYOUT(CliBinaryErrMsgBytesNs, elf_dynamic);
EMBED_TABLE_STORAGE CliBinaryErrMsgBytesNs cli_binary_err_msg_bytes EMBED_UNUSED = {
    .elf_dynamic = cli_elf_dynamic,
};

#endif
