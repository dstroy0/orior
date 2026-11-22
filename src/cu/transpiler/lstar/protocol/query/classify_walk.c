// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// classify_walk.c: the program a classify walk runs as, inside a probe the interface can lose. It puts the type query
// at each address of a span and passes the run of the process that started it one line an address as it is answered,
// the record QUERY_WALK_RECORD:
//
//     classify_walk <from> <count> <stride>
//
// Every number is read as hexadecimal. A line is passed before the next address is queried. An address that ends the
// process ends it with every line before it already passed, and the interface reads where the walk stopped.
#include "verba_scribo/verba_linea.h"
#include "../../../../../platform/platform.h"
#include "memoria_operor/memoria_linea.h"
#include "cellularum_laboro/cellularum_linea.h"
#include "../../../../../../includes/file_defs/qry/writer/qry_member.h"
#include "query_put.h"


int program_main(int count_of_words, char **words)
{
    if (count_of_words != 4)
    {
        return 2;
    }
    const unsigned long long from = mmgr_text_unsigned_in(words[1], NULL, 16);
    const unsigned long long count = mmgr_text_unsigned_in(words[2], NULL, 16);
    const unsigned long long stride = mmgr_text_unsigned_in(words[3], NULL, 16);
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
