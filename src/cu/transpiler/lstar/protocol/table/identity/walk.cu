// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// walk.cu: the one walk over numbers a relation is true at: the last register, the last predicate, and how far a
// container's code reaches
#include "../../../../../../platform/platform.h"
#include "identity_internal.h"

// one question the register walk puts. A chain of ours, its code with every stall the longest, and the register it
// writes that the walk renames. Then every number its register fields represent, ordered and each once, in a cell of
// the temporary tier
struct RegisterProbe
{
    IdentityName number;
    IdentityCode code;
    unsigned long long renamed;
    unsigned long long *named;
    unsigned long long named_count;
};

// the probes the walk goes over at max, each from a chain of its own
#define WALK_PROBES_MAX 3u

// the probe's code and numbers given back
static void register_probe_ended(RegisterProbe *probe)
{
    platform_array_ended(&probe->code);
    platform_cell_released(probe->named, platform_place_g_program_owner);
    probe->named = NULL;
    probe->named_count = 0ULL;
}

// 1 where one of the probe's register fields represents `number`
static int register_probe_names(const RegisterProbe *probe, unsigned long long number)
{
    const unsigned long long at =
        identity_lower(probe->named, probe->named_count, [number](unsigned long long one) { return one < number; });
    return (at < probe->named_count) && (probe->named[at] == number);
}

// `turned` made `code` with every field of `group`, `fields` of them, that contains `from` set to `to`
static void register_renamed(const IdentityCode *code, const StallField *group, unsigned int fields,
                             unsigned long long from, unsigned long long to, IdentityCode *turned)
{
    (void)identity_code_copied(turned, code);
    for (unsigned long long index = 0ULL; index < (code->count / 16ULL); index += 1ULL)
    {
        for (unsigned int field = 0u; field < fields; field += 1u)
        {
            if (stall_bits(code, index, group[field]) == from)
            {
                stall_bits_write(turned, index, group[field], to);
            }
        }
    }
}

// a line on the output stream: the mode's name, then `first`, `second` and `third`
static void walk_said(const char *mode, const char *first, const char *second, const char *third)
{
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "orior_klq_identity ");
    mmgr_line_put(output_line, mode);
    mmgr_line_put(output_line, first);
    mmgr_line_put(output_line, second);
    mmgr_line_put(output_line, third);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
}

// the places of the cases a walk goes over, shared by the walks of this module, one at a time
static unsigned int s_walk_places[RUN_CASES_MAX];

