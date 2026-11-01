// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pages_shadow.c: the paths beside a paged text's file, and where a written piece's lines lie in the shadow beside it
#include "../command_line/cells.h"
#include "../text/text.h"
#include "pages_internal.h"

void cli_page_beside(const char *path, const char *ending, char *beside)
{
    char parent[CLI_PATH_MAX_LEN];
    const char *name = path;
    const unsigned long long name_past =
        cli_text_back_any(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), "/\\");
    name = (name_past != 0ull) ? (path + name_past) : name;
    VerbaLinea line = {beside, CLI_PATH_MAX_LEN, 0ull};
    if (cli_path_parent(path, parent))
    {
        mmgr_line_put(&line, parent);
        mmgr_line_ch(&line, '/');
    }
    mmgr_line_ch(&line, '.');
    mmgr_line_put(&line, name);
    mmgr_line_put(&line, ending);
    mmgr_line_finish(&line);
}

unsigned long long cli_page_written_at(const CliPaged *one, const CliPagePiece *piece, unsigned long long line)
{
    if (line >= piece->lines)
    {
        return piece->to;
    }
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_taken(one->shadow, &size, &g_cli_editor_cells);
    const unsigned long long end = (piece->to < size) ? piece->to : size;
    unsigned long long at = piece->from;
    for (unsigned long long passed = 0ull; (passed < line) && (at < end); passed += 1ull)
    {
        at = cli_text_line_at((const char *)bytes, at, end).next;
    }
    cli_file_given(bytes, &g_cli_editor_cells);
    return at;
}
