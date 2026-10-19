// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_walk.c: the program a walk of asks runs as, inside the cell
//
//     query_walk <qualifier> <from> <count> <stride> <turns> <word>
//
// Every number is read as hexadecimal. The program puts one ask at each address from `from`, `stride` bytes apart,
// `count` of them, and hands the run of the process that started it one line an address as it is answered, the record
// QUERY_WALK_RECORD:
//
//     <address> <bit> <kind>
//
// A line is handed before the next address is asked. An address that ends the process ends it with every answer
// before it already handed, and the cell reads where the walk stopped from the last line. `turns` is how
// many reads ADVANCES puts at an address after its first before the address reads as not advancing: a counter that
// steps once a clock interrupt reads as still between two reads far more often than not.
#include "query_ask.h"
#include "../../../../includes/file_defs/qry/writer/qry_member.h"
#include "../../../../engine/runtime/scriptura/scriptura.h"

#include <stdlib.h>

int main(int count_of_words, char **words)
{
    if (count_of_words != 7)
    {
        return 2;
    }
    // each is read as hexadecimal and kept at the width the ask carries
    const unsigned int qualifier = (unsigned int)strtoul(words[1], NULL, 16);
    const unsigned long long from = strtoull(words[2], NULL, 16);
    const unsigned long long count = strtoull(words[3], NULL, 16);
    const unsigned long long stride = strtoull(words[4], NULL, 16);
    const unsigned long long turns = strtoull(words[5], NULL, 16);
    const unsigned int word = (unsigned int)strtoul(words[6], NULL, 16);
    for (unsigned long long number = 0ull; number < count; number += 1ull)
    {
        QueryAsk asked = {0};
        asked.address = from + (number * stride);
        asked.qualifier = qualifier;
        asked.word = word;
        asked.turns = turns;
        query_ask(&asked);
        // an address in hex and two numbers, each in the room of a 64-bit word's digits
        char text[64];
        ScripturaLine line = {text, sizeof(text), 0ull};
        scriptura_hex(&line, asked.address, 0u);
        scriptura_character(&line, ' ');
        scriptura_decimal(&line, asked.bit, 0u);
        scriptura_character(&line, ' ');
        scriptura_decimal(&line, asked.kind, 0u);
        scriptura_character(&line, '\n');
        if (qry_member_append(QUERY_WALK_RECORD, text, line.at, QRY_ORDINARY) == 0ull)
        {
            return 3;
        }
    }
    return 0;
}
