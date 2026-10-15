// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// discover.c: the discover: the kernel read line by line, a form asked, the register bits, the seeds asked their
// relation, and the working set widened
#include "khw_write_internal.h"

// the line at `*at` of the `size` bytes at `bytes` read into `line`, which holds `room`, as much of it as fits, and
// `*at` moved past it: 1, or 0 where no line is left
int khw_line_read(const unsigned char *bytes, unsigned long long size, unsigned long long *at, char *line, size_t room)
{
    if (*at >= size)
    {
        return 0;
    }
    const unsigned char *const end = (const unsigned char *)memchr(bytes + *at, '\n', (size_t)(size - *at));
    const size_t length = (end != NULL) ? (size_t)(end - (bytes + *at)) : (size_t)(size - *at);
    snprintf(line, room, "%.*s", (int)length, (const char *)(bytes + *at));
    *at += (unsigned long long)length + 1ull;
    return 1;
}

unsigned int s_kernel_slot;

// the kernel the vendor's writer reads out of the container, got through the interface and read back from the blob
// it hands the run's buffer (khw_machine_write, kernel mode), the one handed after any before it of the name: the
// places it holds from the one the least opening begins at into s_kernel_places, the part's answer the writer names
// after the places and the registers (The frame asked, Open 6.3), the registers a thread holds into s_registers, each
// kept place's encoding into s_kernel_text, and the slot's place among the kept ones into s_kernel_slot where the
// writer names it after the opening's, s_kernel_places where it names none. 1, or 0 with the reason printed
int khw_kernel_read(void)
{
    QryBuffer *const run = qry_run();
    if (run == NULL)
    {
        printf("  khw_write: no query writer runs: %s names no .qry a writer holds open\n", QRY_ENVIRONMENT);
        return 0;
    }
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    unsigned long long before = 0ull;
    if (!qry_latest(run, KHW_KERNEL_NAME, &bytes, &size, &before))
    {
        before = 0ull;
    }
    unsigned long long sequence = 0ull;
    if (!khw_vendor_run("kernel", KHW_KERNEL_NAME))
    {
        return 0;
    }
    if (!qry_latest(run, KHW_KERNEL_NAME, &bytes, &size, &sequence) || (sequence <= before))
    {
        printf("  khw_write: the vendor's writer handed the run's buffer no %s\n", KHW_KERNEL_NAME);
        return 0;
    }
    char line[256];
    unsigned long long at = 0ull;
    unsigned int places = 0u;
    unsigned int from = 0u;
    char slot_text[16] = "-";
    if (!khw_line_read(bytes, size, &at, line, sizeof(line)) ||
        (sscanf(line, "kernel %u %u %u %15s", &places, &s_registers, &from, slot_text) < 2) || (places == 0u) ||
        (places > KHW_PLACES) || (s_registers == 0u) || (from >= places))
    {
        printf("  khw_write: %s holds no kernel this reads\n", KHW_KERNEL_NAME);
        return 0;
    }
    // the slot's place where the listing names one from the opening on, counted among the kept places; none otherwise
    char *end = NULL;
    const unsigned long long slot = strtoull(slot_text, &end, 10);
    const int named = (end != NULL) && (end != slot_text) && (*end == '\0') && (slot >= from) && (slot < places);
    s_kernel_slot = named ? (unsigned int)(slot - from) : (places - from);
    // every place is read, and those from the one the least opening begins at are kept
    unsigned int kept = 0u;
    for (unsigned int place = 0u; place < places; place += 1u)
    {
        unsigned long long low = 0ull;
        unsigned long long high = 0ull;
        if (!khw_line_read(bytes, size, &at, line, sizeof(line)) || (sscanf(line, "%llx %llx", &low, &high) != 2))
        {
            printf("  khw_write: %s holds %u places, fewer than its header says\n", KHW_KERNEL_NAME, place);
            return 0;
        }
        if (place < from)
        {
            khw_word_write(&s_opening_text[place * KHW_INSTRUCTION], 0u, low);
            khw_word_write(&s_opening_text[place * KHW_INSTRUCTION], 8u, high);
            continue;
        }
        khw_word_write(&s_kernel_text[kept * KHW_INSTRUCTION], 0u, low);
        khw_word_write(&s_kernel_text[kept * KHW_INSTRUCTION], 8u, high);
        kept += 1u;
    }
    s_kernel_places = kept;
    s_opening_places = from;
    return 1;
}

