// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// classify_walk.c: the program a classify walk runs as, inside a probe the interface can lose. It puts the kind ask
// at each address of a span and hands the run of the process that started it one line an address as it is answered,
// the record QUERY_WALK_RECORD:
//
//     classify_walk <from> <count> <stride>
//
// Every number is read as hexadecimal. A line is handed before the next address is asked. An address that ends the
// process ends it with every line before it already handed, and the interface reads where the walk stopped.
#include "query_ask.h"
#include "../../../../includes/file_defs/qry/writer/qry_member.h"
#include "../../../../engine/runtime/scriptura/scriptura.h"

#include <stdlib.h>

int main(int count_of_words, char **words)
{
    if (count_of_words != 4)
    {
        return 2;
    }
    const unsigned long long from = strtoull(words[1], NULL, 16);
    const unsigned long long count = strtoull(words[2], NULL, 16);
    const unsigned long long stride = strtoull(words[3], NULL, 16);
    for (unsigned long long number = 0ull; number < count; number += 1ull)
    {
        const unsigned long long at = from + (number * stride);
        unsigned int read_back = 0u;
        const unsigned int kind = host_address_ask(at, &read_back);
        // an address and two words in hex, each in the room of a 64-bit word's digits
        char text[64];
        ScripturaLine line = {text, sizeof(text), 0ull};
        scriptura_hex(&line, at, 0u);
        scriptura_character(&line, ' ');
        scriptura_hex(&line, kind, 0u);
        scriptura_character(&line, ' ');
        scriptura_hex(&line, read_back, 0u);
        scriptura_character(&line, '\n');
        if (qry_member_append(QUERY_WALK_RECORD, text, line.at, QRY_ORDINARY) == 0ull)
        {
            return 3;
        }
    }
    return 0;
}
