// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// listed.c: a form the machine file names, read off the file's text as the run carried it in
#include "khw_write_internal.h"

int khw_form_listed(const unsigned char *bytes, unsigned long long size, unsigned long long low,
                    unsigned long long high)
{
    unsigned long long at = 0ull;
    while (at < size)
    {
        char line[512];
        size_t length = 0u;
        while ((at < size) && (bytes[at] != '\n'))
        {
            if (length < (sizeof(line) - 1u))
            {
                line[length] = (char)bytes[at];
                length += 1u;
            }
            at += 1ull;
        }
        at += 1ull;
        line[length] = '\0';
        char operation[64];
        char kinds[256];
        unsigned long long listed_low = 0ull;
        unsigned long long listed_high = 0ull;
        if ((strncmp(line, "form ", 5u) == 0) &&
            (sscanf(line + 5, "%63s %255s 0x%llx 0x%llx", operation, kinds, &listed_low, &listed_high) == 4) &&
            (listed_low == low) && (listed_high == high))
        {
            return 1;
        }
    }
    return 0;
}