// the forms the part has answered for, each with the relation it answered
KhwAnswered s_named[KHW_FORMS];

unsigned int s_names;

// 1 where `low` and `high` is already one of the forms the part has answered for
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

// `low` and `high` asked its relation and its fields, and kept among the forms the part has answered for where it
// answered one, whether the vendor's table names it: what the part answered is the protocol's, and only the
// file written at the end takes a name (P14). 1 where it was kept
int khw_form_asked(unsigned long long low, unsigned long long high)
{
    if (khw_named_already(low, high) || (s_names == KHW_FORMS))
    {
        return 0;
    }
    KhwAnswered held;
    memset(&held, 0, sizeof(held));
    if (!khw_relation_asked(low, high, &held))
    {
        return 0;
    }
    khw_fields_asked(&held);
    s_named[s_names] = held;
    s_names += 1u;
    char relation[64];
    printf("    %-14s %016llx %016llx  %u runs, %u surviving\n",
           khw_relation_named(&held, 1, relation, sizeof(relation)), low, high, held.runs, held.survivors);
    return 1;
}

// the bits a register's number takes, from the last register the part answers a question's code can name, kept in
// the .ksc at `ksc` by the register walk as `run answers <last> register last` (klq_identity): the bits of that
// number; 0 where the part has not answered it
static unsigned int khw_register_bits(const char *ksc)
{
    static KscFile s_file;
    static KscRow s_row;
    unsigned int bits = 0u;
    ksc_read(&s_file, ksc);
    for (unsigned int line = 0u; line < s_file.lines; line += 1u)
    {
        if (ksc_row_read(s_file.line[line], "run", &s_row) && (strcmp(s_row.answer, "answers") == 0) &&
            (strncmp(s_row.question, "register last", 13u) == 0))
        {
            for (bits = 0u; (bits < 64u) && ((s_row.value >> bits) != 0ull); bits += 1u)
            {
            }
        }
    }
    ksc_free(&s_file);
    return bits;
}

// the most seeds a discover pass asks, the listing's and their variants together
#define KHW_SEEDS_MOST 4096u

