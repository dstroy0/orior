// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// walk.cu: the one walk over numbers a relation is true at: the last register, the last predicate, and how far a
// container's code reaches
#include "identity_internal.h"

// one question the register walk puts: a chain of ours, its code with every stall the longest, the register it
// writes that the walk renames, and every number its register fields represent
// The one walk over numbers a relation is true at on one side of a bound and not past it: the last register, the fewest
// registers and each knee are read by it. `predicate` answers 1 where the relation is true at a number, 0 where it does
// not, and -1 at a number that is not the relation's to answer, which the walk passes over. Stepping: from `from` by
// `step` as far as `end`, the first number the relation is true at, or `end` + `step` where it is true at none
long long walk_step(const std::function<int(long long)> &predicate, long long from, long long step, long long end)
{
    for (long long number = from; number != (end + step); number += step)
    {
        if (predicate(number) == 1)
        {
            return number;
        }
    }
    return end + step;
}

// Halving: `truthy` a number the relation is true at and `falsy` one it does not, on either side of it, each moved to
// the middle the relation gives the same answer as until the two are adjacent. The truthy end, the last number the
// relation is true at before the bound
long long walk_halved(const std::function<int(long long)> &predicate, long long truthy, long long falsy)
{
    while (((truthy - falsy) > 1) || ((falsy - truthy) > 1))
    {
        const long long middle = truthy + ((falsy - truthy) / 2);
        if (predicate(middle) == 1)
        {
            truthy = middle;
        }
        else
        {
            falsy = middle;
        }
    }
    return truthy;
}

// `code` with every register field that contains `from` set to `to`
static std::vector<unsigned char> register_renamed(const std::vector<unsigned char> &code,
                                                   const std::vector<StallField> &registers, unsigned long long from,
                                                   unsigned long long to)
{
    std::vector<unsigned char> turned = code;
    for (size_t index = 0u; index < (code.size() / 16u); index += 1u)
    {
        for (const StallField &field : registers)
        {
            if (stall_bits(code, index, field) == from)
            {
                stall_bits_write(&turned, index, field, to);
            }
        }
    }
    return turned;
}

