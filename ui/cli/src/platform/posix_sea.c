// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_sea.c: Node's single executable on Linux and macOS: a program's file takes no extension, and a copy of node
// takes the blob as the note NODE_SEA_BLOB in a segment of its own. On macOS a node older than 25.5 needs postject,
// which impero does not run
#include "../command_line/callback_cfg.h"
#include "../programs/sea.h"

#if !defined(_WIN32)

typedef struct
{
    QuerySink sink;
    const char *text;
    const char *path;
    const char *how;
    VerbaLinea *err_msg;
} PosixSeaCtx;

// a line printed through `sink`
EMBED_INLINE void sea_print(const PosixSeaCtx *args)
{
    EMBED_CALL(args->sink.print, CliEachLineSizeCfg, .context = args->sink.context, .line = args->text,
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES}));
}

// the report that a read or a write of `path` failed
EMBED_INLINE int sea_fail(const PosixSeaCtx *args)
{
    mmgr_line_put(args->err_msg, args->path);
    mmgr_line_put(args->err_msg, args->how);
    return 0;
}

const char *cli_sea_program_extension(void)
{
    return NULL;
}

int cli_sea_older_node(const CliSeaSystemCfg *args)
{
#if defined(__APPLE__)
    mmgr_line_put(args->err_msg, "on macOS, Node ");
    mmgr_line_decimal(args->err_msg, args->major, 1u);
    mmgr_line_ch(args->err_msg, '.');
    mmgr_line_decimal(args->err_msg, args->minor, 1u);
    mmgr_line_put(args->err_msg, " needs postject to add the blob; Node 25.5 builds it itself, with --build-sea");
    return 0;
#else
    // a Linux copy of node takes the blob as a note, whatever node's version
    (void)args;
    return 1;
#endif
}

// the copy of node with the blob added, as Linux takes it: a note in a segment of its own, the fuse turned
int cli_sea_copy(const CliSeaSystemCfg *args)
{
    const unsigned char *program = args->program;
    unsigned long long size = args->size;
    unsigned long long data_size = args->data_size;
    VerbaLinea *err_msg = args->err_msg;
    static CliSeaNote s_note;
    unsigned long long fuse = 0ULL;
    if (!EMBED_CALL(cli_sea_err_msg.note, CliSeaErrMsgCfg, .program = program, .size = size, .data_size = data_size,
                    .note = &s_note, .err_msg = err_msg) ||
        !EMBED_CALL(cli_sea_err_msg_program_size.fuse_at, CliSeaErrMsgProgramSizeCfg, .program = program, .size = size,
                    .at = &fuse, .err_msg = err_msg))
    {
        return 0;
    }
    char print_line[CLI_NAME_MAX_LEN];
    VerbaLinea line = {print_line, sizeof(print_line), 0ULL};
    mmgr_line_put(&line, "added ");
    mmgr_line_decimal(&line, data_size, 1u);
    mmgr_line_put(&line, " bytes as the note NODE_SEA_BLOB");
    mmgr_line_finish(&line);
    EMBED_CALL(sea_print, PosixSeaCtx, .sink = args->sink, .text = print_line);
    // the program's head with its header table's place and count set to the new ones
    unsigned char head[64];
    memor.cpy(&(MemoriaCfg){.dst = head, .src = program, .bytes = 64u});
    for (unsigned int byte = 0u; byte < 8u; byte += 1u)
    {
        // each byte of the table's place, its low eight bits
        head[0x20u + byte] = (unsigned char)(s_note.region >> (8u * byte));
    }
    const unsigned long long count = s_note.table_size / 56ULL;
    // the count of headers, below 65536 as CLI_SEA_HEADERS bounds it, its two bytes
    head[0x38] = (unsigned char)count;
    head[0x39] = (unsigned char)(count >> 8u);
    static const unsigned char s_turned = '1';
    const CliFilePiece pieces[9] = {{head, 64ULL},
                                    {program + 64, fuse - 64ULL},
                                    {&s_turned, 1ULL},
                                    {program + fuse + 1, size - fuse - 1ULL},
                                    {NULL, s_note.region - size},
                                    {s_note.table, s_note.table_size},
                                    {NULL, s_note.note_at - s_note.region - s_note.table_size},
                                    {s_note.head, s_note.head_size},
                                    {args->data, data_size}};
    static const unsigned char s_pad[4] = {0u, 0u, 0u, 0u};
    const CliFilePiece padding = {s_pad, (4ULL - (data_size % 4ULL)) % 4ULL};
    CliFilePiece all[10];
    memor.cpy(&(MemoriaCfg){.dst = all, .src = pieces, .bytes = sizeof(pieces)});
    all[9] = padding;
    if ((fuse < 64ULL) || !EMBED_CALL(cli_path_count_internal.file_pieces_write, CliPathCountCtx, .path = args->output,
                                      .pieces = all, .count = 10u))
    {
        return EMBED_CALL(sea_fail, PosixSeaCtx, .path = args->output, .how = ": it could not be written",
                          .err_msg = err_msg);
    }
    EMBED_CALL(cli_path_file_internal.file_runnable, CliPathFileCtx, .path = args->output);
    return 1;
}
#endif
