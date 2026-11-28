// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// discover.c: the discover: the kernel read line by line, a form queried, the register bits, the seeds queried their
// relation, and the working set widened
#include "../../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "khw_learn_internal.h"
#include "memoria_operor/memoria_operor.h"

unsigned int s_kernel_probe_place;

// The kernel that the writer of the vendor reads from the container. The call gets it through the interface, and reads
// it where the writer appends it in the run (orior_khw_machine_write, kernel mode; kernel_blob.h). The call reads the
// run to its end before the writer runs. Thus the kernel that it reads is the one that the writer appends after.
//
// The call puts the places that the kernel holds into s_kernel_places, from the place where the least opening starts.
// It puts the places before that one into s_opening_places, and the registers that a thread uses into s_registers. It
// puts the encoding of each kept place into s_kernel_text. The place of the position among the kept places goes into
// s_kernel_probe_place, where the head names one from the opening on. Where the head names none, s_kernel_places goes
// there. The return is 1, or 0 where the run holds no kernel that this call reads
int khw_kernel_read(void)
{
    if (qry_member_run() == NULL)
    {
        return 0;
    }
    const unsigned long long before = qry_member_rings_settled();
    if (!khw_vendor_run("kernel", KERNEL_BLOB_NAME))
    {
        return 0;
    }
    qry_member_rings_settled();
    const unsigned char *kernel = NULL;
    unsigned long long kernel_size = 0ULL;
    unsigned long long place = 0ULL;
    KernelBlobHead head;
    if (!qry_member_blob(KERNEL_BLOB_NAME, &kernel, &kernel_size, &place) || (place < before) ||
        (kernel_size < sizeof(head)))
    {
        return 0;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &head, .src = kernel, .bytes = sizeof(head));
    if ((head.places == 0u) || (head.places > KHW_PLACES) || (head.registers == 0u) || (head.from >= head.places) ||
        ((kernel_size - sizeof(head)) < ((unsigned long long)head.places * KHW_INSTRUCTION)))
    {
        return 0;
    }
    const unsigned char *const code = kernel + sizeof(head);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = s_opening_text, .src = code, .bytes = (size_t)head.from * KHW_INSTRUCTION);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = s_kernel_text, .src = code + ((size_t)head.from * KHW_INSTRUCTION),
               .bytes = (size_t)(head.places - head.from) * KHW_INSTRUCTION);
    s_registers = head.registers;
    s_kernel_places = head.places - head.from;
    s_opening_places = head.from;
    s_kernel_probe_place =
        ((head.place != KERNEL_BLOB_PLACE_NONE) && (head.place >= head.from) && (head.place < head.places))
            ? (head.place - head.from)
            : s_kernel_places;
    return 1;
}

// the forms the part answered for, each with the relation it answered
KhwAnswered s_named[KHW_FORMS];

unsigned int s_names;

// 1 where `low` and `high` is already one of the forms the part answered for
int khw_named_already(unsigned long long low, unsigned long long high)
{
    for (unsigned int at = 0u; at < s_names; at += 1u)
    {
        if ((s_named[at].low == low) && (s_named[at].high == high))
        {
            return 1;
        }
    }
    return 0;
}

// The call queries the relation and the fields of `low` and `high`. Where the part answers a relation, the call records
// the encoding among the forms that the part answered for. It does so whether the table of the vendor names it or not.
// What the part answered belongs to the protocol, and only the file that the program writes at the end takes a name
// (P14). The return is 1 where the call recorded it
int khw_form_queried(unsigned long long low, unsigned long long high)
{
    if (khw_named_already(low, high) || (s_names == KHW_FORMS))
    {
        return 0;
    }
    KhwAnswered form;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &form, .bytes = sizeof(form), .val = 0);
    if (!khw_relation_queried(low, high, &form))
    {
        return 0;
    }
    khw_fields_queried(&form);
    s_named[s_names] = form;
    s_names += 1u;
    char relation[64];
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "    ");
        mmgr_line_put_columns(output_line, khw_relation_named(&form, 1, relation, sizeof(relation)), 14u);
        mmgr_line_put(output_line, " ");
        mmgr_line_hex(output_line, low, 16u);
        mmgr_line_put(output_line, " ");
        mmgr_line_hex(output_line, high, 16u);
        mmgr_line_put(output_line, "  ");
        mmgr_line_decimal(output_line, form.runs, 1u);
        mmgr_line_put(output_line, " runs, ");
        mmgr_line_decimal(output_line, form.survivors, 1u);
        mmgr_line_put(output_line, " surviving");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 1;
}

