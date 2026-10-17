// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// vendor.c: the vendor's instruction set for the part the cross reads for, and whether its disassembler reads a piece
#include "msq_internal.h"

#include "../tables/read.h"

// the vendor's instruction set for the part, read once a process; empty where the part's tables are not pulled
static TablesSet s_tables;

static int s_tables_state;

// The vendor's instruction set for the part the cross reads for, read from the run where the run holds it and from
// the tables the tables pass pulled otherwise: the part named by the architecture, `SM86` the part `sm_86`. 1 where it
// holds a class, 0 where the tables are not pulled or do not read
int msq_tables_read(void)
{
    if (s_tables_state != 0)
    {
        return s_tables_state > 0;
    }
    s_tables_state = -1;
    char part[32] = "";
    size_t length = 0u;
    for (const char *at = s_architecture; (*at != '\0') && (length < (sizeof(part) - 2u)); at += 1)
    {
        const char c = (char)(((*at >= 'A') && (*at <= 'Z')) ? (*at - 'A' + 'a') : *at);
        // the part's name puts an underscore between its letters and its digits
        if ((length != 0u) && (c >= '0') && (c <= '9') && !((part[length - 1u] >= '0') && (part[length - 1u] <= '9')))
        {
            part[length] = '_';
            length += 1u;
        }
        part[length] = c;
        length += 1u;
        part[length] = '\0';
    }
    char path[512];
    snprintf(path, sizeof(path), "build/vendor/nvidia/tables/%s_instructions.txt", part);
    unsigned char *text = NULL;
    unsigned long long size = 0ull;
    if (!qry_file_read(s_run, path, &text, &size))
    {
        return 0;
    }
    const int read = tables_set_read(&s_tables, text, (size_t)size) && (s_tables.count != 0u);
    free(text);
    s_tables_state = read ? 1 : -1;
    return read;
}

// 1 where every instruction of `piece` is one a class of the vendor's instruction set reads, its opcode field holding
// the class's opcode: the disassembler reads each, and stops at none
int msq_vendor_reads(const MsqBytes *piece)
{
    for (unsigned long long at = 0ull; (at + 16ull) <= piece->size; at += 16ull)
    {
        unsigned long long low = 0ull;
        unsigned long long high = 0ull;
        memcpy(&low, piece->bytes + at, 8u);
        memcpy(&high, piece->bytes + at + 8u, 8u);
        if (tables_class_of(&s_tables, low, high) == NULL)
        {
            return 0;
        }
    }
    return 1;
}
