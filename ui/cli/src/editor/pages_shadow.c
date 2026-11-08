// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pages_shadow.c: the paths beside a paged text's file, and where a written piece's lines lie in the shadow beside it
#include "../command_line/pool.h"
#include "../text/text.h"
#include "pages_internal.h"

void cli_page_adjacent(const CliPagesPathCtx *args)
{
    char parent[CLI_PATH_MAX_LEN];
    const char *name = args->path;
    const unsigned long long name_end =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}), .set = "/\\");
    name = (name_end != 0ULL) ? (args->path + name_end) : name;
    VerbaLinea line = {args->adjacent, CLI_PATH_MAX_LEN, 0ULL};
    if (EMBED_CALL(cli_path_out_type_internal.path_parent, CliPathOutTypeCtx, .path = args->path, .out = parent))
    {
        mmgr_line_put(&line, parent);
        mmgr_line_ch(&line, '/');
    }
    mmgr_line_ch(&line, '.');
    mmgr_line_put(&line, name);
    mmgr_line_put(&line, args->ending);
    mmgr_line_finish(&line);
}

unsigned long long cli_page_write_at(const CliPagesPieceCtx *args)
{
    if (args->line >= args->piece->lines)
    {
        return args->piece->to;
    }
    unsigned long long size = 0ULL;
    const unsigned char *const bytes =
        EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = args->one_read->shadow, .size_pointer = &size,
                   .owner = g_cli_editor_owner);
    const unsigned long long end = (args->piece->to < size) ? args->piece->to : size;
    unsigned long long at = args->piece->from;
    for (unsigned long long passed = 0ULL; (passed < args->line) && (at < end); passed += 1ULL)
    {
        at = EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = (const char *)bytes, .at = at, .size = end).next;
    }
    g_cli_guard->persistent_buf_release((void *)(uintptr_t)bytes, (g_cli_editor_owner));
    return at;
}