// the bits that the number of a register takes. They come from the last register that, as the part answers, the code
// of a question can name. The register walk stores it in the .ksc at `ksc` as `run answers <last> register last`
// (orior_klq_identity). The return is the bits of that number, or 0 where the part did not answer it
static unsigned int khw_register_bits(const char *ksc)
{
    static KscFile s_file;
    static KscRow s_row;
    unsigned int bits = 0u;
    ksc_read(&s_file, ksc);
    for (unsigned int line = 0u; line < s_file.lines; line += 1u)
    {
        if (ksc_row_read(s_file.line[line], "run", &s_row) &&
            (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = s_row.answer, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = "answers", .ci = EMBED_FALSE)) &&
            (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = s_row.question, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = "register last", .ci = EMBED_FALSE)))
        {
            for (bits = 0u; (bits < 64u) && ((s_row.value >> bits) != 0ULL); bits += 1u)
            {
            }
        }
    }
    ksc_free(&s_file);
    return bits;
}

// the max seeds a discover pass queries, the listing's and their variants together
#define KHW_SEEDS_MAX 4096u

// `low` and `high` added to the seeds `seed_low` and `seed_high`, `*count` of them, where neither the working set nor
// the seeds contain it already
static void khw_seed_added(unsigned long long *seed_low, unsigned long long *seed_high, unsigned int *count,
                           unsigned long long low, unsigned long long high)
{
    if (khw_named_already(low, high) || (*count == KHW_SEEDS_MAX))
    {
        return;
    }
    for (unsigned int at = 0u; at < *count; at += 1u)
    {
        if ((seed_low[at] == low) && (seed_high[at] == high))
        {
            return;
        }
    }
    seed_low[*count] = low;
    seed_high[*count] = high;
    *count += 1u;
}