// The last value a question's code can name in a group of fields, queried of the part and walked down. A chain of ours
// the host also computes is a probe where, with every stall the longest, it answers alike with the host, and still
// does with a value it writes in the group's first field and then reads in another renamed, in every field of the
// group, to the lowest number those fields do not represent. The walk starts at the highest number the group's first
// field represents and steps down one at a time, skipping a number a probe uses: a number is truthy where every probe
// answers alike with its value renamed to it, and falsy where one answers apart, is refused, or is blocked by the gate.
// The last value is the first truthy number, and every number under it is the code's to name. It is written to the
// .ksc as the part's answer on the run channel, `run answers <last> <answer>`, and the run's blob `blob` contains every
// number queried with its verdict. `token` is the letter the group's values are printed with, R for registers and P
// for predicates. The register walk and the predicate walk are this one walk over their own fields (P11 Step 9)
static int identity_last(const char *engine, const char *answers, const char *ksc, const std::string &folder,
                         const char *const *carrier, const StallField &stall,
                         const std::vector<StallField> &group, const char *answer, const char *blob,
                         const char *mode, char token)
{
    const unsigned long long longest = (1ull << stall.bits) - 1ull;
    const unsigned long long highest = (1ull << group[0].bits) - 1ull;
    std::map<std::string, std::vector<std::string>> host = host_answers_read(answers);
    if (!run_channel_open(carrier, 60000000ull))
    {
        return 1;
    }
    record_open(ksc, mode);
    // a chain of ours contains no probe place, and the cross reads it whole (measuring_stick_query.c)
    static RunQuestion s_question;
    s_question.place = RUN_PLACE_NONE;
    std::vector<unsigned int> places;
    stall_cases(&s_question, &places);
    unsigned long long queries = 0ull;
    // the probes the walk is put over at most, each from a chain of its own
    const size_t most = 3u;
    std::vector<RegisterProbe> probes;
    for (const auto &entry : host)
    {
        if (probes.size() >= most)
        {
            break;
        }
        std::vector<unsigned char> code;
        std::vector<Link> links;
        if (!chain_code_read(engine, entry.first, &code, &links))
        {
            continue;
        }
        for (size_t index = 0u; index < (code.size() / 16u); index += 1u)
        {
            stall_bits_write(&code, index, stall, longest);
        }
        if (!stall_alike(code, entry.second, places, &s_question, &queries))
        {
            continue;
        }
        RegisterProbe probe{entry.first, code, highest, {}};
        for (size_t index = 0u; index < (code.size() / 16u); index += 1u)
        {
            for (const StallField &field : group)
            {
                probe.named.insert(stall_bits(code, index, field));
            }
        }
        unsigned long long lowest = 0ull;
        while ((lowest < highest) && (probe.named.count(lowest) != 0u))
        {
            lowest += 1ull;
        }
        // the values the chain writes in the group's first field and reads after in another, in the order it writes
        // them, each tried until one renamed to the lowest number the chain does not use still answers alike: a value
        // written and never read again tests whether a number is written, and not whether it retains what is written
        for (size_t index = 0u; (index < (code.size() / 16u)) && (probe.renamed == highest); index += 1u)
        {
            const unsigned long long written = stall_bits(code, index, group[0]);
            int read_after = 0;
            for (size_t after = index + 1u; after < (code.size() / 16u); after += 1u)
            {
                for (size_t at = 1u; at < group.size(); at += 1u)
                {
                    read_after = read_after || (stall_bits(code, after, group[at]) == written);
                }
            }
            if ((written == highest) || (lowest == highest) || !read_after ||
                !stall_alike(register_renamed(code, group, written, lowest), entry.second, places, &s_question,
                             &queries))
            {
                continue;
            }
            probe.renamed = written;
        }
        if (probe.renamed != highest)
        {
            printf("  probe %s: %c%llu renamed\n", entry.first.c_str(), token, probe.renamed);
            probes.push_back(probe);
        }
    }
    if (probes.empty())
    {
        printf("orior_klq_identity %s: no chain answers alike with a value renamed, and no probe\n", mode);
        run_channel_close();
        return 1;
    }
    RunWritten table = {(folder + "/" + blob), 0, std::string()};
    // a number is the walk's to query where no probe uses it
    const auto renamed_alike = [&](long long number) -> int {
        const int probed = std::any_of(probes.begin(), probes.end(), [&](const RegisterProbe &probe) {
            return probe.named.count((unsigned long long)number) != 0u;
        });
        if (probed)
        {
            return -1;
        }
        int truthy = 1;
        for (const RegisterProbe &probe : probes)
        {
            truthy =
                truthy && stall_alike(register_renamed(probe.code, group, probe.renamed, (unsigned long long)number),
                                      host[probe.number], places, &s_question, &queries);
        }
        run_print(&table, "%c%lld %s%s%s\n", token, number, truthy ? "truthy" : "falsy", truthy ? "" : ": ",
                  truthy ? "" : s_question.refused);
        printf("  %c%lld: %s\n", token, number, truthy ? "truthy" : "falsy");
        return truthy;
    };
    const long long last = walk_step(renamed_alike, (long long)highest, -1, 0);
    if (!run_closed(&table))
    {
        printf("orior_klq_identity %s: %s/%s could not be written\n", mode, folder.c_str(), blob);
        run_channel_close();
        return 1;
    }
    run_channel_close();
    if (last < 0)
    {
        printf("orior_klq_identity %s: no number answers alike, and no last value\n", mode);
        return 1;
    }
    char row[64];
    snprintf(row, sizeof(row), "run answers %08llx %s", (unsigned long long)last, answer);
    if (!ksc_answers_write(ksc, mode, std::vector<std::string>{row}))
    {
        printf("orior_klq_identity %s: %s could not be written\n", mode, ksc);
        return 1;
    }
    printf("orior_klq_identity %s: the last %c%lld, over %llu queries, written to %s\n", mode, token, last, queries,
           ksc);
    return 0;
}

// The last register a question's code can name, its register fields rd, ra and rb, rd the one a chain writes
// (identity_last): written to the .ksc as `run answers <last> register last`, R255 being RZ and excepted where
// renaming a register to it drops the chain's value and answers apart
int identity_register(const char *engine, const char *answers, const char *ksc, const std::string &folder,
                             const char *const *carrier)
{
    std::map<std::string, StallField> fields = stall_fields(ksc);
    if ((fields.count("stall") == 0u) || (fields.count("rd") == 0u) || (fields.count("ra") == 0u) ||
        (fields.count("rb") == 0u))
    {
        printf("orior_klq_identity register: %s gives no stall field or not every register field\n", ksc);
        return 1;
    }
    const std::vector<StallField> registers = {fields["rd"], fields["ra"], fields["rb"]};
    return identity_last(engine, answers, ksc, folder, carrier, fields["stall"], registers, "register last",
                         "register.txt", "register", 'R');
}

// The last predicate a question's code can name, its predicate fields pg, the guard and pt (sass_nvidia.tsv), pg the
// one a chain writes and the guard and pt those it reads after (identity_last): written to the .ksc as
// `run answers <last> predicate last`, P7 being PT and excepted where renaming a predicate to it answers always and
// apart from the chain's. The walk is the register walk over the predicate fields (P11 Step 9)
int identity_predicate(const char *engine, const char *answers, const char *ksc, const std::string &folder,
                              const char *const *carrier)
{
    std::map<std::string, StallField> fields = stall_fields(ksc);
    if ((fields.count("stall") == 0u) || (fields.count("pg") == 0u) || (fields.count("guard") == 0u) ||
        (fields.count("pt") == 0u))
    {
        printf("orior_klq_identity predicate: %s gives no stall field or not every predicate field\n", ksc);
        return 1;
    }
    const std::vector<StallField> predicates = {fields["pg"], fields["guard"], fields["pt"]};
    return identity_last(engine, answers, ksc, folder, carrier, fields["stall"], predicates, "predicate last",
                         "predicate.txt", "predicate", 'P');
}

