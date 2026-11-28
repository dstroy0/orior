// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// classify_walk.c: the program that a classify walk runs as, inside a probe that the interface can lose. It puts the
// type query at each address of a span. It passes one line for each address, after its answer, to the run of the
// process that started it. The record is QUERY_WALK_RECORD:
//
//     classify_walk <from> <count> <stride>
//
// The program reads every number as hexadecimal. It passes a line before it queries the next address. An address that
// ends the process ends it with every line before it already passed. The interface reads where the walk stopped.
#include "../../../../../../includes/file_defs/qry/writer/qry_member.h"
#include "../../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "query_put.h"
#include "verba_scribo/verba_linea.h"

int program_main(int count_of_words, char **words)
{
    if (count_of_words != 4)
    {
        return 2;
    }
    const unsigned long long from =
        EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = words[1], .end = NULL, .base = 16);
    const unsigned long long count =
        EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = words[2], .end = NULL, .base = 16);
    const unsigned long long stride =
        EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = words[3], .end = NULL, .base = 16);
    for (unsigned long long number = 0ULL; number < count; number += 1ULL)
    {
        const unsigned long long at = from + (number * stride);
        unsigned int read_back = 0u;
        const unsigned int type = host_address_probe(at, &read_back);
        // an address and two words in hex, each in the room of a 64-bit word's digits
        char text[64];
        VerbaLinea line = {text, sizeof(text), 0ULL};
        mmgr_line_hex(&line, at, 0u);
        mmgr_line_ch(&line, ' ');
        mmgr_line_hex(&line, type, 0u);
        mmgr_line_ch(&line, ' ');
        mmgr_line_hex(&line, read_back, 0u);
        mmgr_line_ch(&line, '\n');
        if (qry_member_append(QUERY_WALK_RECORD, text, line.at, QRY_ORDINARY) == 0ULL)
        {
            return 3;
        }
    }
    return 0;
}
