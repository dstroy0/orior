// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// khw_write.c: a part's forms learned by asking the part, and its .khw written from the answers
//
//     khw_write <part> <path.khw> <layout> <mnemonics> <fields> <machine writer> [<rounds>] -- <carrier word>...
//
// Every form in the file this writes is there because the part answered for it (P8). Nothing reads a listing, a
// disassembler or a vendor's table of operations: the part is handed code and cases over the run channel
// (run_channel.h), the host computes the same cases off the ladder's relations (ladder.h), and a form enters the file
// where the part's answer and the host's agree on every case of one relation.
//
// The kernel is the code the container the system accepted already holds (the layout's container rows, `layout`
// above). It loads a case's first two words into two registers, computes one instruction, and stores the result as
// the case's answer. That one instruction is the slot, and which place of the kernel it stands at is asked: the kernel
// is put with each place holding the encoding its unreached places hold, and the place whose answer comes back as the
// case's second word is the slot, because nothing then wrote the register the kernel stores.
//
// The first question is the kernel's own relation: the kernel the system accepted, every instruction of it waiting on
// all six barriers and otherwise unchanged, over every case of each relation of the ladder, and the launches of
// the one that answers timed, the time the part takes to run it. Every question after is put from what that one
// answered.
//
// What is asked of a form, in order:
//
//   the relation   the form put over every case of each relation of the ladder, both orders of the case's two words.
//                  The relation every case answers is the form's, and the order that answered is the order its
//                  operands are read in. A form that answers no relation on every case is kept out of the file.
//   the fields     each bit from 12 to 104 turned one at a time, the key and the scheduler's own word left alone
//                  (P9), and the runs of adjacent bits whose turning changes the answer kept. A run set to the number
//                  of each of the two registers the kernel left a case word in answers two different words where it
//                  carries a register, and one word where it does not: an operand's run answers two words and a
//                  modifier's run one. The run whose setting leaves the answer at the case's second word is where the form
//                  writes its result. A run that answers the relation at one register and the relation over that
//                  register's word twice at the other reads that word. Each run keeps its place in the relation's
//                  tuple, the words in their order and then the answer, as 1,1 -> 2 holds them, and a run neither
//                  test reads keeps none.
//   the name       no name is composed here: the protocol keeps the relation answered, how many sources the fields
//                  found, and whether the answers read the form signed, and the vendor's writer (`machine writer`
//                  above, run through the interface) names it from its own table (mnemonic_nvidia.tsv) and writes it.
//
// Then the forms found are widened: each bit outside every run, the key and the scheduler's word is turned, and the
// turned encoding is asked its relation from the top. A turn that answers a relation is a form of its own. Widening
// repeats over what it finds for `rounds` rounds, 1 where none is given.
//
// The gate on the host (cubin_safe.h) reads every question against the .khw the vendor's writer lays out, so that
// file is written before asking and the carrier is handed that same path. The vendor's writer runs in a gate mode,
// which holds every place of the kernel whole under its own encoding so the gate reads a question against what the
// part has already run, and a final mode, which at the end holds only the forms the part named.
#include "khw_write/khw_write_internal.h"