// The chain `cases` names as a probe of the walk, into `probe`. Its code with every stall the longest, the numbers its
// fields represent, and the value it writes that a rename keeps alike. 1, or 0 where the chain is no probe
static int register_probe_read(const char *engine, const HostCases *cases, StallField stall, const StallField *group,
                               unsigned int fields, RunQuestion *question, unsigned long long *queries,
                               PlatformArray *links, IdentityCode *turned, RegisterProbe *probe)
{
    const unsigned long long longest = (1ULL << stall.bits) - 1ULL;
    const unsigned long long highest = (1ULL << group[0].bits) - 1ULL;
    probe->number = cases->number;
    probe->renamed = highest;
    if (!chain_code_read(engine, cases->number.text, &probe->code, links))
    {
        return 0;
    }
    const unsigned long long instructions = probe->code.count / 16ULL;
    for (unsigned long long index = 0ULL; index < instructions; index += 1ULL)
    {
        stall_bits_write(&probe->code, index, stall, longest);
    }
    if (!stall_alike(&probe->code, cases, s_walk_places, question, queries))
    {
        return 0;
    }
    probe->named = (unsigned long long *)platform_cell_taken(((instructions != 0ULL) ? (instructions * fields) : 1ULL) *
                                                                 sizeof(unsigned long long),
                                                             platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if (probe->named == NULL)
    {
        return 0;
    }
    for (unsigned long long index = 0ULL; index < instructions; index += 1ULL)
    {
        for (unsigned int field = 0u; field < fields; field += 1u)
        {
            probe->named[probe->named_count] = stall_bits(&probe->code, index, group[field]);
            probe->named_count += 1ULL;
        }
    }
    probe->named_count = identity_numbers_settled(probe->named, probe->named_count);
    unsigned long long lowest = 0ULL;
    while ((lowest < highest) && register_probe_names(probe, lowest))
    {
        lowest += 1ULL;
    }
    // The values the chain writes in the group's first field and reads after in another, in the order it writes
    // them. The walk tries each until one renamed to the lowest number the chain does not use still answers alike. A
    // value the chain writes and never reads again tests whether the code writes a number, and not whether it keeps
    // what the chain wrote
    for (unsigned long long index = 0ULL; (index < instructions) && (probe->renamed == highest); index += 1ULL)
    {
        const unsigned long long written = stall_bits(&probe->code, index, group[0]);
        int read_after = 0;
        for (unsigned long long after = index + 1ULL; after < instructions; after += 1ULL)
        {
            for (unsigned int at = 1u; at < fields; at += 1u)
            {
                read_after = read_after || (stall_bits(&probe->code, after, group[at]) == written);
            }
        }
        if ((written == highest) || (lowest == highest) || !read_after)
        {
            continue;
        }
        register_renamed(&probe->code, group, fields, written, lowest, turned);
        if (!stall_alike(turned, cases, s_walk_places, question, queries))
        {
            continue;
        }
        probe->renamed = written;
    }
    return probe->renamed != highest;
}

// The last value a question's code can name in a group of fields, queried of the part and walked down. A chain of ours
// the host also computes is a probe where, with every stall the longest, it answers alike with the host. It must still
// do so with a value it writes in the group's first field, and then reads in another. The value goes renamed in every
// field of the group to the lowest number those fields do not represent. The walk starts at the highest number the
// group's first field represents and steps down one at a time, past each number a probe uses.
//
// A number is truthy where every probe answers alike with its value renamed to it. It is falsy where one answers
// apart, the part refuses it, or the gate blocks it. The last value is the first truthy number, and every number under
// it is the code's to name. It goes to the .ksc as the part's answer on the run channel, `run answers <last> <answer>`,
// and the run's blob `blob` contains every number queried with its verdict. `token` is the character that prints the
// group's values, R for registers and P for predicates. The register walk and the predicate walk are this one walk
// over their own fields (P11 Step 9)
static int identity_last(const char *engine, const char *answers, const char *ksc, const char *folder,
                         const char *const *carrier, StallField stall, const StallField *group, const char *answer,
                         const char *blob, const char *mode, char token)
{
    const unsigned int fields = 3u;
    const unsigned long long highest = (1ULL << group[0].bits) - 1ULL;
    HostAnswers host;
    if (!host_answers_read(answers, 1, &host))
    {
        walk_said(mode, ": the pool cannot meet the host's answers", "", "");
        return 1;
    }
    if (!run_channel_open(carrier, 60000000ULL))
    {
        host_answers_ended(&host);
        return 1;
    }
    record_open(ksc, mode);
    // a chain of ours contains no probe place, and the cross reads it whole (measuring_stick_query.c)
    static RunQuestion s_question;
    s_question.place = RUN_PLACE_NONE;
    stall_cases(&s_question, s_walk_places);
    unsigned long long queries = 0ULL;
    static RegisterProbe s_probes[WALK_PROBES_MAX];
    unsigned int probe_count = 0u;
    IdentityCode turned;
    identity_code_begun(&turned, PLATFORM_TIER_TEMPORARY);
    PlatformArray links;
    platform_array_begun(&links, sizeof(Link), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    for (unsigned long long entry_at = 0ULL; (entry_at < host.count) && (probe_count < WALK_PROBES_MAX);
         entry_at += 1ULL)
    {
        RegisterProbe *const probe = &s_probes[probe_count];
        EMBED_CALL(memor.set, MemoriaCfg, .dst = probe, .bytes = sizeof(*probe), .val = 0);
        identity_code_begun(&probe->code, PLATFORM_TIER_TEMPORARY);
        if (!register_probe_read(engine, &host.chain[entry_at], stall, group, fields, &s_question, &queries, &links,
                                 &turned, probe))
        {
            register_probe_ended(probe);
            continue;
        }
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  probe ");
        mmgr_line_put(output_line, probe->number.text);
        mmgr_line_put(output_line, ": ");
        mmgr_line_ch(output_line, token);
        mmgr_line_decimal(output_line, probe->renamed, 1u);
        mmgr_line_put(output_line, " renamed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
        probe_count += 1u;
    }
    platform_array_ended(&links);
    if (probe_count == 0u)
    {
        walk_said(mode, ": no chain answers alike with a value renamed, and no probe", "", "");
        run_channel_close();
        platform_array_ended(&turned);
        host_answers_ended(&host);
        return 1;
    }
    RunWritten table;
    char blob_path[CARRIER_TEXT_MAX];
    VerbaLinea blob_line = mmgr_line_over(blob_path, sizeof(blob_path));
    mmgr_line_ch(&blob_line, '/');
    mmgr_line_put(&blob_line, blob);
    (void)mmgr_line_finish(&blob_line);
    run_written_begun(&table, identity_path_joined(folder, blob_path).text, 0);
    // a number is the walk's to query where no probe uses it
    const auto renamed_alike = [&](long long number) -> int {
        for (unsigned int at = 0u; at < probe_count; at += 1u)
        {
            if (register_probe_names(&s_probes[at], (unsigned long long)number))
            {
                return -1;
            }
        }
        int truthy = 1;
        for (unsigned int at = 0u; truthy && (at < probe_count); at += 1u)
        {
            const RegisterProbe *const probe = &s_probes[at];
            register_renamed(&probe->code, group, fields, probe->renamed, (unsigned long long)number, &turned);
            const HostCases *const probe_host = host_cases_of(&host, probe->number.text);
            truthy = (probe_host != NULL) && stall_alike(&turned, probe_host, s_walk_places, &s_question, &queries);
        }
        VerbaLinea *const line = &table.text.line;
        mmgr_line_ch(line, token);
        mmgr_line_decimal_signed(line, number);
        mmgr_line_put(line, " ");
        mmgr_line_put(line, truthy ? "truthy" : "falsy");
        mmgr_line_put(line, truthy ? "" : ": ");
        mmgr_line_put(line, truthy ? "" : s_question.refused);
        mmgr_line_ch(line, '\n');
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_ch(output_line, token);
        mmgr_line_decimal_signed(output_line, number);
        mmgr_line_put(output_line, ": ");
        mmgr_line_put(output_line, truthy ? "truthy" : "falsy");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
        return truthy;
    };
    const long long last = walk_step(renamed_alike, (long long)highest, -1, 0);
    for (unsigned int at = 0u; at < probe_count; at += 1u)
    {
        register_probe_ended(&s_probes[at]);
    }
    platform_array_ended(&turned);
    host_answers_ended(&host);
    const int closed = run_closed(&table);
    run_written_ended(&table);
    if (!closed)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "orior_klq_identity ");
        mmgr_line_put(output_line, mode);
        mmgr_line_put(output_line, ": ");
        mmgr_line_put(output_line, folder);
        mmgr_line_put(output_line, blob_path);
        mmgr_line_put(output_line, " could not be written");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
        run_channel_close();
        return 1;
    }
    run_channel_close();
    if (last < 0)
    {
        walk_said(mode, ": no number answers alike, and no last value", "", "");
        return 1;
    }
    char row[64];
    VerbaLinea row_line = mmgr_line_over(row, sizeof(row));
    mmgr_line_put(&row_line, "run answers ");
    mmgr_line_hex(&row_line, (unsigned long long)last, 8u);
    mmgr_line_put(&row_line, " ");
    mmgr_line_put(&row_line, answer);
    (void)mmgr_line_finish(&row_line);
    if (!ksc_answer_write(ksc, mode, row))
    {
        walk_said(mode, ": ", ksc, " could not be written");
        return 1;
    }
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "orior_klq_identity ");
    mmgr_line_put(output_line, mode);
    mmgr_line_put(output_line, ": the last ");
    mmgr_line_ch(output_line, token);
    mmgr_line_decimal_signed(output_line, last);
    mmgr_line_put(output_line, ", over ");
    mmgr_line_decimal(output_line, queries, 1u);
    mmgr_line_put(output_line, " queries, written to ");
    mmgr_line_put(output_line, ksc);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return 0;
}

