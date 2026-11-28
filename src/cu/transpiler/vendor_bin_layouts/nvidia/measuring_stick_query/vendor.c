// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// vendor.c: the vendor's instruction set for the part the cross reads for, and whether its disassembler reads a piece
#include "../../../../../../includes/platform/platform.h"
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "memoria_operor/memoria_operor.h"
#include "msq_internal.h"

#include "../tables/read.h"

// the instruction set of the vendor for the part, read once for each process. It is empty where the tables pass did
// not pull the tables of the part
static TablesSet s_tables;

static int s_tables_state;

// The instruction set of the vendor for the part that the cross reads for. The call reads it from the run where the
// run holds it, and else from the tables that the tables pass pulled. The architecture names the part, with `SM86` the
// part `sm_86`. The return is 1 where it holds a class, and 0 where the tables pass did not pull the tables, or they do
// not read
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
        // Explicit cast reads the byte as its code point
        const char c = (char)EMBED_CALL(ascii.lower, AsciiCfg, .byte = (uint8_t)*at);
        // the part's name puts an underscore between its characters and its digits
        // Explicit cast reads the byte as its code point
        if ((length != 0u) && EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_NUM, .byte = (uint8_t)c) &&
            !EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_NUM, .byte = (uint8_t)part[length - 1u]))
        {
            part[length] = '_';
            length += 1u;
        }
        part[length] = c;
        length += 1u;
        part[length] = '\0';
    }
    char path[512];
    {
        VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put(&buffer_line, "build/vendor/nvidia/tables/");
        mmgr_line_put(&buffer_line, part);
        mmgr_line_put(&buffer_line, "_instructions.txt");
        (void)mmgr_line_finish(&buffer_line);
    }
    const unsigned char *text = NULL;
    unsigned long long size = 0ULL;
    if (!qry_member_file_blob(path, &text, &size))
    {
        return 0;
    }
    const int read = tables_set_read(&s_tables, text, (size_t)size) && (s_tables.count != 0u);
    s_tables_state = read ? 1 : -1;
    return read;
}

// 1 where a class of the instruction set of the vendor reads every instruction of `piece`, where its opcode field holds
// the opcode of the class. Then the disassembler reads each, and stops at none
int msq_vendor_reads(const MsqBytes *piece)
{
    for (unsigned long long at = 0ULL; (at + 16ULL) <= piece->size; at += 16ULL)
    {
        unsigned long long low = 0ULL;
        unsigned long long high = 0ULL;
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &low, .src = piece->bytes + at, .bytes = 8u);
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &high, .src = piece->bytes + at + 8u, .bytes = 8u);
        if (tables_class_of(&s_tables, low, high) == NULL)
        {
            return 0;
        }
    }
    return 1;
}
