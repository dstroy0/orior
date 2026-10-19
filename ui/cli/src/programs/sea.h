// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sea.h: Node's single executable: a script built into a copy of node (sea.c, sea_build.c). From Node 25.5,
// `node --build-sea` makes it. Before that, orior takes Node's documented steps itself, with no package fetched to
// take them: `node --experimental-sea-config` makes the script's blob, node is copied, the copy's signature is taken
// off, the blob is added to it as the resource NODE_SEA_BLOB on Windows or as the note of that name in a segment of its
// own on Linux, and the fuse is turned from 0 to 1.
//
// The executable loads Node's own modules alone: a script that requires another file or a package is told so before
// the build, since only a bundler such as Bun or Deno takes those in.
#ifndef SEA_H
#define SEA_H

#include "../command_line/cli_internal.h"

// the most program headers a Linux node's are read with, the two this adds among them
#define CLI_SEA_HEADERS 4096u

// the modules a script asks for that Node does not hold itself, each named in a require(), an import or an import(),
// other than a `node:` name or one of Node's own, into `found`, which holds `room`: their count
unsigned int cli_sea_outside(const char *text, char (*found)[CLI_NAME_LONGEST], unsigned int room);
// the script at `main` built into the single executable `output` with `node`, `module` saying the script is an ES
// module, each step said through `say` as it goes: the executable's path into `made`, which holds CLI_PATH_LONGEST, 1,
// or 0 with why said
int cli_sea_build(const char *node, const char *main, const char *output, int module, QuerySink say, char *made,
                  CliLine *said);

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

// what the checks hold, each on a program's bytes: the place of the fuse's 0, 1, or 0 with why said where the program
// has no fuse or has it turned; a Windows program's signature: 1 with the place of the certificate table's entry to
// empty and the length the file is cut to, 0 where it has none, -1 with why said where it is no Windows program; and
// the note of `data_size` bytes a Linux program takes, 1, or 0 with why said
int cli_sea_fuse_at(const unsigned char *program, unsigned long long size, unsigned long long *at, CliLine *said);
int cli_sea_signature(const unsigned char *program, unsigned long long size, unsigned long long *entry,
                      unsigned long long *length, CliLine *said);
int cli_sea_note(const unsigned char *program, unsigned long long size, unsigned long long data_size, CliSeaNote *note,
                 CliLine *said);

#endif
