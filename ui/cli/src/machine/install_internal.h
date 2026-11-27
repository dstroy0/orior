// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// install_internal.h: what install.c shares with each system's install, windows_install.c and posix_install.c. The
// install's lines said and recorded, and its paths joined and compared, are in install.c. How a system names a program,
// finds its default folder, puts a name on its PATH and removes the folder of a running program are in the platform
// files
#ifndef CLI_INSTALL_INTERNAL_H
#define CLI_INSTALL_INTERNAL_H
#include "install.h"

typedef struct InstallCtx InstallCtx;

// What an install's step works on. First the install's words and its account, and up to three texts to say. Then the
// folder the programs come from, the folder they go into, a name and a path. Then the line to write, the record, a
// program's place among the names, each program found, and whether a name reached the PATH
struct InstallCtx
{
    const CliInstallCfg *cfg;
    const char *text;
    const char *other;
    const char *folder;
    const char *into;
    const char *name;
    const char *path;
    VerbaLinea *line;
    VerbaLinea *record;
    unsigned int at;
    int present[CLI_INSTALL_PROGRAMS];
    int placed;
};

// A line of the account passed to the caller, made of up to three texts. Then `folder`/`name` written into `line`,
// with its length. Then a line of the record, its type `text`, a tab and its value `other`. Then whether `folder` and
// `other` name one folder, each compared by its key (install.c)
void cli_install_said(const InstallCtx *args);
unsigned long long cli_install_joined(const InstallCtx *args);
void cli_install_recorded(const InstallCtx *args);
int cli_install_same_folder(const InstallCtx *args);

// The default install folder into `folder_out` of `cfg`, its base read from the system's environment: 1, or 0 where
// the base is empty. Then the program at `path` made runnable by its owner alone after its copy: 1, or 0 where the
// system refused (windows_install.c, posix_install.c)
int cli_install_default_found(const CliInstallCfg *args);
int cli_install_runnable(const InstallCtx *args);

// The name `name` put on the PATH for the program in `into`, the place recorded and said: 1, or 0 with why. Then the
// folder that the names answer from, written into `line`. Then the reader's home folder written into `line`, empty
// where the system puts no link under it (windows_install.c, posix_install.c)
int cli_install_place(InstallCtx *args);
void cli_install_answers(const InstallCtx *args);
void cli_install_home(const InstallCtx *args);

// The install folder `into` removed after this program ends, where the program runs from it as `folder` names it.
// The result is 1 where the system removes it later, and 0 where the caller removes it now. It is -1, with why, where
// the later removal could not begin (windows_install.c, posix_install.c)
int cli_install_removed_later(const InstallCtx *args);

typedef struct
{
    void (*said)(const InstallCtx *args);
    unsigned long long (*joined)(const InstallCtx *args);
    void (*recorded)(const InstallCtx *args);
    int (*same_folder)(const InstallCtx *args);
} CliInstallSharedNs;
EMBED_TABLE_LAYOUT(CliInstallSharedNs, said, joined, recorded, same_folder);
EMBED_TABLE_STORAGE CliInstallSharedNs cli_install_shared EMBED_UNUSED = {
    .said = cli_install_said,
    .joined = cli_install_joined,
    .recorded = cli_install_recorded,
    .same_folder = cli_install_same_folder,
};

typedef struct
{
    int (*default_found)(const CliInstallCfg *args);
    int (*runnable)(const InstallCtx *args);
    int (*place)(InstallCtx *args);
    void (*answers)(const InstallCtx *args);
    void (*home)(const InstallCtx *args);
    int (*removed_later)(const InstallCtx *args);
} CliInstallSystemNs;
EMBED_TABLE_LAYOUT(CliInstallSystemNs, default_found, runnable, place, answers, home, removed_later);
EMBED_TABLE_STORAGE CliInstallSystemNs cli_install_system EMBED_UNUSED = {
    .default_found = cli_install_default_found,
    .runnable = cli_install_runnable,
    .place = cli_install_place,
    .answers = cli_install_answers,
    .home = cli_install_home,
    .removed_later = cli_install_removed_later,
};

#endif
