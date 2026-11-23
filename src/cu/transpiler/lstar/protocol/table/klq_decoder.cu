// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// klq_decoder.cu: reads the log orior_klq_identity writes into the query's .qry beside its trace. It writes the set of
// our coherence it reads each pair of the bridge into, beneath its verdict in Lstar.klq (P13, the qualifiers and the
// modifiers)
//
//   `orior_klq_decoder <query .qry> <carrier folder> <Lstar.klq>`
//
// The decoder reads a pair where its two forms are one text but for their modifiers. The first word of each link's
// text is the same up to its first `.`. It reads each put of one form in the other's place that the part answered off
// the cases it answered apart on. It reads the carrier the put went in as well. A case word decides the cases apart
// where two cases apart in that word alone answer one alike and the other apart. The decoder follows each operand of
// the link back through the carrier to the case words it reads (carrier_flow.h).
//
// A pair is a qualifier's where it is apart and a case word decides it that a flag reads. Only the form it was names
// the flag, and no other operand of the link reads the word. A pair whose forms name the same operands, apart where a
// case word the link reads decides it, is a modifier's. It is a statement's where it is alike on every case whose
// values every statement reads as themselves. Those are the values as the part stores them at the link. A pair of
// forms of a flag that name the same operands, apart whatever the link reads, is a negation's.
//
// The two are the members of one node. A pair answered alike at every put is a qualifier's. The decoder writes a pair
// a member of every set a test reads it into, and of every meta, each a light of its own. It writes unknown_coherence
// for a pair the part answered apart in this log that no test reads into a set. A pair the log contains no answer of
// retains the set line the bridge recorded beneath it. One that contains none gets unknown_coherence: no answer read
// it into a set, and it is a member of every one.
//
// The texts the log read of a pair read it into its categories before any query, each written beside its sets. The
// categories are a comparison, an equality or an order, an operation, a commutation, a verb, a range, a vector, a
// control or a switch. A text reads no pair out of unknown_coherence
//
// The decoder writes each answered pair whose product is whole beneath its set with its concept, `concept_coherence
// <identity>`. That is whether its two forms write one value or two on each case, as a function of the values the form
// reads there. The product goes over every put the part answered (concept_product.h). A concept is its product and
// cares for nothing that made it, and two pairs of one product are one concept whatever forms name them
#include "../../../../../platform/platform.h"
#include "decoder/decoder_internal.h"

int program_main(int argc, char **argv)
{
    if (argc != 4)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "orior_klq_decoder <query .qry> <carrier folder> <Lstar.klq>");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
        return 1;
    }
    static Decoding s_decoding;
    Decoding *const decoding = &s_decoding;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = decoding, .bytes = sizeof(*decoding), .val = 0);
    int code = decoding->log_read(argv[1], argv[3]);
    if (code == 0)
    {
        decoding->products_decoded(argv[2]);
        decoding->concepts_folded();
        decoding->bridge_folded();
        code = decoding->bridge_passed(argv[3]);
    }
    if (code == 0)
    {
        decoding->printed(argv[3]);
    }
    if (decoding->texts_begun)
    {
        platform_text_ended(&decoding->concepts);
        platform_text_ended(&decoding->written);
    }
    decoding->ended();
    return code;
}
