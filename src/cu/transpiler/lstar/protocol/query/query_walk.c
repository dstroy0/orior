// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_walk.c: the program a walk of queries runs as, inside the cell
//
//     query_walk <qualifier> <from> <count> <stride> <turns> <word>
//
// Every number is read as hexadecimal. The program puts one query at each address from `from`, `stride` bytes apart,
// `count` of them, and passes the run of the process that started it one line an address as it is answered, the record
// QUERY_WALK_RECORD:
//
//     <address> <bit> <type>
//
// A line is passed before the next address is queried. An address that ends the process ends it with every answer
// before it already passed, and the cell reads where the walk stopped from the last line. `turns` is how
// many reads ADVANCES puts at an address after its first before the address reads as not advancing: a counter that
// steps once a clock interrupt reads as still between two reads far more often than not.
#include "../../../../../../includes/file_defs/qry/writer/qry_member.h"
#include "../../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "query_put.h"
#include "verba_scribo/verba_linea.h"

int program_main(int count_of_words, char **words)
{
    if (count_of_words != 7)
    {
        return 2;
    }
    // each is read as hexadecimal and stored at the width the query carries
    const unsigned int qualifier =
        (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = words[1], .end = NULL, .base = 16);
    const unsigned long long from =
        EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = words[2], .end = NULL, .base = 16);
    const unsigned long long count =
        EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = words[3], .end = NULL, .base = 16);
    const unsigned long long stride =
        EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = words[4], .end = NULL, .base = 16);
    const unsigned long long turns =
        EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = words[5], .end = NULL, .base = 16);
    const unsigned int word =
        (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = words[6], .end = NULL, .base = 16);
    for (unsigned long long number = 0ULL; number < count; number += 1ULL)
    {
        Query query = {0};
        query.address = from + (number * stride);
        query.qualifier = qualifier;
        query.word = word;
        query.turns = turns;
        query_put(&query);
        // an address in hex and two numbers, each in the room of a 64-bit word's digits
        char text[64];
        VerbaLinea line = {text, sizeof(text), 0ULL};
        mmgr_line_hex(&line, query.address, 0u);
        mmgr_line_ch(&line, ' ');
        mmgr_line_decimal(&line, query.bit, 0u);
        mmgr_line_ch(&line, ' ');
        mmgr_line_decimal(&line, query.type, 0u);
        mmgr_line_ch(&line, '\n');
        if (qry_member_append(QUERY_WALK_RECORD, text, line.at, QRY_ORDINARY) == 0ULL)
        {
            return 3;
        }
    }
    return 0;
}
