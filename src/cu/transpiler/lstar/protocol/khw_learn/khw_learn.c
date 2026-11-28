// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// khw_learn.c: a part's forms learned by querying the part, and its .khw written from the answers
//
//     orior_khw_learn <part> <path.khw> <layout> <mnemonics> <fields> <machine writer> [<rounds>] -- <carrier word>...
//
// Every form in the file that this writes is there because the part answered for it (P8). Nothing reads a listing, a
// disassembler or a table of operations of the vendor. The part gets code and cases over the run channel
// (run_channel.h). The host computes the same cases from the relations of the ladder (ladder.h). A form enters the
// file where the answer of the part and the answer of the host agree on every case of one relation.
//
// The kernel is the code that the container already holds, the one that the system accepted (the container rows of
// the layout, `layout` above). It loads the first two words of a case into two registers, computes one instruction,
// and stores the result as the answer of the case. That one instruction is the position, and the program queries
// which place of the kernel it is at. It puts the kernel with the encoding of its unreached places in each place.
// The place whose answer comes back as the second word of the case is the position. Nothing then wrote the register
// that the kernel stores.
//
// The first question is the own relation of the kernel. It puts the kernel that the system accepted over every case of
// each relation of the ladder. Every instruction of the kernel waits on all six barriers, with no other change. It
// also times the launches of the one that answers, which gives the time that the part takes to run it. Every later
// question comes from what that one answered.
//
// What the program queries of a form, in order:
//
//   - the relation: the form put over every case of each relation of the ladder, with both orders of the two words of
//     the case. The relation that every case answers is the relation of the form. The order that answered is the order
//     in which it reads its operands. A form that answers no relation on every case stays out of the file.
//   - the fields: each bit from 12 to 104 turned one at a time, with the key and the own word of the scheduler left
//     alone (P9). The program records the runs of adjacent bits whose turn changes the answer. The program sets a run
//     to the number of each of the two registers that hold a case word. It answers two different words where it
//     carries a register, and one word where it does not. Thus the run of an operand answers two words, and the run of
//     a modifier answers one.
//   - the places of the runs: the destination is the run whose value leaves the answer at the second word of the
//     case. A run that answers the relation at one register, and the relation over the word of that
//     register twice at the other, reads that word. Each run keeps its place in the tuple of the relation: the words in
//     their order and then the answer, as 1,1 -> 2 shows them. A run that neither test reads keeps none.
//   - the name: the program makes no name. The protocol records the relation answered, how many sources the fields
//     found, and whether the answers read the form as signed. The writer of the vendor (`machine writer` above, run
//     through the interface) names it from its own table (mnemonic_nvidia.tsv) and writes it.
//
// Then the program widens the forms found. It turns each bit outside every run, the key and the word of the scheduler.
// Then it queries the relation of the turned encoding from the top. A turn that answers a relation is a form of its
// own. The widening repeats over what it finds for `rounds` rounds, 1 where the caller gives none.
//
// The gate on the host (cubin_safe.h) reads every question against the .khw that the writer of the vendor arranges.
// Thus the program writes that file before it queries, and passes the same path to the carrier. The writer of the
// vendor runs in a gate mode or a final mode. The gate mode stores every place of the kernel whole under its own
// encoding. Thus the gate reads a question against what the part already ran. The final mode stores at the end
// only the forms that the part named.
#include "../../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "khw_learn_internal.h"
#include "memoria_operor/memoria_operor.h"

