// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// binary.h: what a built program loads as it starts, read from the file itself (binary.c reads a Windows program's
// imports, binary_elf.c a Linux program's needs): a Windows program's imports and its delay-loaded imports, and a Linux
// program's needed libraries, its run paths and the glibc versions it asks for. Each library is looked for as the
// system's loader looks for it where the program is opened on its own, and the libraries those load in turn, past the
// system's own. A library found only through the folders orior puts on its jobs' PATH, one found nowhere, one found by
// a path of the machine that built the program, and Visual Studio's debug runtime are each told, as the things that
// start a program here and keep it from starting elsewhere
#ifndef BINARY_H
#define BINARY_H

#include "../command_line/cli_internal.h"

// the most libraries a report holds, and the most run paths of the machine that built the program
#define CLI_BINARY_LIBRARIES 512u

#define CLI_BINARY_BUILD_PATHS 32u

// the most folders the Linux loader looks in after a program's own run paths
#define CLI_BINARY_LINUX_FOLDERS 256u

// Where a library a program loads is found: in the program's own folder or by a run path from it; in the system's own
// folders or one Windows resolves itself; there, put by the Visual C++ Redistributable, which a machine without it
// lacks; Visual Studio's debug runtime, which only a machine with Visual Studio has; through the PATH the system gives
// a program opened on its own; only through the folders orior puts on its jobs' PATH; by a run path that names a folder
// of the machine that built the program; or nowhere
enum
{
    CLI_FOUND_BESIDE,
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
    char name[CLI_NAME_LONGEST];
    unsigned int found;
    char at[CLI_PATH_LONGEST];
    int has_at;
    char by[CLI_NAME_LONGEST];
} CliLibrary;

// What a program loads: its libraries, the run paths of a Linux program that name folders of the machine that built
// it, and the newest glibc version a Linux program asks for, as GLIBC_2.38
typedef struct
{
    CliLibrary library[CLI_BINARY_LIBRARIES];
    unsigned int libraries;
    char build_path[CLI_BINARY_BUILD_PATHS][CLI_PATH_LONGEST];
    unsigned int build_paths;
    char glibc[CLI_NAME_LONGEST];
    int has_glibc;
} CliBinaryReport;

// what the program at `path` loads read into `report`, each library looked for as the system's loader looks for it
// where the program is opened on its own, and then, where it is not found so, through `orior_path`, `count` folders
// orior's jobs have on their PATH: 1, or 0 with why said
int cli_binary_read(const char *path, const char (*orior_path)[CLI_PATH_LONGEST], unsigned int count,
                    CliBinaryReport *report, CliLine *said);
// what a report tells of the program, each line handed to `say`: each library found other than beside it or in the
// system's own folders, each run path of the machine that built it, and the newest glibc it asks for
void cli_binary_report_said(const CliBinaryReport *report, QuerySink say);
// the program at `path` read and what it loads said line by line to `say`: 1, or 0 where it could not be read
int cli_binary_said(const char *path, const char (*orior_path)[CLI_PATH_LONGEST], unsigned int count, QuerySink say);
// each library the program loads through a PATH, this machine's or orior's, copied beside it, each name handed to
// `copied`: 1, or 0 with why said
int cli_binary_copy_beside(const char *path, const CliBinaryReport *report,
                           void (*copied)(void *context, const char *name), void *context, CliLine *said);

// what binary.c and binary_elf.c share: a little-endian number read from the bytes at `at`, 0 past their end; the text
// ended by a 0 at `at` into `out`, which holds `room`: 1, or 0 where it has no end; a Linux program's libraries read
// into the report; and the folders the Linux loader looks in after a program's own run paths into `folders`, which
// hold CLI_BINARY_LINUX_FOLDERS, those /etc/ld.so.conf names and the files it includes, then the system's own library
// folders: their count
unsigned long long cli_binary_number(const unsigned char *bytes, unsigned long long size, unsigned long long at,
                                     unsigned int width);
int cli_binary_text(const unsigned char *bytes, unsigned long long size, unsigned long long at, char *out,
                    unsigned long long room);
int cli_binary_elf_read(const unsigned char *bytes, unsigned long long size, const char *path, CliBinaryReport *report,
                        CliLine *said);
unsigned int cli_binary_linux_folders(char (*folders)[CLI_PATH_LONGEST]);

// A Windows program's machine and the DLLs it imports, the delay-loaded ones among them, each put once, case aside,
// after the names from `count`, which hold `room`: the count after, or -1 with why said (binary.c)
int cli_pe_imports(const unsigned char *bytes, unsigned long long size, unsigned int *machine,
                   char (*names)[CLI_NAME_LONGEST], int count, unsigned int room, CliLine *said);

// What a dynamic section gives: its needed libraries, its RPATH and RUNPATH, and the newest glibc it asks for
typedef struct
{
    char needed[64][CLI_NAME_LONGEST];
    unsigned int count;
    char rpath[CLI_PATH_LONGEST * 2u];
    char runpath[CLI_PATH_LONGEST * 2u];
    char glibc[CLI_NAME_LONGEST];
    int has_glibc;
} CliDynamic;

// The dynamic section of a 64-bit little-endian program read into `found`: 1, or 0 with why said (binary_elf.c)
int cli_elf_dynamic(const unsigned char *bytes, unsigned long long size, CliDynamic *found, CliLine *said);

#endif