// `low` and `high` added to the seeds `seed_low` and `seed_high`, `*count` of them, where neither the working set nor
// the seeds hold it already
static void khw_seed_added(unsigned long long *seed_low, unsigned long long *seed_high, unsigned int *count,
                           unsigned long long low, unsigned long long high)
{
    if (khw_named_already(low, high) || (*count == KHW_SEEDS_MOST))
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

// Every seed of the run's blob `name` asked its relation from the top in the slot, each a line `<low> <high>` in hex
// as a vendor's reader of a listing writes it (sass_listing_seeds): one that answers a ladder relation enters the
// working set as a discovered form, its fields left for the turns and register passes once a cross has read them, and
// one that answers none, or is a form the part has answered for already, is passed over. A seed is put with the
// slot's own registers: a listing's instruction names the compiler's registers, and the slot's case words lie in the
// kernel's. The words the kernel's own classified form holds at its register fields are written into the seed at the
// same fields, every other register field is set in turn to the zero register the part answered, and the part says
// whether the relation holds there. The seeds are the question Q17 names and the part answers; no byte of the listing
// enters a form. They go through the airlock in rounds as any question: the cross reads each, and the gate holds its
// key unknown until a conference admits it (overseer.h). The count that entered
unsigned int khw_seeds_asked(const char *name)
{
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    if (!qry_latest(qry_run(), name, &bytes, &size, NULL))
    {
        printf("  khw_write: the run holds no seeds %s\n", name);
        return 0u;
    }
    // the kernel's own form, the slot's encoding as classified, else the first classified form with operand runs
    const unsigned long long slot_low = khw_word_read(&s_kernel_text[s_slot * KHW_INSTRUCTION], 0u);
    const unsigned long long slot_high = khw_word_read(&s_kernel_text[s_slot * KHW_INSTRUCTION], 8u);
    // the register fields of the instruction word's layout: every field as wide as the last register the part
    // answers the code can name, the walk's answer in the .ksc (`register last`); none where the part has not answered
    // it, and the kernel's operand runs stand in, which the turns found over two registers and reach only the bits
    // that told them apart
    static ContainerLayout s_layout;
    const unsigned int register_bits = khw_register_bits(s_ksc_path);
    const int fields_read = (register_bits != 0u) && container_layout_read(&s_layout, s_fields);
    const unsigned int zero = khw_fixed_register("zero");
    const KhwAnswered *kernel = NULL;
    for (unsigned int at = 0u; at < s_names; at += 1u)
    {
        const KhwAnswered *const held = &s_named[at];
        if ((held->phase != KHW_PHASE_CLASSIFIED) || (held->runs == 0u))
        {
            continue;
        }
        if ((kernel == NULL) || ((held->low == slot_low) && (held->high == slot_high)))
        {
            kernel = held;
        }
    }
    if (kernel == NULL)
    {
        printf("  khw_write: no classified form holds the slot's registers, and no seed is asked\n");
        return 0u;
    }
    // The seeds asked: each of the listing's as it stands, the slot's registers in the register fields the kernel's
    // form covers, and for each register field it does not cover the same with that field set to the zero register
    // the part answered (fixed zero, Open 6.2), so that a source the slot's form does not hold adds nothing; which of
    // these is a form the part decides. A field holding no register, set so, the vendor's reader holds before the part
    static unsigned long long s_seed_low[KHW_SEEDS_MOST];
    static unsigned long long s_seed_high[KHW_SEEDS_MOST];
    unsigned int seeds = 0u;
    char line[256];
    unsigned long long next = 0ull;
    while ((seeds < KHW_SEEDS_MOST) && khw_line_read(bytes, size, &next, line, sizeof(line)))
    {
        unsigned long long low = 0ull;
        unsigned long long high = 0ull;
        if (sscanf(line, "%llx %llx", &low, &high) != 2)
        {
            continue;
        }
        unsigned int open_first[KHW_RUNS];
        unsigned int open_last[KHW_RUNS];
        unsigned int opens = 0u;
        for (unsigned int at = 0u; fields_read && (at < s_layout.rows); at += 1u)
        {
            const LayoutRow *const row = &s_layout.row[at];
            if ((row->kind != (unsigned int)LAYOUT_FIELD) || (row->width != register_bits))
            {
                continue;
            }
            // a field's first bit is below 128 and its width at most that: both fit an unsigned int
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
                khw_run_write(&low, &high, first, last, khw_run_read(slot_low, slot_high, first, last));
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
    // the seeds asked in rounds, as many a round as the pool holds, over the union's cases at the slot, and each read
    // for its relation; one that answers a relation enters the working set discovered
    khw_union_written();
    unsigned int entered = 0u;
    khw_plan_handed("seed", (unsigned long long)seeds);
    for (unsigned int first = 0u; first < seeds; first += SCHEDULER_ROUND_MOST)
    {
        const unsigned int last = ((first + SCHEDULER_ROUND_MOST) < seeds) ? (first + SCHEDULER_ROUND_MOST) : seeds;
        s_pooled = 0u;
        for (unsigned int at = first; at < last; at += 1u)
        {
            khw_pooled(s_slot, s_seed_low[at], s_seed_high[at], s_union_word, s_union_cases);
        }
        khw_round_carried();
        for (unsigned int at = first; (at < last) && (s_names < KHW_FORMS); at += 1u)
        {
            KhwAnswered held;
            memset(&held, 0, sizeof(held));
            held.low = s_seed_low[at];
            held.high = s_seed_high[at];
            // a seed the set's form answers for already, a turn of it, is that form, and enters nowhere
            if (!khw_relation_of(&s_pool[at - first], &held) || khw_form_stood_for(s_named, s_names, &held))
            {
                continue;
            }
            held.phase = KHW_PHASE_DISCOVERED;
            s_named[s_names] = held;
            s_names += 1u;
            entered += 1u;
            char relation[64];
            printf("    seed %-14s %016llx %016llx  discovered, its turns held for a cross-check\n",
                   khw_relation_named(&held, 1, relation, sizeof(relation)), held.low, held.high);
        }
        s_pooled = 0u;
    }
    printf("  seeds: %u asked their relation, %u answered one and entered the working set\n", seeds, entered);
    return entered;
}

// Every form the part answered for widened: each bit outside its operands' runs, its key and the scheduler's word
// turned, and the turned encoding asked its relation from the top. An operand's run is one with a place in the
// relation's tuple; a run that carries no word of the tuple is no operand of the relation, and its bits are turned as
// any other's. A pass turns at most KHW_FORMS encodings, the newest form's first: a form the set gained last is the
// frontier of the widening, and an older form's turns, which R answers already, fill what is left of the pass. Every
// turned encoding's relation waits on no other answer, and they are put SCHEDULER_ROUND_MOST to a round. A turned
// encoding that answers as the form it was turned off does is that form (khw_form_stands_for) and enters the set
// nowhere: only one answering another relation, the negate bits' take off an add, is a form of its own. Where
// `discover` is 0 each that answered a relation is then asked its
// fields; where it is not 0 it is kept with its fields unasked, for a cross-check to read its turns before a later
// pass asks them off a part that would hang on an encoding the vendor calls illegal. Only a classified form is a
// source of a widening: a form still short of its operand runs would widen on bits its register-runs have not yet
// sorted from the operands. The count found this round
unsigned int khw_widened(unsigned int from, int discover)
{
    const unsigned int until = s_names;
    unsigned int found = 0u;
    // every turned encoding not yet answered for, each once, and whether it was turned off a frame form, whose
    // encodings are put whole over the frame's cases and read against the frame's relations
    static unsigned long long s_turned_low[KHW_FORMS];
    static unsigned long long s_turned_high[KHW_FORMS];
    static int s_turned_frame[KHW_FORMS];
    unsigned int turned = 0u;
    for (unsigned int at = until; at > from; at -= 1u)
    {
        const KhwAnswered held = s_named[at - 1u];
        const int framed = (held.frame != (unsigned int)FRAME_RELATION_COUNT);
        if ((discover != 0) && (held.phase != KHW_PHASE_CLASSIFIED))
        {
            continue;
        }
        for (unsigned int bit = KHW_TURN_FIRST; (bit <= KHW_TURN_LAST) && (turned < KHW_FORMS); bit += 1u)
        {
            int inside = 0;
            for (unsigned int run = 0u; run < held.runs; run += 1u)
            {
                inside = inside ||
                         ((held.place[run] != KHW_PLACE_NONE) && (bit >= held.first[run]) && (bit <= held.last[run]));
            }
            unsigned long long low = held.low;
            unsigned long long high = held.high;
            khw_run_write(&low, &high, bit, bit, khw_run_read(low, high, bit, bit) ^ 1ull);
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
    static KhwAnswered s_related[SCHEDULER_ROUND_MOST];
    for (unsigned int first = 0u; first < turned; first += SCHEDULER_ROUND_MOST)
    {
        const unsigned int last = ((first + SCHEDULER_ROUND_MOST) < turned) ? (first + SCHEDULER_ROUND_MOST) : turned;
        s_pooled = 0u;
        for (unsigned int at = first; at < last; at += 1u)
        {
            if (s_turned_frame[at] != 0)
            {
                khw_frame_pooled(s_turned_low[at], s_turned_high[at], frame_word, frame_cases);
            }
            else
            {
                khw_pooled(s_slot, s_turned_low[at], s_turned_high[at], s_union_word, s_union_cases);
            }
        }
        khw_round_carried();
        unsigned int related = 0u;
        for (unsigned int at = first; at < last; at += 1u)
        {
            KhwAnswered held;
            memset(&held, 0, sizeof(held));
            held.low = s_turned_low[at];
            held.high = s_turned_high[at];
            const int answered = (s_turned_frame[at] != 0) ? khw_frame_of(&s_pool[at - first], &held)
                                                           : khw_relation_of(&s_pool[at - first], &held);
            if (answered)
            {
                s_related[related] = held;
                related += 1u;
            }
        }
        s_pooled = 0u;
        for (unsigned int at = 0u; (at < related) && (s_names < KHW_FORMS); at += 1u)
        {
            KhwAnswered held = s_related[at];
            // a turn the set's form answers for already is that form, and enters nowhere
            if (khw_form_stood_for(s_named, s_names, &held))
            {
                continue;
            }
            if (discover == 0)
            {
                khw_fields_asked(&held);
            }
            s_named[s_names] = held;
            s_names += 1u;
            found += 1u;
            char relation[64];
            khw_relation_named(&held, 1, relation, sizeof(relation));
            if (discover != 0)
            {
                printf("    %-14s %016llx %016llx  discovered, its turns held for a cross-check\n", relation, held.low,
                       held.high);
            }
            else
            {
                printf("    %-14s %016llx %016llx  %u runs, %u surviving\n", relation, held.low, held.high, held.runs,
                       held.survivors);
            }
        }
    }
    return found;
}