// The forms of the round that answered a relation, carried into `related`. The round has the `count` questions of the
// pool in their order, each put at the position with the encoding that `lows` and `highs` give.
//
// The call reads a frame candidate, where `frame` marks it, for its frame relation, and every other candidate against
// the register relations. A candidate can answer none and leave the second word as it was on every case, as a form that
// writes a predicate does. Such a candidate goes again in a round of its own. In that round, P0 guards the store, and
// the call reads the candidate against the predicate relations. The return is the count related
static unsigned int khw_round_related(const unsigned long long *lows, const unsigned long long *highs, const int *frame,
                                      unsigned int count, KhwAnswered *related)
{
    static unsigned int s_again[SCHEDULER_ROUND_MAX];
    unsigned int found = 0u;
    unsigned int again = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        KhwAnswered form;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &form, .bytes = sizeof(form), .val = 0);
        form.low = lows[at];
        form.high = highs[at];
        const int framed = (frame != NULL) && (frame[at] != 0);
        if (framed ? khw_frame_of(&s_pool[at], &form) : khw_relation_of(&s_pool[at], &form, 0))
        {
            related[found] = form;
            found += 1u;
        }
        else if (!framed && khw_second_word_located(&s_pool[at]))
        {
            s_again[again] = at;
            again += 1u;
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  round: ");
        mmgr_line_decimal(output_line, found, 1u);
        mmgr_line_put(output_line, " answered a relation, ");
        mmgr_line_decimal(output_line, count - found, 1u);
        mmgr_line_put(output_line, " answered none, ");
        mmgr_line_decimal(output_line, again, 1u);
        mmgr_line_put(output_line, " of those left the second word located");
        mmgr_line_put(output_line, (again != 0u) ? " and are put again with the store guarded by P0" : "");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    if (again == 0u)
    {
        return found;
    }
    s_pooled = 0u;
    khw_store_guarded(1);
    for (unsigned int at = 0u; at < again; at += 1u)
    {
        khw_pooled(s_probe_place, lows[s_again[at]], highs[s_again[at]], s_union_word, s_union_cases);
    }
    khw_store_guarded(0);
    khw_round_carried();
    unsigned int predicates = 0u;
    for (unsigned int at = 0u; at < again; at += 1u)
    {
        KhwAnswered form;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &form, .bytes = sizeof(form), .val = 0);
        form.low = lows[s_again[at]];
        form.high = highs[s_again[at]];
        if (khw_relation_of(&s_pool[at], &form, 1))
        {
            related[found] = form;
            found += 1u;
            predicates += 1u;
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  round: ");
        mmgr_line_decimal(output_line, predicates, 1u);
        mmgr_line_put(output_line, " of the ");
        mmgr_line_decimal(output_line, again, 1u);
        mmgr_line_put(output_line, " put again answered a predicate relation");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return found;
}

// Every seed of the blob `name` of the run, with its relation queried from the top in the position. Each seed is a line
// `<low> <high>` in hex, as the reader of a listing of a vendor writes it (orior_sass_listing_seeds). A seed that
// answers a ladder relation enters the working set as a discovered form. Its fields stay for the turns and register
// passes, after a cross reads them. The call passes over a seed that answers none, or that is a form the part answered
// for.
//
// A seed goes with the own registers of the position, because the instruction of a listing names the registers of the
// compiler. The case words of the position lie in the registers of the kernel. The call writes into the seed the words
// that the own classified form of the kernel holds at its register fields, at the same fields. It sets each other
// register field in turn to the zero register that the part answered. The part reports whether the relation is true
// there.
//
// The seeds are the question that Q17 names, and the part answers it. No byte of the listing enters a form. The seeds
// go through the airlock in rounds, as any question does. The cross reads each, and the gate keeps its key unknown
// until a conference admits it (overseer.h). The return is the count that entered
unsigned int khw_seeds_queried(const char *name)
{
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ULL;
    if (!qry_member_blob(name, &bytes, &size, NULL))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  khw_write: the run contains no seeds ");
            mmgr_line_put(output_line, name);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0u;
    }
    // the kernel's own form, the position's encoding as classified, else the first classified form with operand runs
    const unsigned long long probe_low = khw_word_read(&s_kernel_text[s_probe_place * KHW_INSTRUCTION], 0u);
    const unsigned long long probe_high = khw_word_read(&s_kernel_text[s_probe_place * KHW_INSTRUCTION], 8u);
    // the register fields of the layout of the instruction word. Each field is as wide as the last register that, as
    // the part answers, the code can name. That is the answer of the walk in the .ksc (`register last`). Where the part
    // did not answer it, there are none, and the call uses the operand runs of the kernel. The turns found those runs
    // over two registers, and they reach only the bits that told the two apart
    static ContainerLayout s_layout;
    const unsigned int register_bits = khw_register_bits(s_ksc_path);
    const int fields_read = (register_bits != 0u) && container_layout_read(&s_layout, s_fields);
    const unsigned int zero = khw_fixed_register("zero");
    const KhwAnswered *kernel = NULL;
    for (unsigned int at = 0u; at < s_names; at += 1u)
    {
        const KhwAnswered *const form = &s_named[at];
        if ((form->phase != KHW_PHASE_CLASSIFIED) || (form->runs == 0u))
        {
            continue;
        }
        if ((kernel == NULL) || ((form->low == probe_low) && (form->high == probe_high)))
        {
            kernel = form;
        }
    }
    if (kernel == NULL)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(
                output_line,
                "  khw_write: no classified form contains the probe place's registers, and no seed is queried");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0u;
    }
    // The seeds to query. Each seed of the listing goes in with the registers of the position in the register fields
    // that the form of the kernel covers. For each register field that the form does not cover, a copy goes in. The
    // copy has that field set to the zero register that the part answered (fixed zero, Open 6.2). Thus a source that
    // the form of the position does not hold adds nothing.
    //
    // The part decides which of these is a form. Where such a field holds no register, the reader of the vendor reads
    // the seed before the part
    static unsigned long long s_seed_low[KHW_SEEDS_MAX];
    static unsigned long long s_seed_high[KHW_SEEDS_MAX];
    unsigned int seeds = 0u;
    char line[256];
    unsigned long long next = 0ULL;
    while ((seeds < KHW_SEEDS_MAX) && line_read(bytes, size, &next, line, sizeof(line)))
    {
        unsigned long long low = 0ULL;
        unsigned long long high = 0ULL;
        const char *at = line;
        if (!line_number(&at, 1, &low) || !line_number(&at, 1, &high))
        {
            continue;
        }
        unsigned int open_first[KHW_RUNS];
        unsigned int open_last[KHW_RUNS];
        unsigned int opens = 0u;
        for (unsigned int at = 0u; fields_read && (at < s_layout.rows); at += 1u)
        {
            const LayoutRow *const row = &s_layout.row[at];
            if ((row->type != (unsigned int)LAYOUT_FIELD) || (row->width != register_bits))
            {
                continue;
            }
            // a field's first bit is below 128 and its width at max that: both fit an unsigned int
            const unsigned int first = (unsigned int)row->offset;
            const unsigned int last = first + row->width - 1u;
            // a register field is one an operand run of the kernel's form lies within
            int operand = 0;
            for (unsigned int run = 0u; run < kernel->runs; run += 1u)
            {
                operand = operand || ((kernel->place[run] != KHW_PLACE_NONE) && (kernel->first[run] >= first) &&
                                      (kernel->last[run] <= last));
            }
            if (operand)
            {
                khw_run_write(&low, &high, first, last, khw_run_read(probe_low, probe_high, first, last));
            }
            else if (opens < KHW_RUNS)
            {
                open_first[opens] = first;
                open_last[opens] = last;
                opens += 1u;
            }
        }
        for (unsigned int run = 0u; !fields_read && (run < kernel->runs); run += 1u)
        {
            if (kernel->place[run] != KHW_PLACE_NONE)
            {
                khw_run_write(&low, &high, kernel->first[run], kernel->last[run],
                              khw_run_read(kernel->low, kernel->high, kernel->first[run], kernel->last[run]));
            }
        }
        khw_seed_added(s_seed_low, s_seed_high, &seeds, low, high);
        for (unsigned int at = 0u; (zero != KHW_FIXED_NONE) && (at < opens); at += 1u)
        {
            unsigned long long zeroed_low = low;
            unsigned long long zeroed_high = high;
            khw_run_write(&zeroed_low, &zeroed_high, open_first[at], open_last[at], zero);
            khw_seed_added(s_seed_low, s_seed_high, &seeds, zeroed_low, zeroed_high);
        }
    }
    // the seeds queried in rounds, as many to a round as the pool holds, over the cases of the union at the position.
    // The call reads each for its relation, and one that answers a relation enters the working set as discovered
    khw_union_written();
    unsigned int entered = 0u;
    khw_plan_passed("seed", (unsigned long long)seeds);
    for (unsigned int first = 0u; first < seeds; first += SCHEDULER_ROUND_MAX)
    {
        const unsigned int last = ((first + SCHEDULER_ROUND_MAX) < seeds) ? (first + SCHEDULER_ROUND_MAX) : seeds;
        s_pooled = 0u;
        for (unsigned int at = first; at < last; at += 1u)
        {
            khw_pooled(s_probe_place, s_seed_low[at], s_seed_high[at], s_union_word, s_union_cases);
        }
        khw_round_carried();
        static KhwAnswered s_seed_related[SCHEDULER_ROUND_MAX];
        const unsigned int related =
            khw_round_related(s_seed_low + first, s_seed_high + first, NULL, last - first, s_seed_related);
        for (unsigned int at = 0u; (at < related) && (s_names < KHW_FORMS); at += 1u)
        {
            KhwAnswered form = s_seed_related[at];
            // a seed the set's form answers for already, a turn of it, is that form, and enters nowhere
            if (khw_form_was_for(s_named, s_names, &form))
            {
                continue;
            }
            form.phase = KHW_PHASE_DISCOVERED;
            s_named[s_names] = form;
            s_names += 1u;
            entered += 1u;
            char relation[64];
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "    seed ");
                mmgr_line_put_columns(output_line, khw_relation_named(&form, 1, relation, sizeof(relation)), 14u);
                mmgr_line_put(output_line, " ");
                mmgr_line_hex(output_line, form.low, 16u);
                mmgr_line_put(output_line, " ");
                mmgr_line_hex(output_line, form.high, 16u);
                mmgr_line_put(output_line, "  discovered, its turns retained for a cross-check");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
        }
        s_pooled = 0u;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  seeds: ");
        mmgr_line_decimal(output_line, seeds, 1u);
        mmgr_line_put(output_line, " queried their relation, ");
        mmgr_line_decimal(output_line, entered, 1u);
        mmgr_line_put(output_line, " answered one and entered the working set");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return entered;
}

