// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// fixed.cu: the fixed registers asked of the part: the slot of the kernel the system accepted, the register that reads
// 0 whatever is written to it, and what each register holds at the slot (The fixed registers asked, Open 6.2)
#include "fixed_read.h"
#include "identity_internal.h"

// the place the least opening begins at, the part's answer in its .ksc, and 0 where none stands
static unsigned int fixed_opening_read(const char *ksc)
{
    KscFile file = {};
    static KscRow s_row;
    unsigned int from = 0u;
    ksc_read(&file, ksc);
    for (unsigned int at = 0u; at < file.lines; at += 1u)
    {
        if (ksc_row_read(file.line[at], "run", &s_row) && (strcmp(s_row.answer, "answers") == 0) &&
            (strcmp(s_row.question, "opening least") == 0))
        {
            // the row's word is a place, at most eight hex digits
            from = (unsigned int)s_row.value;
        }
    }
    ksc_free(&file);
    return from;
}

// the two words of place `place` of `kernel`, low then high
static void place_words(const std::vector<unsigned char> &kernel, unsigned int place, unsigned long long *low,
                        unsigned long long *high)
{
    *low = 0ull;
    *high = 0ull;
    for (unsigned int byte = 0u; byte < 8u; byte += 1u)
    {
        *low |= (unsigned long long)kernel[(place * 16u) + byte] << (8u * byte);
        *high |= (unsigned long long)kernel[(place * 16u) + 8u + byte] << (8u * byte);
    }
}

// `kernel` with place `place` holding the words `low` and `high`
static std::vector<unsigned char> place_put(const std::vector<unsigned char> &kernel, unsigned int place,
                                            unsigned long long low, unsigned long long high)
{
    std::vector<unsigned char> variant(kernel);
    for (unsigned int byte = 0u; byte < 8u; byte += 1u)
    {
        variant[(place * 16u) + byte] = (unsigned char)((low >> (8u * byte)) & 0xffull);
        variant[(place * 16u) + 8u + byte] = (unsigned char)((high >> (8u * byte)) & 0xffull);
    }
    return variant;
}

// what the mode holds while it asks: the cases' host places, the question, the asks counted, the blob fixed.txt, and
// whether the run is dry
struct FixedAsk
{
    std::vector<unsigned int> places;
    RunQuestion *question;
    unsigned long long asks;
    RunWritten table;
    int dry;
};

// 1 where the part answers `variant` alike with `reference` on every case; the ask is a line of fixed.txt named by
// `what`, and a dry run is noted from the carrier's refusal
static int fixed_alike(FixedAsk *ask, const std::vector<unsigned char> &variant,
                       const std::vector<std::string> &reference, const char *what)
{
    const int alike = question_alike(variant, reference, ask->places, ask->question, &ask->asks);
    ask->dry = ask->dry || (strncmp(ask->question->refused, "skipped dry", 11u) == 0);
    const char *const why = (ask->question->outcome == RUN_ANSWERED) ? "apart" : ask->question->refused;
    run_print(&ask->table, "%s %s%s%s\n", what, alike ? "truthy" : "falsy", alike ? "" : ": ", alike ? "" : why);
    printf("  %s: %s\n", what, alike ? "truthy" : "falsy");
    return alike;
}

// the blob handed to the run and the channel closed, as every way out does
static void fixed_closed(FixedAsk *ask)
{
    if (!run_closed(&ask->table))
    {
        printf("klq_identity fixed: %s could not be written\n", ask->table.path.c_str());
    }
    run_channel_close();
}

