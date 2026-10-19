// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// mapped_test.c: the mapping's check: a file mapped reads as its bytes, one of no bytes as none, and one not there
// maps to nothing
#include "cli_check.h"

int main(void)
{
    static unsigned char s_bytes[(1u << 20u) + 4099u];
    static const unsigned long long s_lengths[2] = {70000ull, (1ull << 20u) + 4099ull};
    char folder[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    cli_check_folder("mapped", folder);
    cli_full(folder, "bytes.txt", full);
    for (unsigned int each = 0u; each < 2u; each += 1u)
    {
        const unsigned long long length = s_lengths[each];
        for (unsigned long long at = 0ull; at < length; at += 1ull)
        {
            s_bytes[at] = (unsigned char)(at % 251ull);
        }
        (void)query_file_written(full, s_bytes, length);
        unsigned long long size = 0ull;
        const unsigned char *const mapped = qry_member_file_mapped(full, &size);
        char what[64];
        CliLine line = {what, sizeof(what), 0ull};
        cli_line_decimal(&line, length, 1u);
        cli_line_text(&line, " bytes map as they are");
        (void)cli_line_closed(&line);
        cli_check((mapped != NULL) && (size == length) &&
                      (memor.cmp(&(MemoriaCfg){.src = mapped, .other = s_bytes, .bytes = length}) == 0),
                  what);
        if (mapped != NULL)
        {
            qry_member_file_unmapped(mapped, size);
        }
    }
    cli_full(folder, "empty.txt", full);
    (void)query_file_written(full, "", 0ull);
    unsigned long long size = 1ull;
    const unsigned char *const empty = qry_member_file_mapped(full, &size);
    cli_check((empty != NULL) && (size == 0ull), "a file of no bytes maps to none");
    if (empty != NULL)
    {
        qry_member_file_unmapped(empty, size);
    }
    cli_full(folder, "absent.txt", full);
    cli_check(qry_member_file_mapped(full, &size) == NULL, "a file not there maps to nothing");
    return cli_checked();
}
