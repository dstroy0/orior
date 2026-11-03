// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sea.h: Node's single executable: a script built into a copy of node (sea.c, sea_build.c). From Node 25.5,
// `node --build-sea` makes it. Before that, orior_cli takes Node's documented steps itself, with no package fetched to
// take them: `node --experimental-sea-config` makes the script's blob, node is copied, the copy's signature is taken
// off, the blob is added to it as the resource NODE_SEA_BLOB on Windows or as the note of that name in a segment of its
// own on Linux, and the fuse is turned from 0 to 1.
//
// The executable loads Node's own modules alone: a script that requires another file or a package is informed so before
// the build, since only a bundler such as Bun or Deno takes those in.
#ifndef SEA_H
#define SEA_H
#include "../command_line/cells.h"
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliSeaNodeCfg CliSeaNodeCfg;
typedef struct CliSeaErrMsgCfg CliSeaErrMsgCfg;
typedef struct CliSeaErrMsgProgramCfg CliSeaErrMsgProgramCfg;
typedef struct CliSeaErrMsgProgramSizeCfg CliSeaErrMsgProgramSizeCfg;
typedef struct CliSeaCfg CliSeaCfg;

// the most program headers a Linux node's are read with, the two this adds among them
#define CLI_SEA_HEADERS 4096u

// the modules a script requires that Node does not contain itself, each named in a require(), an import or an import(),
// other than a `node:` name or one of Node's own, into `found`, sized for `capacity`: their count
unsigned int cli_sea_outside(const CliSeaCfg *args);
// the script at `main` built into the single executable `output` with `node`, `module` specifying the script is an ES
// module, each step printed through `sink` as it goes: the executable's path into `made`, sized for CLI_PATH_MAX_LEN,
// 1, or 0 with why given the cells a build occupies while it runs, the script read (sea_build.c)
extern CellOwner g_cli_sea_cells;
int cli_sea_build(const CliSeaNodeCfg *args);

// What adding a note to a Linux node changes: the program headers as they become, `table` bytes of them; where the new
// segment starts, past every segment the program loads, the table at its head; where the note starts in it; and the
// note's head, its sizes, type and name, `head` bytes, before the blob, which is padded to four bytes after
typedef struct
{
    unsigned char table[(CLI_SEA_HEADERS + 2u) * 56u];
    unsigned long long table_size;
    unsigned long long region;
    unsigned long long note_at;
    unsigned char head[32];
    unsigned long long head_size;
} CliSeaNote;

// what the checks check, each on a program's bytes: the place of the fuse's 0, 1, or 0 with why given where the program
// has no fuse or has it turned; a Windows program's signature: 1 with the place of the certificate table's entry to
// empty and the length the file is cut to, 0 where it has none, -1 with why given where it is no Windows program; and
// the note of `data_size` bytes a Linux program takes, 1, or 0 with why given
int cli_sea_fuse_at(const CliSeaErrMsgProgramSizeCfg *args);
int cli_sea_signature(const CliSeaErrMsgProgramCfg *args);
int cli_sea_note(const CliSeaErrMsgCfg *args);

struct CliSeaNodeCfg
{
    const char *node;
    const char *main;
    const char *output;
    int module;
    QuerySink sink;
    char *made;
    VerbaLinea *err_msg;
};

struct CliSeaErrMsgCfg
{
    VerbaLinea *err_msg;
    const unsigned char *program;
    unsigned long long size;
    unsigned long long data_size;
    CliSeaNote *note;
};

struct CliSeaErrMsgProgramCfg
{
    VerbaLinea *err_msg;
    const unsigned char *program;
    unsigned long long size;
    unsigned long long *entry;
    unsigned long long *length;
};

struct CliSeaErrMsgProgramSizeCfg
{
    VerbaLinea *err_msg;
    const unsigned char *program;
    unsigned long long size;
    unsigned long long *at;
};

struct CliSeaCfg
{
    const char *text;
    char (*found)[CLI_NAME_MAX_LEN];
    unsigned int capacity;
};

typedef struct
{
    unsigned int (*outside)(const CliSeaCfg *args);
} CliSeaNs;
EMBED_TABLE_LAYOUT(CliSeaNs, outside);
EMBED_TABLE_STORAGE CliSeaNs cli_sea_table EMBED_UNUSED = {
    .outside = cli_sea_outside,
};

typedef struct
{
    int (*build)(const CliSeaNodeCfg *args);
} CliSeaNodeNs;
EMBED_TABLE_LAYOUT(CliSeaNodeNs, build);
EMBED_TABLE_STORAGE CliSeaNodeNs cli_sea_node EMBED_UNUSED = {
    .build = cli_sea_build,
};

typedef struct
{
    int (*fuse_at)(const CliSeaErrMsgProgramSizeCfg *args);
} CliSeaErrMsgProgramSizeNs;
EMBED_TABLE_LAYOUT(CliSeaErrMsgProgramSizeNs, fuse_at);
EMBED_TABLE_STORAGE CliSeaErrMsgProgramSizeNs cli_sea_err_msg_program_size EMBED_UNUSED = {
    .fuse_at = cli_sea_fuse_at,
};

typedef struct
{
    int (*signature)(const CliSeaErrMsgProgramCfg *args);
} CliSeaErrMsgProgramNs;
EMBED_TABLE_LAYOUT(CliSeaErrMsgProgramNs, signature);
EMBED_TABLE_STORAGE CliSeaErrMsgProgramNs cli_sea_err_msg_program EMBED_UNUSED = {
    .signature = cli_sea_signature,
};

typedef struct
{
    int (*note)(const CliSeaErrMsgCfg *args);
} CliSeaErrMsgNs;
EMBED_TABLE_LAYOUT(CliSeaErrMsgNs, note);
EMBED_TABLE_STORAGE CliSeaErrMsgNs cli_sea_err_msg EMBED_UNUSED = {
    .note = cli_sea_note,
};

#endif