// The fields `names` of the .ksc into `group`, and the stall field into `stall`: 1, or 0 with the reason printed where
// the .ksc gives one of them not
static int walk_fields(const char *ksc, const char *mode, const char *const names[3], const char *missing,
                       StallField *stall, StallField group[3])
{
    StallFields fields;
    stall_fields(ksc, &fields);
    const StallField *const stall_field = field_of(&fields, "stall");
    int whole = stall_field != NULL;
    for (unsigned int at = 0u; whole && (at < 3u); at += 1u)
    {
        const StallField *const field = field_of(&fields, names[at]);
        whole = field != NULL;
        group[at] = whole ? *field : group[at];
    }
    *stall = (stall_field != NULL) ? *stall_field : *stall;
    stall_fields_ended(&fields);
    if (!whole)
    {
        walk_said(mode, ": ", ksc, missing);
    }
    return whole;
}

// The last register a question's code can name, its register fields rd, ra and rb, rd the one a chain writes
// (identity_last). It goes to the .ksc as `run answers <last> register last`. R255 is RZ, and the walk excepts it where
// a rename of a register to it drops the chain's value and answers apart
int identity_register(const char *engine, const char *answers, const char *ksc, const char *folder,
                      const char *const *carrier)
{
    static const char *const s_names[3] = {"rd", "ra", "rb"};
    StallField stall = {0u, 0u};
    StallField registers[3] = {{0u, 0u}, {0u, 0u}, {0u, 0u}};
    if (!walk_fields(ksc, "register", s_names, " gives no stall field or not every register field", &stall, registers))
    {
        return 1;
    }
    return identity_last(engine, answers, ksc, folder, carrier, stall, registers, "register last", "register.txt",
                         "register", 'R');
}