int main(int count, char **word)
{
    int split = 0;
    while ((split < count) && (strcmp(word[split], "--") != 0))
    {
        split += 1;
    }
    if ((split < 7) || (split >= count) || ((count - split) < 2))
    {
        printf("khw_write <part> <path.khw> <layout> <mnemonics> <fields> <machine writer> [<rounds>] -- "
               "<carrier word>...\n");
        return 2;
    }
    s_part = word[1];
    s_path = word[2];
    s_layout = word[3];
    s_mnemonics = word[4];
    s_fields = word[5];
    s_machine_writer = word[6];
    // [<rounds>] and, for a dry run tuned off the part, [--slot <n>] and [--forms <name>]: the slot whose questions are
    // emitted for the vendor's disassembler, and the blob of the run's buffer holding the forms a prior run learned to
    // seed them from, in place of the learn loop (khw_enumerate). The split's passes over the working set: [--discover]
    // widens it a round and keeps what it finds with its fields unasked; [--turns] asks the turns of what a discover
    // left; [--registers] asks the register-runs the turns found; [--costs] times every classified form (P6) and keeps
    // its least and spread in the part's .ksc. A cross-check reads each round's questions before it is carried
    // (run_channel.h), and no form's turns or register-runs reach the part before the vendor has read them
    unsigned int rounds = 1u;
    unsigned int enumerate = 0xffffffffu;
    const char *forms = NULL;
    // [--seeds <name>] with --discover: the blob of the run holding encodings to ask their relation of, a line
    // `<low> <high>` in hex each, as a vendor's reader of a listing writes them (sass_listing_seeds): the question Q17
    // names and the part answers, each a discovered form where it answers a ladder relation and passed over where not
    const char *seeds = NULL;
    int discover = 0;
    int turns = 0;
    int registers = 0;
    int costs = 0;
    for (int at = 7; at < split; at += 1)
    {
        if ((strcmp(word[at], "--slot") == 0) && ((at + 1) < split))
        {
            enumerate = (unsigned int)strtoul(word[at + 1], NULL, 10);
            at += 1;
        }
        else if ((strcmp(word[at], "--forms") == 0) && ((at + 1) < split))
        {
            forms = word[at + 1];
            at += 1;
        }
        else if ((strcmp(word[at], "--seeds") == 0) && ((at + 1) < split))
        {
            seeds = word[at + 1];
            at += 1;
        }
        else if (strcmp(word[at], "--discover") == 0)
        {
            discover = 1;
        }
        else if (strcmp(word[at], "--turns") == 0)
        {
            turns = 1;
        }
        else if (strcmp(word[at], "--registers") == 0)
        {
            registers = 1;
        }
        else if (strcmp(word[at], "--costs") == 0)
        {
            costs = 1;
        }
        else
        {
            rounds = (unsigned int)strtoul(word[at], NULL, 10);
        }
    }
    if (!khw_kernel_read())
    {
        return 1;
    }
    // the gate's file written before asking: the vendor's writer lays the kernel's places out, and the carrier reads
    // the questions of every round against them
    if (!khw_vendor_run("gate", KHW_FORMS_NAME))
    {
        return 1;
    }
    const char *carrier[KHW_CARRIER_WORDS];
    unsigned int words = 0u;
    for (int at = split + 1; (at < count) && (words < ((sizeof(carrier) / sizeof(carrier[0])) - 1u)); at += 1)
    {
        carrier[words] = word[at];
        words += 1u;
    }
    carrier[words] = NULL;
    if (!run_channel_open(carrier, 60000000ull))
    {
        return 1;
    }
    // R beside the machine file, its .kqr, so that no question the part has answered is put to it again
    char record[1024];
    const size_t stem =
        strlen(s_path) - (((strlen(s_path) > 4u) && (strcmp(s_path + strlen(s_path) - 4u, ".khw") == 0)) ? 4u : 0u);
    // how far a container's code reaches, from the .ksc beside the machine file, for a register hold the cross gives
    char reach[1024];
    snprintf(reach, sizeof(reach), "%.*s.ksc", (int)stem, s_path);
    // the part's operand fields into the .ksc the walks read them from
    snprintf(s_ksc_path, sizeof(s_ksc_path), "%s", reach);
    khw_fields_written(reach);
    if (!run_channel_registers_past(reach))
    {
        printf("  %s holds no answer of how far a container's code reaches: a register hold stays held\n", reach);
    }
    snprintf(record, sizeof(record), "%.*s.kqr", (int)stem, s_path);
    if (!run_channel_record(record, s_part, "khw_write"))
    {
        run_channel_close();
        return 1;
    }
    s_pool = (RunQuestion *)calloc(SCHEDULER_ROUND_MOST, sizeof(RunQuestion));
    if (s_pool == NULL)
    {
        printf("  khw_write: no room for a round's questions\n");
        run_channel_close();
        return 1;
    }
    // tuned off the part: the slot's first field-finding round emitted for the vendor's disassembler, the learn loop
    // left for a run that reaches the part
    if (enumerate != 0xffffffffu)
    {
        if (enumerate >= s_kernel_places)
        {
            printf("  khw_write: the slot %u is past the kernel's %u places\n", enumerate, s_kernel_places);
            run_channel_close();
            return 1;
        }
        khw_enumerate(enumerate, forms);
        run_channel_close();
        printf("  enumerated the slot %u's questions off the part%s, %llu asks\n", enumerate,
               (forms != NULL) ? " from the forms a prior run learned" : "", s_asks);
        return 0;
    }
    // the split: a discover pass widens the working set a round and keeps what it finds unasked, a turns pass asks the
    // turns of what a discover left, a register pass asks the register-runs the turns found. A cross-check reads each
    // round's questions before it is carried, and neither a form's turns nor its register-runs reach the part before
    // the vendor has read them. The working set is the run's blob KHW_WORK_NAME, which the passes hand each other; the
    // slot is found afresh each pass, off the kernel's code
    if ((discover != 0) || (turns != 0) || (registers != 0) || (costs != 0))
    {
        KhwAnswered kernel;
        memset(&kernel, 0, sizeof(kernel));
        unsigned long long nanoseconds = 0ull;
        unsigned int slot = 0u;
        if (!khw_first_round(&kernel, &slot, &nanoseconds) || (slot == s_kernel_places))
        {
            printf("  khw_write: no slot was found, and nothing was asked of a form\n");
            run_channel_close();
            return 1;
        }
        s_slot = slot;
        s_names = khw_forms_read(KHW_WORK_NAME);
        if (discover != 0)
        {
            unsigned int seeded = 0u;
            if (s_names == 0u)
            {
                // the working set empty: the kernel's own slot form seeded as a discovered form, its relation asked but
                // its fields left for the turns and register passes to ask once a cross has read them, as of any form.
                // The relation is the real instruction the part already ran, safe to ask without a cross
                const unsigned long long slot_low = khw_word_read(&s_kernel_text[s_slot * KHW_INSTRUCTION], 0u);
                const unsigned long long slot_high = khw_word_read(&s_kernel_text[s_slot * KHW_INSTRUCTION], 8u);
                KhwAnswered held;
                memset(&held, 0, sizeof(held));
                if (khw_relation_asked(slot_low, slot_high, &held))
                {
                    held.phase = KHW_PHASE_DISCOVERED;
                    s_named[s_names] = held;
                    s_names += 1u;
                    seeded = 1u;
                }
                printf("  the slot: place %u of %u, at 0x%x; the kernel's form seeded, its turns await a cross-check\n",
                       s_slot, s_kernel_places, s_slot * KHW_INSTRUCTION);
            }
            if (seeds != NULL)
            {
                seeded += khw_seeds_asked(seeds);
            }
            const unsigned int found = khw_widened(0u, 1);
            run_channel_close();
            if (!khw_forms_work_written())
            {
                return 1;
            }
            printf("  discover: %u seeded, %u widened, %u in the working set, %llu asks; their turns await a "
                   "cross-check\n",
                   seeded, found, s_names, s_asks);
            return 0;
        }
        if (turns != 0)
        {
            // every discovered form turned, its register-runs left for the cross-check after: a discover set its
            // relation and the order its words read, which the turns need and read back from the working set
            unsigned int asked = 0u;
            for (unsigned int at = 0u; at < s_names; at += 1u)
            {
                if ((s_named[at].phase == KHW_PHASE_DISCOVERED) && khw_fields_turns(&s_named[at]))
                {
                    asked += 1u;
                }
            }
            run_channel_close();
            if (!khw_forms_work_written())
            {
                return 1;
            }
            printf("  turns: %u form(s) turned, %llu asks; their register-runs await a cross-check\n", asked, s_asks);
            return 0;
        }
        if (costs != 0)
        {
            // every classified form timed at the slot, its least and spread kept in the part's .ksc beside the machine
            // file, where the stall and the last register are kept
            char ksc_path[1024];
            snprintf(ksc_path, sizeof(ksc_path), "%.*s.ksc", (int)stem, s_path);
            const unsigned int timed = khw_costs_asked(ksc_path);
            run_channel_close();
            printf("  costs: %u form(s) timed over %u launches each, %llu asks; kept in %s\n", timed, KHW_LAUNCHES,
                   s_asks, ksc_path);
            return 0;
        }
        // every turned form's register-runs asked, its operand runs kept and its phase classified; what the part has
        // answered for whole then laid out by the vendor
        unsigned int asked = 0u;
        for (unsigned int at = 0u; at < s_names; at += 1u)
        {
            if ((s_named[at].phase == KHW_PHASE_TURNED) && khw_fields_registers(&s_named[at]))
            {
                asked += 1u;
            }
        }
        run_channel_close();
        if (!khw_forms_work_written() || !khw_answers_written() || !khw_vendor_run("final", KHW_FORMS_NAME))
        {
            return 1;
        }
        printf("  registers: %u form(s) classified, %llu asks; %s names what the part has answered for\n", asked,
               s_asks, s_path);
        return 0;
    }
    KhwAnswered kernel;
    memset(&kernel, 0, sizeof(kernel));
    unsigned long long nanoseconds = 0ull;
    unsigned int slot = 0u;
    const int related = khw_first_round(&kernel, &slot, &nanoseconds);
    printf("  the first round: the kernel's relation and %llu places asked for the slot, %llu asks\n", s_asks - 1ull,
           s_asks);
    if (!related)
    {
        printf("  khw_write: the kernel the system accepted answered no relation of the ladder (%s), and "
               "nothing was asked after it\n",
               s_pool[0].refused);
        run_channel_close();
        return 1;
    }
    printf("  the kernel: ladder.%s, its words read %s, %u candidates surviving, %llu ns a launch over %u launches\n",
           s_anchor_text[kernel.anchor], (kernel.swapped != 0) ? "swapped" : "in order", kernel.survivors,
           nanoseconds / KHW_LAUNCHES, KHW_LAUNCHES);
    s_slot = slot;
    if (s_slot == s_kernel_places)
    {
        printf("  khw_write: no place of the kernel answered as the slot, and nothing was asked of a form\n");
        run_channel_close();
        return 1;
    }
    printf("  the slot: place %u of %u, at 0x%x\n", s_slot, s_kernel_places, s_slot * KHW_INSTRUCTION);
    const unsigned long long slot_low = khw_word_read(&s_kernel_text[s_slot * KHW_INSTRUCTION], 0u);
    const unsigned long long slot_high = khw_word_read(&s_kernel_text[s_slot * KHW_INSTRUCTION], 8u);
    printf("  the forms the part answers for:\n");
    khw_form_asked(slot_low, slot_high);
    unsigned int from = 0u;
    for (unsigned int round = 0u; round < rounds; round += 1u)
    {
        const unsigned int until = s_names;
        const unsigned int found = khw_widened(from, 0);
        from = until;
        printf("  round %u: %u forms found, %u in all, %llu asks\n", round + 1u, found, s_names, s_asks);
        if (found == 0u)
        {
            break;
        }
    }
    run_channel_close();
    // the forms the part answered written out, and the vendor's writer run once more to name them and lay out the file
    if (!khw_answers_written() || !khw_vendor_run("final", KHW_FORMS_NAME))
    {
        return 1;
    }
    printf("  khw_write: %llu asks put\n", s_asks);
    return 0;
}
