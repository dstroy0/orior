// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_sea.c: Node's single executable on Windows: a program's file ends in .exe, and a copy of node takes the blob
// as the resource NODE_SEA_BLOB after node's signature comes off
#include "../command_line/callback_cfg.h"
#include "../programs/sea.h"

#if defined(_WIN32)

typedef struct
{
    QuerySink sink;
    const char *text;
    const char *path;
    const char *how;
    VerbaLinea *err_msg;
} WindowsSeaCtx;

// a line printed through `sink`
EMBED_INLINE void sea_print(const WindowsSeaCtx *args)
{
    EMBED_CALL(args->sink.print, CliEachLineSizeCfg, .context = args->sink.context, .line = args->text,
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES}));
}

// the report that a read or a write of `path` failed
EMBED_INLINE int sea_fail(const WindowsSeaCtx *args)
{
    mmgr_line_put(args->err_msg, args->path);
    mmgr_line_put(args->err_msg, args->how);
    return 0;
}

const char *cli_sea_program_extension(void)
{
    return "exe";
}

int cli_sea_older_node(const CliSeaSystemCfg *args)
{
    // a Windows copy of node takes the blob as a resource, whatever node's version
    (void)args;
    return 1;
}

// The copy of node with the blob added, as Windows takes it. Node's signature comes off, the blob goes in as a
// resource, and the fuse turns. The copy goes beside the output, and then moves over it
int cli_sea_copy(const CliSeaSystemCfg *args)
{
    const unsigned char *program = args->program;
    unsigned long long size = args->size;
    const unsigned char *data = args->data;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long entry = 0ULL;
    unsigned long long length = size;
    const int cut = EMBED_CALL(cli_sea_err_msg_program.signature, CliSeaErrMsgProgramCfg, .program = program,
                               .size = size, .entry = &entry, .length = &length, .err_msg = err_msg);
    if (cut < 0)
    {
        return 0;
    }
    if (cut > 0)
    {
        EMBED_CALL(sea_print, WindowsSeaCtx, .sink = args->sink, .text = "took off node's signature");
    }
    const CliFilePiece unsigned_pieces[3] = {{program, (cut > 0) ? entry : length},
                                             {NULL, (cut > 0) ? 8ULL : 0ULL},
                                             {program + entry + 8ULL, (cut > 0) ? (length - entry - 8ULL) : 0ULL}};
    if (!EMBED_CALL(cli_path_count_internal.file_pieces_write, CliPathCountCtx, .path = args->output,
                    .pieces = unsigned_pieces, .count = 3u))
    {
        return EMBED_CALL(sea_fail, WindowsSeaCtx, .path = args->output, .how = ": it could not be written",
                          .err_msg = err_msg);
    }
    if (!EMBED_CALL(cli_path_err_msg_internal.resource_add, CliPathErrMsgCtx, .path = args->output,
                    .name = "NODE_SEA_BLOB", .data = data, .size = args->data_size, .err_msg = err_msg))
    {
        return 0;
    }
    char print_line[CLI_NAME_MAX_LEN];
    VerbaLinea line = {print_line, sizeof(print_line), 0ULL};
    mmgr_line_put(&line, "added ");
    mmgr_line_decimal(&line, args->data_size, 1u);
    mmgr_line_put(&line, " bytes as the resource NODE_SEA_BLOB");
    mmgr_line_finish(&line);
    EMBED_CALL(sea_print, WindowsSeaCtx, .sink = args->sink, .text = print_line);
    unsigned long long created_size = 0ULL;
    const unsigned char *const made = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = args->output,
                                                 .size_pointer = &created_size, .owner = g_cli_sea_owner);
    if (made == NULL)
    {
        return EMBED_CALL(sea_fail, WindowsSeaCtx, .path = args->output, .how = ": it could not be read",
                          .err_msg = err_msg);
    }
    unsigned long long fuse = 0ULL;
    char fused[CLI_PATH_MAX_LEN];
    VerbaLinea fused_line = {fused, sizeof(fused), 0ULL};
    mmgr_line_put(&fused_line, args->output);
    mmgr_line_put(&fused_line, ".fused");
    static const unsigned char s_turned = '1';
    int done = (mmgr_line_finish(&fused_line) != 0ULL) &&
               EMBED_CALL(cli_sea_err_msg_program_size.fuse_at, CliSeaErrMsgProgramSizeCfg, .program = made,
                          .size = created_size, .at = &fuse, .err_msg = err_msg);
    if (done)
    {
        const CliFilePiece pieces[3] = {{made, fuse}, {&s_turned, 1ULL}, {made + fuse + 1, created_size - fuse - 1ULL}};
        done = EMBED_CALL(cli_path_count_internal.file_pieces_write, CliPathCountCtx, .path = fused, .pieces = pieces,
                          .count = 3u);
        if (!done)
        {
            EMBED_CALL(sea_fail, WindowsSeaCtx, .path = fused, .how = ": it could not be written", .err_msg = err_msg);
        }
    }
    g_cli_guard->persistent_buf_release((void *)(embed_word)made, (g_cli_sea_owner));
    if (done && !EMBED_CALL(query_from_internal.file_rename, QueryFromCtx, .from = fused, .to = args->output))
    {
        return EMBED_CALL(sea_fail, WindowsSeaCtx, .path = args->output, .how = ": it could not be replaced",
                          .err_msg = err_msg);
    }
    return done;
}
#endif