// The last predicate a question's code can name, its predicate fields pg, the guard and pt (sass_nvidia.tsv). The
// chain writes pg, and reads the guard and pt after (identity_last). It goes to the .ksc as
// `run answers <last> predicate last`. P7 is PT, and the walk excepts it where a rename of a predicate to it answers
// always and apart from the chain's. The walk is the register walk over the predicate fields (P11 Step 9)
int identity_predicate(const char *engine, const char *answers, const char *ksc, const char *folder,
                       const char *const *carrier)
{
    static const char *const s_names[3] = {"pg", "guard", "pt"};
    StallField stall = {0u, 0u};
    StallField predicates[3] = {{0u, 0u}, {0u, 0u}, {0u, 0u}};
    if (!walk_fields(ksc, "predicate", s_names, " gives no stall field or not every predicate field", &stall,
                     predicates))
    {
        return 1;
    }
    return identity_last(engine, answers, ksc, folder, carrier, stall, predicates, "predicate last", "predicate.txt",
                         "predicate", 'P');
}

// How far a container's code reaches, queried of the part and walked (P11 Step 9, the fewest registers). A container
// that declares d registers gives code R0 through R(d - past) and no other. One past that faults the part with a
// register out of range (sass_machine.h, `register past`). A chain of ours the host also computes, with every stall
// the longest, names a highest register n. The walk puts it with d from n + 1 upward, one at a time, until the part
// answers alike with the host. Then past is that d less n.
//
// The walk queries up to three chains, and the answer recorded is the max any gives, the count that clears every
// chain's refusal. It goes to the .ksc as `run answers <past> register past`, and the run's blob past.txt contains
// every count queried with what came back
int identity_past(const char *engine, const char *answers, const char *ksc, const char *folder,
                  const char *const *carrier)
{
    static const char *const s_names[3] = {"rd", "ra", "rb"};
    StallField stall = {0u, 0u};
    StallField registers[3] = {{0u, 0u}, {0u, 0u}, {0u, 0u}};
    if (!walk_fields(ksc, "past", s_names, " gives no stall field or not every register field", &stall, registers))
    {
        return 1;
    }
    const unsigned long long longest = (1ULL << stall.bits) - 1ULL;
    // a register field is far below 32 bits
    const unsigned int highest = (unsigned int)((1ULL << registers[0].bits) - 1ULL);
    HostAnswers host;
    if (!host_answers_read(answers, 1, &host))
    {
        walk_said("past", ": the pool cannot meet the host's answers", "", "");
        return 1;
    }
    if (!run_channel_open(carrier, 60000000ULL))
    {
        host_answers_ended(&host);
        return 1;
    }
    record_open(ksc, "past");
    RunWritten table;
    run_written_begun(&table, identity_path_joined(folder, "/past.txt").text, 0);
    // a chain of ours contains no probe place, and the cross reads it whole (measuring_stick_query.c)
    static RunQuestion s_question;
    s_question.place = RUN_PLACE_NONE;
    stall_cases(&s_question, s_walk_places);
    unsigned long long queries = 0ULL;
    const unsigned long long max = 3ULL;
    unsigned long long tried = 0ULL;
    long long past = -1;
    IdentityCode code;
    identity_code_begun(&code, PLATFORM_TIER_TEMPORARY);
    PlatformArray links;
    platform_array_begun(&links, sizeof(Link), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    for (unsigned long long entry_at = 0ULL; entry_at < host.count; entry_at += 1ULL)
    {
        if (tried >= max)
        {
            break;
        }
        const HostCases *const entry = &host.chain[entry_at];
        if (!chain_code_read(engine, entry->number.text, &code, &links))
        {
            continue;
        }
        unsigned int named = 0u;
        for (unsigned long long index = 0ULL; index < (code.count / 16ULL); index += 1ULL)
        {
            stall_bits_write(&code, index, stall, longest);
            for (unsigned int field = 0u; field < 3u; field += 1u)
            {
                // a register number is below 2^32
                const unsigned int number = (unsigned int)stall_bits(&code, index, registers[field]);
                named = ((number != highest) && (number > named)) ? number : named;
            }
        }
        if (named == 0u)
        {
            continue;
        }
        tried += 1ULL;
        // the chain that declares `count` registers, untimed, in the carrier's own block
        const auto declaring_alike = [&](long long count) -> int {
            // a count of registers is below 2^32
            s_question.registers = (unsigned int)count;
            s_question.launches = 0u;
            const int alike = question_alike(&code, entry, s_walk_places, &s_question, &queries);
            VerbaLinea *const line = &table.text.line;
            mmgr_line_put(line, entry->number.text);
            mmgr_line_put(line, " names R");
            mmgr_line_decimal(line, named, 1u);
            mmgr_line_put(line, ", declaring ");
            mmgr_line_decimal_signed(line, count);
            mmgr_line_put(line, ": ");
            mmgr_line_put(line, alike ? "alike" : "apart");
            mmgr_line_put(line, alike ? "" : ", ");
            mmgr_line_put(line, alike ? "" : s_question.refused);
            mmgr_line_ch(line, '\n');
            return alike;
        };
        const long long found = walk_step(declaring_alike, (long long)named + 1, 1, (long long)highest);
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  chain ");
        mmgr_line_put(output_line, entry->number.text);
        mmgr_line_put(output_line, " names R");
        mmgr_line_decimal(output_line, named, 1u);
        if (found > (long long)highest)
        {
            mmgr_line_put(output_line, ", and answers alike declaring no count");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
            continue;
        }
        mmgr_line_put(output_line, " and answers alike declaring ");
        mmgr_line_decimal_signed(output_line, found);
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal_signed(output_line, found - (long long)named);
        mmgr_line_put(output_line, " past");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
        past = ((found - (long long)named) > past) ? (found - (long long)named) : past;
    }
    platform_array_ended(&code);
    platform_array_ended(&links);
    host_answers_ended(&host);
    if (!run_closed(&table))
    {
        walk_said("past", ": ", folder, "/past.txt could not be written");
    }
    run_written_ended(&table);
    run_channel_close();
    if (past < 0)
    {
        walk_said("past", ": no chain answers alike declaring any count, and no answer", "", "");
        return 1;
    }
    char row[64];
    VerbaLinea row_line = mmgr_line_over(row, sizeof(row));
    mmgr_line_put(&row_line, "run answers ");
    mmgr_line_hex(&row_line, (unsigned long long)past, 8u);
    mmgr_line_put(&row_line, " register past");
    (void)mmgr_line_finish(&row_line);
    if (!ksc_answer_write(ksc, "past", row))
    {
        walk_said("past", ": ", ksc, " could not be written");
        return 1;
    }
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "orior_klq_identity past: a container's code reaches ");
    mmgr_line_decimal_signed(output_line, past);
    mmgr_line_put(output_line, " past the highest register it names, over ");
    mmgr_line_decimal(output_line, queries, 1u);
    mmgr_line_put(output_line, " queries, written to ");
    mmgr_line_put(output_line, ksc);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return 0;
}
