// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// install.h: orior installed from the folder its programs stand in, and taken out again (install.c). The install shows
// the license and takes the reader's answer, copies impero and tabvla with the license texts into the install folder,
// puts `imp` and `tab` on the PATH where neither name is taken, writes the defaults, and keeps a record of what it
// made. The uninstall reads the record and removes exactly that, and with --files the reader's orior folder too.
//
// `imp` and `tab` reach the PATH as links in ~/.local/bin, or as shell aliases with --aliases; on Windows as .cmd shims
// in the install folder's bin, which the install puts on the reader's Path. A name that already answers to another
// program on the PATH is left to that program, said so, and --no-imp or --no-tab leaves a name out by choice
#ifndef CLI_INSTALL_H
#define CLI_INSTALL_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliInstallCfg CliInstallCfg;

// the programs an install places, in the order of the options' switches; the record the install leaves in its folder;
// the folder of the shims on Windows and of the links elsewhere; and the setting the install writes a default for
#define CLI_INSTALL_PROGRAMS 2u
#define CLI_INSTALL_RECORD "install.tsv"
#define CLI_INSTALL_SHIMS "bin"
#define CLI_INSTALL_LINKS ".local/bin"
#define CLI_INSTALL_PORT_KEY "debug-port"
#define CLI_INSTALL_PORT_DEFAULT "off"

// what the install's or the uninstall's words asked for: the install folder, "" for the default; the license accepted
// on the command line; every question answered yes; shell aliases in place of links; each program left out; the
// reader's files removed by the uninstall; and the debugging port's setting, "" where not given
typedef struct
{
    char folder[CLI_PATH_MAX_LEN];
    int accepted;
    int yes;
    int aliases;
    int skipped[CLI_INSTALL_PROGRAMS];
    int files;
    char port[16];
} CliInstallOptions;

// the names of the two programs on the command line, impero's then tabvla's
extern const char *const g_cli_install_names[CLI_INSTALL_PROGRAMS];

// the words after `install` or `uninstall` read into `options`: 1, or 0 with why written to `err_msg`
int cli_install_options(const CliInstallCfg *args);
// the default install folder into `folder_out`: Programs/orior under `local_app_data` on Windows, .local/orior under
// `home` elsewhere; 1, or 0 where the base is empty
int cli_install_default_folder(const CliInstallCfg *args);
// whether `name` already names a program in one of the `folder_count` PATH folders at `folders` other than `own` and
// `into`: that folder's index plus one, or 0 where no folder answers to the name
unsigned int cli_install_collision(const CliInstallCfg *args);
// the install run with `options`, each line of its account handed to `say` and each question to `asked`: 1 where it
// ended whole, or 0 with why written to `err_msg`
int cli_install_run(const CliInstallCfg *args);
// the uninstall run with `options`, read from the record beside the program or in the default folder: 1, or 0 with why
int cli_uninstall_run(const CliInstallCfg *args);

struct CliInstallCfg
{
    const char *const *words;
    unsigned int count;
    CliInstallOptions *options;
    VerbaLinea *err_msg;
    const char *name;
    const char (*folders)[CLI_PATH_MAX_LEN];
    unsigned int folder_count;
    const char *own;
    const char *into;
    char *folder_out;
    const char *home;
    const char *local_app_data;
    void (*say)(void *context, const char *text);
    int (*asked)(void *context, const char *question);
    void *context;
};

typedef struct
{
    int (*options)(const CliInstallCfg *args);
    int (*default_folder)(const CliInstallCfg *args);
    unsigned int (*collision)(const CliInstallCfg *args);
    int (*install)(const CliInstallCfg *args);
    int (*uninstall)(const CliInstallCfg *args);
} CliInstallNs;
EMBED_TABLE_LAYOUT(CliInstallNs, options, default_folder, collision, install, uninstall);
EMBED_TABLE_STORAGE CliInstallNs cli_install EMBED_UNUSED = {
    .options = cli_install_options,
    .default_folder = cli_install_default_folder,
    .collision = cli_install_collision,
    .install = cli_install_run,
    .uninstall = cli_uninstall_run,
};

#endif