// The fixed registers asked (Open 6.2), over the kernel the system accepted, read whole from the run's kernel.txt as
// the frame reads it. The slot: each place from the opening's beginning is put holding the encoding the unreached
// places hold, the last place's, and the first whose kernel answers the case's second word on every case is the slot,
// nothing then writing the register the kernel stores: `run answers <s> slot place`. Zero: the slot's own instruction
// is put writing register n and the closing's store, the first place after the slot reading the register the slot
// writes, reading n, for n the top of the register field then each register the kernel holds; the first that answers
// 0 on every case whatever was written reads 0: `run answers <n> fixed zero`. What each register holds: for each
// register n the kernel holds, the slot's instruction is put with n as its first source and zero as its second, and the
// stored answer is n's value, read against the thread's number and the case's words (fixed_read.h): `run answers <n>
// fixed lane_number`, `fixed record`, `fixed index`, `case word 0` and `case word 1`, each kind the lowest register
// holding it. Each ask and each reading is a line of the run's blob fixed.txt. 0 where the rows are written or the
// run is dry, 1 otherwise with the reason printed
int identity_fixed(const char *engine, const char *answers, const char *ksc, const std::string &folder,
                   const char *const *carrier)
{
    (void)engine;
    (void)answers;
    std::vector<unsigned char> kernel;
    unsigned int registers = 0u;
    if (!frame_kernel_read("fixed", &kernel, &registers))
    {
        return 1;
    }
    const unsigned int places = (unsigned int)(kernel.size() / 16u);
    std::map<std::string, StallField> fields = stall_fields(ksc);
    if ((fields.count("rd") == 0u) || (fields.count("ra") == 0u) || (fields.count("rb") == 0u))
    {
        printf("klq_identity fixed: %s gives not every register field\n", ksc);
        return 1;
    }
    const StallField rd = fields["rd"];
    const StallField ra = fields["ra"];
    const StallField rb = fields["rb"];
    const unsigned long long top = (1ull << rd.bits) - 1ull;
    const unsigned int from = fixed_opening_read(ksc);
    if (!run_channel_open(carrier, 60000000ull))
    {
        return 1;
    }
    record_open(ksc, "fixed");
    static RunQuestion s_question;
    s_question.slot = RUN_SLOT_NONE;
    s_question.registers = registers;
    FixedAsk ask = {std::vector<unsigned int>(), &s_question, 0ull, {(folder + "/fixed.txt"), 0, std::string()}, 0};
    stall_cases(&s_question, &ask.places);
    // the slot
    unsigned long long unreached_low = 0ull;
    unsigned long long unreached_high = 0ull;
    place_words(kernel, places - 1u, &unreached_low, &unreached_high);
    std::vector<std::string> second(IDENTITY_CASES, "-");
    for (unsigned int at = 0u; at < s_question.cases; at += 1u)
    {
        char word[24];
        snprintf(word, sizeof(word), "%x", s_question.word[at][1]);
        second[ask.places[at]] = word;
    }
    unsigned int slot = places;
    for (unsigned int place = from; (place < places) && (slot == places) && !ask.dry; place += 1u)
    {
        unsigned long long low = 0ull;
        unsigned long long high = 0ull;
        place_words(kernel, place, &low, &high);
        if ((low == unreached_low) && (high == unreached_high))
        {
            continue;
        }
        char what[32];
        snprintf(what, sizeof(what), "slot %u", place);
        if (fixed_alike(&ask, place_put(kernel, place, unreached_low, unreached_high), second, what))
        {
            slot = place;
        }
    }
    if (ask.dry)
    {
        fixed_closed(&ask);
        printf("klq_identity fixed: the run is dry, and nothing is written\n");
        return 0;
    }
    if (slot == places)
    {
        fixed_closed(&ask);
        printf("klq_identity fixed: no place answers the case's second word, and nothing is written\n");
        return 1;
    }
    char row[64];
    snprintf(row, sizeof(row), "run answers %08x slot place", slot);
    if (!ksc_answers_write(ksc, "slot", std::vector<std::string>{row}))
    {
        fixed_closed(&ask);
        printf("klq_identity fixed: %s could not be written\n", ksc);
        return 1;
    }
    printf("  the slot is place %u of %u\n", slot, places);
    // the closing's store: the first place after the slot reading the register the slot writes, through ra or rb
    const unsigned long long written = stall_bits(kernel, slot, rd);
    unsigned int store = places;
    StallField read = ra;
    for (unsigned int place = slot + 1u; (place < places) && (store == places); place += 1u)
    {
        if (stall_bits(kernel, place, ra) == written)
        {
            store = place;
            read = ra;
        }
        else if (stall_bits(kernel, place, rb) == written)
        {
            store = place;
            read = rb;
        }
    }
    if (store == places)
    {
        fixed_closed(&ask);
        printf("klq_identity fixed: no place after the slot reads the register it writes, and nothing more is "
               "written\n");
        return 1;
    }
    // zero
    const std::vector<std::string> zeros(IDENTITY_CASES, "0");
    std::vector<unsigned long long> candidates = {top};
    for (unsigned int number = 0u; number < registers; number += 1u)
    {
        candidates.push_back(number);
    }
    unsigned long long zero = top + 1ull;
    for (const unsigned long long candidate : candidates)
    {
        if (zero != (top + 1ull))
        {
            break;
        }
        std::vector<unsigned char> variant(kernel);
        stall_bits_write(&variant, slot, rd, candidate);
        stall_bits_write(&variant, store, read, candidate);
        char what[32];
        snprintf(what, sizeof(what), "zero %llu", candidate);
        if (fixed_alike(&ask, variant, zeros, what))
        {
            zero = candidate;
        }
    }
    if (zero == (top + 1ull))
    {
        fixed_closed(&ask);
        printf("klq_identity fixed: no register reads 0 whatever is written to it, and nothing more is written\n");
        return 1;
    }
    snprintf(row, sizeof(row), "run answers %08llx fixed zero", zero);
    std::vector<std::string> fixed_rows = {row};
    std::vector<std::string> case_rows;
    printf("  zero is register %llu\n", zero);
    // what each register holds at the slot, each kind's row the lowest register holding it
    std::vector<unsigned int> word_0(s_question.cases);
    std::vector<unsigned int> word_1(s_question.cases);
    for (unsigned int at = 0u; at < s_question.cases; at += 1u)
    {
        word_0[at] = s_question.word[at][0];
        word_1[at] = s_question.word[at][1];
    }
    std::set<FixedKind> given;
    for (unsigned int number = 0u; number < registers; number += 1u)
    {
        std::vector<unsigned char> variant(kernel);
        stall_bits_write(&variant, slot, ra, number);
        stall_bits_write(&variant, slot, rb, zero);
        if (!question_read(variant, &s_question, &ask.asks))
        {
            run_print(&ask.table, "register %u refused: %s\n", number, s_question.refused);
            printf("  register %u: refused\n", number);
            continue;
        }
        const FixedKind kind = fixed_kind(s_question.answered, word_0.data(), word_1.data(), s_question.cases);
        const char *const question = fixed_kind_question(kind);
        run_print(&ask.table, "register %u holds %s\n", number, (kind == FIXED_NOTHING) ? "nothing fixed" : question);
        printf("  register %u: %s\n", number, (kind == FIXED_NOTHING) ? "nothing fixed" : question);
        if ((kind == FIXED_NOTHING) || (given.count(kind) != 0u))
        {
            continue;
        }
        given.insert(kind);
        snprintf(row, sizeof(row), "run answers %08x %s", number, question);
        ((strncmp(question, "fixed", 5u) == 0) ? fixed_rows : case_rows).push_back(row);
    }
    fixed_closed(&ask);
    if (!ksc_answers_write(ksc, "fixed", fixed_rows) || !ksc_answers_write(ksc, "case word", case_rows))
    {
        printf("klq_identity fixed: %s could not be written\n", ksc);
        return 1;
    }
    printf("klq_identity fixed: the slot at place %u of %u, zero register %llu, %zu readings fixed at the slot, over "
           "%llu asks, written to %s\n",
           slot, places, zero, given.size(), ask.asks, ksc);
    return 0;
}
