// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// klq_decoder.cu: reads the log klq_identity writes into the query's .qry beside its trace, and writes the set of our
// coherence each pair of the bridge is read into beneath its verdict in Lstar.klq (P13, the qualifiers and the
// modifiers)
//
//   klq_decoder <query .qry> <carrier folder> <Lstar.klq>
//
// A pair is read where its two forms are one text but for their modifiers, the first word of each link's text the same
// up to its first `.`. Each put of one form in the other's place that the part answered is read off the cases it
// answered apart on and the carrier it was put in. A case word decides the cases apart where two cases apart in that
// word alone answer one alike and the other apart. Each operand of the link is followed back through the carrier to
// the case words it reads (carrier_flow.h). A pair apart where a case word decides it that a flag only the form it
// stood as names reads, and no other operand of the link, is a qualifier's. A pair whose forms name the same operands,
// apart where a case word the link reads decides it, is a modifier's, and a statement's where it is alike on every case
// whose values, as the part holds them at the link, every statement reads as themselves. A pair of forms of a flag that
// name the same operands, apart whatever the link reads, is a negation's: the two members of one node. A pair answered
// alike at every put is a qualifier's. A pair is written a member of every set a test reads it into, and of every meta,
// each a light of its own, and a pair the part answered apart in this log that no test reads into a set is written
// unknown_coherence. A pair the log holds no answer of keeps the set line the bridge held beneath it, and one that
// holds none is written unknown_coherence: no answer has read it into a set, and it is a member of every one. The
// texts the log read of a pair read it into its categories before any ask, a comparison, an equality or an order, an
// operation, a commutation, a verb, a range, a vector, a control or a switch, each written beside its sets, and a text
// reads no pair out of
// unknown_coherence
//
// Each answered pair whose product is whole is written beneath its set with its concept, `concept_coherence
// <identity>`: whether its two forms write one value or two on each case, as a function of the values the form reads
// there, over every put the part answered (concept_product.h). A concept is its product and cares for nothing that made
// it, and two pairs of one product are one concept whatever forms name them
#include "decoder/decoder_internal.h"

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        printf("klq_decoder <query .qry> <carrier folder> <Lstar.klq>\n");
        return 1;
    }
    Decoding decoding;
    if (decoding.log_read(argv[1]) != 0)
    {
        return 1;
    }
    decoding.products_decoded(argv[2]);
    if (decoding.bridge_read(argv[3]) != 0)
    {
        return 1;
    }
    decoding.bridge_folded();
    if (decoding.bridge_handed(argv[3]) != 0)
    {
        return 1;
    }
    decoding.printed(argv[3]);
    return 0;
}