// How far a container's code reaches, queried of the part and walked (P11 Step 9, the fewest registers): a container
// declaring d registers gives code R0 through R(d - past) and no other, and one past that faults the part with a
// register out of range (sass_machine.h, `register past`). A chain of ours the host also computes, with every stall
// the longest, names a highest register n; it is put declaring d from n + 1 upward, one at a time, until the part
// answers alike with the host, and past is that d less n. Up to three chains are queried, and the answer recorded is
// the most any gives, the count that clears every chain's refusal. Written to the .ksc as `run answers <past> register
// past`, and the run's blob past.txt contains every count queried with what came back
int identity_past(const char *engine, const char *answers, const char *ksc, const std::string &folder,
                         const char *const *carrier)
{
    std::map<std::string, StallField> fields = stall_fields(ksc);
    if ((fields.count("stall") == 0u) || (fields.count("rd") == 0u) || (fields.count("ra") == 0u) ||
        (fields.count("rb") == 0u))
    {
        printf("orior_klq_identity past: %s gives no stall field or not every register field\n", ksc);
        return 1;
    }
    const StallField stall = fields["stall"];
    const std::vector<StallField> registers = {fields["rd"], fields["ra"], fields["rb"]};
    const unsigned long long longest = (1ull << stall.bits) - 1ull;
    const unsigned int highest = (unsigned int)((1ull << fields["rd"].bits) - 1ull);
    std::map<std::string, std::vector<std::string>> host = host_answers_read(answers);
    if (!run_channel_open(carrier, 60000000ull))
    {
        return 1;
    }
    record_open(ksc, "past");
    RunWritten table = {(folder + "/past.txt"), 0, std::string()};
    // a chain of ours contains no probe place, and the cross reads it whole (measuring_stick_query.c)
    static RunQuestion s_question;
    s_question.place = RUN_PLACE_NONE;
    std::vector<unsigned int> places;
    stall_cases(&s_question, &places);
    unsigned long long queries = 0ull;
    const size_t most = 3u;
    size_t tried = 0u;
    long long past = -1;
    for (const auto &entry : host)
    {
        if (tried >= most)
        {
            break;
        }
        std::vector<unsigned char> code;
        std::vector<Link> links;
        if (!chain_code_read(engine, entry.first, &code, &links))
        {
            continue;
        }
        unsigned int named = 0u;
        for (size_t index = 0u; index < (code.size() / 16u); index += 1u)
        {
            stall_bits_write(&code, index, stall, longest);
            for (const StallField &field : registers)
            {
                const unsigned int number = (unsigned int)stall_bits(code, index, field);
                named = ((number != highest) && (number > named)) ? number : named;
            }
        }
        if (named == 0u)
        {
            continue;
        }
        tried += 1u;
        // the chain declaring `count` registers, untimed, in the carrier's own block
        const auto declaring_alike = [&](long long count) -> int {
            s_question.registers = (unsigned int)count;
            s_question.launches = 0u;
            const int alike = question_alike(code, entry.second, places, &s_question, &queries);
            run_print(&table, "%s names R%u, declaring %lld: %s%s%s\n", entry.first.c_str(), named, count,
                      alike ? "alike" : "apart", alike ? "" : ", ", alike ? "" : s_question.refused);
            return alike;
        };
        const long long found = walk_step(declaring_alike, (long long)named + 1, 1, (long long)highest);
        if (found > (long long)highest)
        {
            printf("  chain %s names R%u, and answers alike declaring no count\n", entry.first.c_str(), named);
            continue;
        }
        printf("  chain %s names R%u and answers alike declaring %lld: %lld past\n", entry.first.c_str(), named, found,
               found - (long long)named);
        past = std::max(past, found - (long long)named);
    }
    if (!run_closed(&table))
    {
        printf("orior_klq_identity past: %s/past.txt could not be written\n", folder.c_str());
    }
    run_channel_close();
    if (past < 0)
    {
        printf("orior_klq_identity past: no chain answers alike declaring any count, and no answer\n");
        return 1;
    }
    char row[64];
    snprintf(row, sizeof(row), "run answers %08llx register past", (unsigned long long)past);
    if (!ksc_answers_write(ksc, "past", std::vector<std::string>{row}))
    {
        printf("orior_klq_identity past: %s could not be written\n", ksc);
        return 1;
    }
    printf("orior_klq_identity past: a container's code reaches %lld past the highest register it names, over %llu "
           "queries, "
           "written to %s\n",
           past, queries, ksc);
    return 0;
}