int program_main(int count, char **word)
{
    int split = 0;
    while ((split < count) && (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = word[split],
                                           .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "--", .ci = EMBED_FALSE)))
    {
        split += 1;
    }
    if ((split < 7) || (split >= count) || ((count - split) < 2))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "orior_khw_learn <part> <path.khw> <layout> <mnemonics> <fields> <machine "
                                       "writer> [<rounds>] -- <carrier word>...");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 2;
    }
    s_part = word[1];
    s_path = word[2];
    s_layout = word[3];
    s_mnemonics = word[4];
    s_fields = word[5];
    s_machine_writer = word[6];
    // [<rounds>]. Two options serve a dry run away from the part:
    //   - [--position <n>] gives the position whose questions go to the disassembler of the vendor;
    //   - [--forms <name>] gives the blob of the run that holds the forms that a prior run learned. The program starts
    //     from them in place of the learn loop (khw_enumerate).
    // The passes of the split over the working set:
    //   - [--discover] widens it a round and keeps what it finds, with its fields not queried;
    //   - [--turns] queries the turns of what a discover left;
    //   - [--registers] queries the register-runs that the turns found;
    //   - [--costs] times every form that the machine file names on the timer register (P6). It stores the cost and
    //     spread of each in the .ksc of the part.
    // A cross-check reads the questions of each round before the carry (run_channel.h). The turns or register-runs of
    // a form reach the part only after the vendor read them
    unsigned int rounds = 1u;
    unsigned int enumerate = 0xffffffffu;
    const char *forms = NULL;
    // [--seeds <name>] with --discover: the blob of the run that holds encodings to query the relation of. Each is a
    // line `<low> <high>` in hex, as a reader of a listing of the vendor writes them (orior_sass_listing_seeds). Q17
    // names the question, and the part answers it. Each encoding that answers a ladder relation is a discovered form,
    // and the program skips the others
    const char *seeds = NULL;
    int discover = 0;
    int turns = 0;
    int registers = 0;
    int costs = 0;
    for (int at = 7; at < split; at += 1)
    {
        if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = word[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = "--position", .ci = EMBED_FALSE)) &&
            ((at + 1) < split))
        {
            enumerate = (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = word[at + 1], .end = NULL,
                                                 .base = 10);
            at += 1;
        }
        else if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = word[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "--forms", .ci = EMBED_FALSE)) &&
                 ((at + 1) < split))
        {
            forms = word[at + 1];
            at += 1;
        }
        else if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = word[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "--seeds", .ci = EMBED_FALSE)) &&
                 ((at + 1) < split))
        {
            seeds = word[at + 1];
            at += 1;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = word[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--discover", .ci = EMBED_FALSE))
        {
            discover = 1;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = word[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--turns", .ci = EMBED_FALSE))
        {
            turns = 1;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = word[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--registers", .ci = EMBED_FALSE))
        {
            registers = 1;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = word[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--costs", .ci = EMBED_FALSE))
        {
            costs = 1;
        }
        else
        {
            rounds =
                (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = word[at], .end = NULL, .base = 10);
        }
    }
    if (!khw_kernel_read())
    {
        return 1;
    }
    // the file of the gate, written before the queries. The writer of the vendor sets out the places of the kernel, and
    // the carrier reads the questions of every round against them
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
    if (!run_channel_open(carrier, 60000000ULL))
    {
        return 1;
    }
    // R beside the machine file, its .kqr. Thus the program puts no question to the part again that the part answered
    char record[1024];
    const size_t stem =
        EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = s_path, .cap = PLATFORM_TEXT_TO_TERMINATOR) -
        (((EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = s_path, .cap = PLATFORM_TEXT_TO_TERMINATOR) > 4u) &&
          (EMBED_CALL(
              cellul.eq, CatenaFinitaCfg,
              .src = s_path +
                     EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = s_path, .cap = PLATFORM_TEXT_TO_TERMINATOR) - 4u,
              .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = ".khw", .ci = EMBED_FALSE)))
             ? 4u
             : 0u);
    // how far a container's code reaches, from the .ksc beside the machine file, for a register refusal the cross gives
    char reach[1024];
    {
        VerbaLinea buffer_line = mmgr_line_over(reach, sizeof(reach));
        mmgr_line_put_max(&buffer_line, s_path, (size_t)((int)stem));
        mmgr_line_put(&buffer_line, ".ksc");
        (void)mmgr_line_finish(&buffer_line);
    }
    // the part's operand fields into the .ksc the walks read them from
    {
        VerbaLinea buffer_line = mmgr_line_over(s_ksc_path, sizeof(s_ksc_path));
        mmgr_line_put(&buffer_line, reach);
        (void)mmgr_line_finish(&buffer_line);
    }
    khw_fields_written(reach);
    if (!run_channel_registers_past(reach))
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, reach);
        mmgr_line_put(output_line,
                      " contains no answer of how far a container's code reaches: a register refusal stays refused");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea buffer_line = mmgr_line_over(record, sizeof(record));
        mmgr_line_put_max(&buffer_line, s_path, (size_t)((int)stem));
        mmgr_line_put(&buffer_line, ".kqr");
        (void)mmgr_line_finish(&buffer_line);
    }
    if (!run_channel_record(record, s_part, "khw_write"))
    {
        run_channel_close();
        return 1;
    }
    s_pool = (RunQuestion *)platform_cell_cleared(SCHEDULER_ROUND_MAX, sizeof(RunQuestion),
                                                  platform_place_g_program_owner, 0);
    if (s_pool == NULL)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  khw_write: no room for a round's questions");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        run_channel_close();
        return 1;
    }
    // away from the part: the first round of the position that finds fields goes to the disassembler of the vendor.
    // The learn loop waits for a run that reaches the part
    if (enumerate != 0xffffffffu)
    {
        if (enumerate >= s_kernel_places)
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  khw_write: the probe place ");
                mmgr_line_decimal(output_line, enumerate, 1u);
                mmgr_line_put(output_line, " is past the kernel's ");
                mmgr_line_decimal(output_line, s_kernel_places, 1u);
                mmgr_line_put(output_line, " places");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            run_channel_close();
            return 1;
        }
        khw_enumerate(enumerate, forms);
        run_channel_close();
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  enumerated the probe place ");
            mmgr_line_decimal(output_line, enumerate, 1u);
            mmgr_line_put(output_line, "'s questions off the part");
            mmgr_line_put(output_line, (forms != NULL) ? " from the forms a prior run learned" : "");
            mmgr_line_put(output_line, ", ");
            mmgr_line_decimal(output_line, s_query_count, 1u);
            mmgr_line_put(output_line, " queries");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    // the split: a discover pass widens the working set a round and keeps what it finds, not queried. A turns pass
    // queries the turns of what a discover left, and a register pass queries the register-runs that the turns found.
    // A cross-check reads the questions of each round before the carry. The turns and register-runs of a form reach the
    // part only after the vendor read them. The working set is the blob KHW_WORK_NAME of the run, which the passes
    // hand each other. Each pass finds the position again, from the code of the kernel
    if ((discover != 0) || (turns != 0) || (registers != 0) || (costs != 0))
    {
        KhwAnswered kernel;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &kernel, .bytes = sizeof(kernel), .val = 0);
        unsigned long long nanoseconds = 0ULL;
        unsigned int probe_place = 0u;
        if (!khw_first_round(&kernel, &probe_place, &nanoseconds) || (probe_place == s_kernel_places))
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  khw_write: no probe place was found, and nothing was queried of a form");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            run_channel_close();
            return 1;
        }
        s_probe_place = probe_place;
        // the working set read holds one form for each answer. The program lets go a form that an older one is for
        // (khw_form_is_for). Thus the turns of the bits of a form outside its operands do not fill the set
        const unsigned int read = khw_forms_read(KHW_WORK_NAME);
        s_names = khw_forms_folded(s_named, read);
        if (read != s_names)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  the working set: ");
            mmgr_line_decimal(output_line, read, 1u);
            mmgr_line_put(output_line, " forms read, ");
            mmgr_line_decimal(output_line, read - s_names, 1u);
            mmgr_line_put(output_line, " let go, each an encoding of a form the set contains");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        if (discover != 0)
        {
            unsigned int seeded = 0u;
            const unsigned long long probe_low = khw_word_read(&s_kernel_text[s_probe_place * KHW_INSTRUCTION], 0u);
            const unsigned long long probe_high = khw_word_read(&s_kernel_text[s_probe_place * KHW_INSTRUCTION], 8u);
            if (s_names == 0u)
            {
                // the working set is empty. The own position form of the kernel goes in as a discovered form. The
                // program queries its relation, but leaves its fields to the turns and register passes, after a cross
                // reads them, as for any form. The relation is the real instruction that the part already ran, safe to
                // query without a cross
                KhwAnswered form;
                EMBED_CALL(memor.set, MemoriaCfg, .dst = &form, .bytes = sizeof(form), .val = 0);
                if (khw_relation_queried(probe_low, probe_high, &form))
                {
                    form.phase = KHW_PHASE_DISCOVERED;
                    s_named[s_names] = form;
                    s_names += 1u;
                    seeded = 1u;
                }
                {
                    VerbaLinea *const output_line = platform_line_out();
                    mmgr_line_put(output_line, "  the probe place: place ");
                    mmgr_line_decimal(output_line, s_probe_place, 1u);
                    mmgr_line_put(output_line, " of ");
                    mmgr_line_decimal(output_line, s_kernel_places, 1u);
                    mmgr_line_put(output_line, ", at 0x");
                    mmgr_line_hex(output_line, s_probe_place * KHW_INSTRUCTION, 1u);
                    mmgr_line_put(output_line, "; the kernel's form seeded, its turns await a cross-check");
                    mmgr_line_ch(output_line, '\n');
                    platform_line_out_ended();
                }
            }
            if (seeds != NULL)
            {
                seeded += khw_seeds_queried(seeds);
            }
            // the frame round, where a classified form contains the position's registers: the position's own form,
            // classified by a register pass before this discover
            const KhwAnswered *probed_form = NULL;
            for (unsigned int at = 0u; at < s_names; at += 1u)
            {
                const KhwAnswered *const form = &s_named[at];
                if ((form->phase == KHW_PHASE_CLASSIFIED) && (form->runs != 0u) && (form->low == probe_low) &&
                    (form->high == probe_high))
                {
                    probed_form = form;
                }
            }
            const unsigned int framed = (probed_form != NULL) ? khw_frame_round(probed_form, 1) : 0u;
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  the frame round: ");
                mmgr_line_put(output_line,
                              (probed_form == NULL) ? "the probe place's form is not yet classified, " : "");
                mmgr_line_decimal(output_line, framed, 1u);
                mmgr_line_put(output_line, " place(s) answered a frame relation");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            khw_plan_passed("discover", (unsigned long long)KHW_FORMS);
            const unsigned int found = khw_widened(0u, 1);
            run_channel_close();
            if (!khw_forms_work_written())
            {
                return 1;
            }
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  discover: ");
                mmgr_line_decimal(output_line, seeded, 1u);
                mmgr_line_put(output_line, " seeded, ");
                mmgr_line_decimal(output_line, found, 1u);
                mmgr_line_put(output_line, " widened, ");
                mmgr_line_decimal(output_line, s_names, 1u);
                mmgr_line_put(output_line, " in the working set, ");
                mmgr_line_decimal(output_line, s_query_count, 1u);
                mmgr_line_put(output_line, " queries; their turns await a cross-check");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            return 0;
        }
        if (turns != 0)
        {
            // every discovered form turned at once, in shared rounds, with its register-runs left for the cross-check
            // after. A discover set its relation and the order in which it reads its words. The turns need these, and
            // read them back from the working set
            static KhwAnswered *s_forms[KHW_FORMS];
            const unsigned int count = khw_comparisons_void(s_forms, khw_forms_in_phase(KHW_PHASE_DISCOVERED, s_forms));
            unsigned long long stages = 0ULL;
            for (unsigned int at = 0u; at < count; at += 1u)
            {
                stages += (unsigned long long)khw_turning_cases(s_forms[at]);
            }
            khw_plan_passed("turns", stages * ((unsigned long long)KHW_TURNS + 1ULL));
            const unsigned int fields_turned = khw_fields_turns(s_forms, count);
            run_channel_close();
            if (!khw_forms_work_written())
            {
                return 1;
            }
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  turns: ");
                mmgr_line_decimal(output_line, fields_turned, 1u);
                mmgr_line_put(output_line, " form(s) turned, ");
                mmgr_line_decimal(output_line, s_query_count, 1u);
                mmgr_line_put(output_line, " queries; their register-runs await a cross-check");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            return 0;
        }
        if (costs != 0)
        {
            // every form that the machine file names, timed on the timer register. Its cost and spread go in the .ksc
            // of the part, beside the machine file. A costs question writes a form many times over from the position,
            // and the gate then holds a second instruction that no form holds. Thus the writer of the vendor writes the
            // file of the gate again before the queries (`costs`). The file holds the places of the kernel and the
            // forms that the part answered for. After, the vendor arranges the machine file again from the forms that
            // the part answered for, as the registers pass leaves it
            char ksc_path[1024];
            {
                VerbaLinea buffer_line = mmgr_line_over(ksc_path, sizeof(ksc_path));
                mmgr_line_put_max(&buffer_line, s_path, (size_t)((int)stem));
                mmgr_line_put(&buffer_line, ".ksc");
                (void)mmgr_line_finish(&buffer_line);
            }
            if (!khw_answers_written() || !khw_vendor_run("costs", KHW_FORMS_NAME))
            {
                run_channel_close();
                return 1;
            }
            const unsigned int timed = khw_costs_queried(ksc_path);
            run_channel_close();
            if (!khw_answers_written() || !khw_vendor_run("final", KHW_FORMS_NAME))
            {
                return 1;
            }
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  costs: ");
                mmgr_line_decimal(output_line, timed, 1u);
                mmgr_line_put(output_line, " form(s) timed over ");
                mmgr_line_decimal(output_line, KHW_COST_COPIES, 1u);
                mmgr_line_put(output_line, " copies and ");
                mmgr_line_decimal(output_line, KHW_LAUNCHES, 1u);
                mmgr_line_put(output_line, " launches each on the timer register, ");
                mmgr_line_decimal(output_line, s_query_count, 1u);
                mmgr_line_put(output_line, " queries; stored in ");
                mmgr_line_put(output_line, ksc_path);
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            return 0;
        }
        // the register-runs of every turned form, queried at once in shared rounds. The program records its operand
        // runs and classifies its phase. Then the vendor arranges what the part answered for whole
        static KhwAnswered *s_forms[KHW_FORMS];
        const unsigned int turned = khw_forms_in_phase(KHW_PHASE_TURNED, s_forms);
        // the program queries again a classified form with two runs that carry one place, over a turn case that reads
        // apart
        const unsigned int count = khw_forms_placed_twice(s_forms, turned);
        if (count != turned)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  registers: ");
            mmgr_line_decimal(output_line, count - turned, 1u);
            mmgr_line_put(output_line, " classified form(s) with a place present twice queried again");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        unsigned long long settings = 0ULL;
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            settings += (unsigned long long)s_forms[at]->runs * KHW_SETTINGS * KHW_STAGES;
        }
        khw_plan_passed("registers", settings);
        const unsigned int fields_turned = khw_fields_registers(s_forms, count);
        run_channel_close();
        if (!khw_forms_work_written() || !khw_answers_written() || !khw_vendor_run("final", KHW_FORMS_NAME))
        {
            return 1;
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  registers: ");
            mmgr_line_decimal(output_line, fields_turned, 1u);
            mmgr_line_put(output_line, " form(s) classified, ");
            mmgr_line_decimal(output_line, s_query_count, 1u);
            mmgr_line_put(output_line, " queries; ");
            mmgr_line_put(output_line, s_path);
            mmgr_line_put(output_line, " names what the part has answered for");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    KhwAnswered kernel;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &kernel, .bytes = sizeof(kernel), .val = 0);
    unsigned long long nanoseconds = 0ULL;
    unsigned int probe_place = 0u;
    const int related = khw_first_round(&kernel, &probe_place, &nanoseconds);
    if (s_kernel_probe_place < s_kernel_places)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(
            output_line,
            "  the first round: the kernel's relation queried, the probe place the part's answer in its .ksc, place ");
        mmgr_line_decimal(output_line, probe_place, 1u);
        mmgr_line_put(output_line, " of the retained places, ");
        mmgr_line_decimal(output_line, s_query_count, 1u);
        mmgr_line_put(output_line, " queries");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    else
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the first round: the kernel's relation and ");
        mmgr_line_decimal(output_line, s_query_count - 1ULL, 1u);
        mmgr_line_put(output_line, " places queried for the probe place, ");
        mmgr_line_decimal(output_line, s_query_count, 1u);
        mmgr_line_put(output_line, " queries");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    // a dry channel carries every question by nothing. The kernel answers no relation, and the pass continues with the
    // position that the .ksc names. Every round goes through the airlock as on a live channel, and nothing launches
    const int dry = !related && (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = s_pool[0].refused,
                                            .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "dry:", .ci = EMBED_FALSE));
    if (!related && !dry)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line,
                          "  khw_write: the kernel the system accepted answered no relation of the ladder (");
            mmgr_line_put(output_line, s_pool[0].refused);
            mmgr_line_put(output_line, "), and nothing was queried after it");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        run_channel_close();
        return 1;
    }
    if (dry)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the kernel: dry, carried by nothing; the probe place the .ksc names, and every "
                                   "round put through the airlock with nothing launched");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    else
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the kernel: ladder.");
        mmgr_line_put(output_line, s_anchor_text[kernel.anchor]);
        mmgr_line_put(output_line, ", its words read ");
        mmgr_line_put(output_line, (kernel.swapped != 0) ? "swapped" : "in order");
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal(output_line, kernel.survivors, 1u);
        mmgr_line_put(output_line, " candidates surviving, ");
        mmgr_line_decimal(output_line, nanoseconds / KHW_LAUNCHES, 1u);
        mmgr_line_put(output_line, " a launch over ");
        mmgr_line_decimal(output_line, KHW_LAUNCHES, 1u);
        mmgr_line_put(output_line, " launches on the ");
        mmgr_line_put(output_line, (s_question.clock_address != 0u) ? "timer register, read through the BAR0 driver"
                                                                    : "driver's event timer");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    s_probe_place = probe_place;
    if (s_probe_place == s_kernel_places)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(
                output_line,
                "  khw_write: no place of the kernel answered as the probe place, and nothing was queried of a form");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        run_channel_close();
        return 1;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the probe place: place ");
        mmgr_line_decimal(output_line, s_probe_place, 1u);
        mmgr_line_put(output_line, " of ");
        mmgr_line_decimal(output_line, s_kernel_places, 1u);
        mmgr_line_put(output_line, ", at 0x");
        mmgr_line_hex(output_line, s_probe_place * KHW_INSTRUCTION, 1u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    const unsigned long long probe_low = khw_word_read(&s_kernel_text[s_probe_place * KHW_INSTRUCTION], 0u);
    const unsigned long long probe_high = khw_word_read(&s_kernel_text[s_probe_place * KHW_INSTRUCTION], 8u);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the forms the part answers for:");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    khw_form_queried(probe_low, probe_high);
    // the frame round on the position's own classified form, each place of the kernel queried what it answers at the
    // position
    const unsigned int framed = (s_names != 0u) ? khw_frame_round(&s_named[0], 0) : 0u;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the frame round: ");
        mmgr_line_decimal(output_line, framed, 1u);
        mmgr_line_put(output_line, " place(s) answered a frame relation, ");
        mmgr_line_decimal(output_line, s_query_count, 1u);
        mmgr_line_put(output_line, " queries");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    unsigned int from = 0u;
    for (unsigned int round = 0u; round < rounds; round += 1u)
    {
        const unsigned int until = s_names;
        const unsigned int found = khw_widened(from, 0);
        from = until;
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  round ");
            mmgr_line_decimal(output_line, round + 1u, 1u);
            mmgr_line_put(output_line, ": ");
            mmgr_line_decimal(output_line, found, 1u);
            mmgr_line_put(output_line, " forms found, ");
            mmgr_line_decimal(output_line, s_names, 1u);
            mmgr_line_put(output_line, " in all, ");
            mmgr_line_decimal(output_line, s_query_count, 1u);
            mmgr_line_put(output_line, " queries");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        if (found == 0u)
        {
            break;
        }
    }
    run_channel_close();
    // the forms the part answered written out, and the vendor's writer run once more to name them and arrange the file
    if (!khw_answers_written() || !khw_vendor_run("final", KHW_FORMS_NAME))
    {
        return 1;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  khw_write: ");
        mmgr_line_decimal(output_line, s_query_count, 1u);
        mmgr_line_put(output_line, " queries put");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 0;
}
