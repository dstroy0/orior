// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pages_shadow.c: the paths beside a paged text's file, and where a written piece's lines lie in the shadow beside it
#include "pages_internal.h"

void cli_page_beside(const char *path, const char *ending, char *beside)
{
    char parent[CLI_PATH_LONGEST];
    const char *name = path;
    for (const char *at = path; at[0] != '\0'; at += 1)
    {
        name = ((at[0] == '/') || (at[0] == '\\')) ? (at + 1) : name;
    }
    CliLine line = {beside, CLI_PATH_LONGEST, 0ull};
    if (cli_path_parent(path, parent))
    {
        cli_line_text(&line, parent);
        cli_line_character(&line, '/');
    }
    cli_line_character(&line, '.');
    cli_line_text(&line, name);
    cli_line_text(&line, ending);
    cli_line_closed(&line);
}

unsigned long long cli_page_written_at(const CliPaged *one, const CliPagePiece *piece, unsigned long long line)
{
    if (line >= piece->lines)
    {
        return piece->to;
    }
    unsigned long long size = 0ull;
    const unsigned char *const bytes = qry_member_file_mapped(one->shadow, &size);
    const unsigned long long end = ((bytes != NULL) && (piece->to < size)) ? piece->to : size;
    unsigned long long at = piece->from;
    for (unsigned long long passed = 0ull; (bytes != NULL) && (passed < line) && (at < end); passed += 1ull)
    {
        const unsigned char *const newline =
            memor.chr(&(MemoriaCfg){.src = bytes + at, .val = '\n', .bytes = end - at});
        // a newline found lies in the piece, at or past `at`
        at = (newline != NULL) ? ((unsigned long long)(newline - bytes) + 1ull) : end;
    }
    if (bytes != NULL)
    {
        qry_member_file_unmapped(bytes, size);
    }
    return at;
}