// Every form that the part answered for, widened. The call turns each bit outside the runs of its operands, its key and
// the word of the scheduler. It queries the relation of the turned encoding from the top. The run of an operand is one
// with a place in the tuple of the relation. A run that carries no word of the tuple is no operand of the relation, and
// the call turns its bits as any other bits.
//
// A pass turns at max KHW_FORMS encodings, those of the newest form first. A form that the set gained last is the
// frontier of the widen step. The turns of an older form, which R already answers, fill the rest of the pass. The
// relation of each turned encoding waits on no other answer, and the call puts them SCHEDULER_ROUND_MAX to a round.
//
// A turned encoding that answers as the form that it came from is that form (khw_form_is_for), and enters the set
// nowhere. Only one that answers another relation is a form of its own, such as an add with its negate bits turned.
// Where `discover` is 0, the call then queries the fields of each one that answered a relation. Where it is not 0, the
// call keeps it with its fields not queried. A cross-check can then read its turns before a later pass queries them
// from the part. The part could hang on an encoding that the vendor calls illegal.
//
// Only a classified form is a source of a widen step. A form still short of its operand runs would widen on bits that
// its register-runs did not yet sort from the operands. The return is the count found this round
unsigned int khw_widened(unsigned int from, int discover)
{
    const unsigned int until = s_names;
    unsigned int found = 0u;
    // every turned encoding that has no answer yet, each once, and whether it came from a frame form. The call puts the
    // encodings of a frame form whole over the cases of the frame, and reads them against the relations of the frame
    static unsigned long long s_turned_low[KHW_FORMS];
    static unsigned long long s_turned_high[KHW_FORMS];
    static int s_turned_frame[KHW_FORMS];
    unsigned int turned = 0u;
    for (unsigned int at = until; at > from; at -= 1u)
    {
        const KhwAnswered form = s_named[at - 1u];
        const int framed = (form.frame != (unsigned int)FRAME_RELATION_COUNT);
        if ((discover != 0) && (form.phase != KHW_PHASE_CLASSIFIED))
        {
            continue;
        }
        for (unsigned int bit = KHW_TURN_FIRST; (bit <= KHW_TURN_LAST) && (turned < KHW_FORMS); bit += 1u)
        {
            int inside = 0;
            for (unsigned int run = 0u; run < form.runs; run += 1u)
            {
                inside = inside ||
                         ((form.place[run] != KHW_PLACE_NONE) && (bit >= form.first[run]) && (bit <= form.last[run]));
            }
            unsigned long long low = form.low;
            unsigned long long high = form.high;
            khw_run_write(&low, &high, bit, bit, khw_run_read(low, high, bit, bit) ^ 1ULL);
            int seen = inside || khw_named_already(low, high);
            for (unsigned int other = 0u; (other < turned) && !seen; other += 1u)
            {
                seen = (s_turned_low[other] == low) && (s_turned_high[other] == high);
            }
            if (!seen)
            {
                s_turned_low[turned] = low;
                s_turned_high[turned] = high;
                s_turned_frame[turned] = framed;
                turned += 1u;
            }
        }
    }
    khw_union_written();
    unsigned int frame_cases = 0u;
    const unsigned int (*const frame_word)[2] = khw_frame_cases(&frame_cases);
    static KhwAnswered s_related[SCHEDULER_ROUND_MAX];
    for (unsigned int first = 0u; first < turned; first += SCHEDULER_ROUND_MAX)
    {
        const unsigned int last = ((first + SCHEDULER_ROUND_MAX) < turned) ? (first + SCHEDULER_ROUND_MAX) : turned;
        s_pooled = 0u;
        for (unsigned int at = first; at < last; at += 1u)
        {
            if (s_turned_frame[at] != 0)
            {
                khw_frame_pooled(s_turned_low[at], s_turned_high[at], frame_word, frame_cases);
            }
            else
            {
                khw_pooled(s_probe_place, s_turned_low[at], s_turned_high[at], s_union_word, s_union_cases);
            }
        }
        khw_round_carried();
        const unsigned int related = khw_round_related(s_turned_low + first, s_turned_high + first,
                                                       s_turned_frame + first, last - first, s_related);
        s_pooled = 0u;
        for (unsigned int at = 0u; (at < related) && (s_names < KHW_FORMS); at += 1u)
        {
            KhwAnswered form = s_related[at];
            // a turn the set's form answers for already is that form, and enters nowhere
            if (khw_form_was_for(s_named, s_names, &form))
            {
                continue;
            }
            if (discover == 0)
            {
                khw_fields_queried(&form);
            }
            s_named[s_names] = form;
            s_names += 1u;
            found += 1u;
            char relation[64];
            khw_relation_named(&form, 1, relation, sizeof(relation));
            if (discover != 0)
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "    ");
                mmgr_line_put_columns(output_line, relation, 14u);
                mmgr_line_put(output_line, " ");
                mmgr_line_hex(output_line, form.low, 16u);
                mmgr_line_put(output_line, " ");
                mmgr_line_hex(output_line, form.high, 16u);
                mmgr_line_put(output_line, "  discovered, its turns retained for a cross-check");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            else
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "    ");
                mmgr_line_put_columns(output_line, relation, 14u);
                mmgr_line_put(output_line, " ");
                mmgr_line_hex(output_line, form.low, 16u);
                mmgr_line_put(output_line, " ");
                mmgr_line_hex(output_line, form.high, 16u);
                mmgr_line_put(output_line, "  ");
                mmgr_line_decimal(output_line, form.runs, 1u);
                mmgr_line_put(output_line, " runs, ");
                mmgr_line_decimal(output_line, form.survivors, 1u);
                mmgr_line_put(output_line, " surviving");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
        }
    }
    return found;
}
