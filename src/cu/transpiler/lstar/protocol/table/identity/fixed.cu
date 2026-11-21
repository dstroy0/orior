// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// fixed.cu: the fixed registers queried of the part: the probe place of the kernel the system accepted, the register
// that reads 0 whatever is written to it, and what each register contains at the probe place (Open 6.2)
#include "fixed_read.h"
#include "../../../../../../platform/platform.h"
#include "cellularum_laboro/cellularum_linea.h"
#include "identity_internal.h"

// the place the least opening begins at, the part's answer in its .ksc, and 0 where none is
static unsigned int fixed_opening_read(const char *ksc)
{
    KscFile file = {};
    static KscRow s_row;
    unsigned int from = 0u;
    ksc_read(&file, ksc);
    for (unsigned int at = 0u; at < file.lines; at += 1u)
    {
        if (ksc_row_read(file.line[at], "run", &s_row) &&
            (mmgr_text_equal(s_row.answer, "answers", PLATFORM_TEXT_TO_TERMINATOR)) &&
            (mmgr_text_equal(s_row.question, "opening least", PLATFORM_TEXT_TO_TERMINATOR)))
        {
            // the row's word is a place, at max eight hex digits
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
    *low = 0ULL;
    *high = 0ULL;
    for (unsigned int byte = 0u; byte < 8u; byte += 1u)
    {
        *low |= (unsigned long long)kernel[(place * 16u) + byte] << (8u * byte);
        *high |= (unsigned long long)kernel[(place * 16u) + 8u + byte] << (8u * byte);
    }
}

// `kernel` with place `place` containing the words `low` and `high`
static std::vector<unsigned char> place_put(const std::vector<unsigned char> &kernel, unsigned int place,
                                            unsigned long long low, unsigned long long high)
{
    std::vector<unsigned char> variant(kernel);
    for (unsigned int byte = 0u; byte < 8u; byte += 1u)
    {
        variant[(place * 16u) + byte] = (unsigned char)((low >> (8u * byte)) & 0xffULL);
        variant[(place * 16u) + 8u + byte] = (unsigned char)((high >> (8u * byte)) & 0xffULL);
    }
    return variant;
}

// the yield field as the part's .ksc gives it, none where it gives no row
static StallField s_yield = {0u, 0u};

// place `place` of `variant` put to stall the longest with its yield bit clear: the gate lets an instruction no form
// passes through with the longest stall alone, and the vendor's reader takes the longest stall with the yield bit
// clear alone, the two bits together an encoding it calls illegal
static void place_stalled_longest(std::vector<unsigned char> *variant, unsigned int place, const StallField &stall,
                                  unsigned long long longest)
{
    stall_bits_write(variant, place, stall, longest);
    stall_bits_write(variant, place, s_yield, 0ULL);
}

// what the mode records while it queries: the cases' host places, the question, the queries counted, the blob
// fixed.txt, and whether the run is dry
struct FixedQuery
{
    std::vector<unsigned int> places;
    RunQuestion *question;
    unsigned long long queries;
    RunWritten table;
    int dry;
};

// 1 where the part answers `variant` alike with `reference` on every case. The query is a line of fixed.txt named by
// `what`, and a dry run is noted from the carrier's refusal
static int fixed_alike(FixedQuery *query, const std::vector<unsigned char> &variant,
                       const std::vector<std::string> &reference, const char *what)
{
    const int alike = question_alike(variant, reference, query->places, query->question, &query->queries);
    query->dry = query->dry || (mmgr_text_starts(query->question->refused, "skipped dry", PLATFORM_TEXT_TO_TERMINATOR));
    const char *const why = (query->question->outcome == RUN_ANSWERED) ? "apart" : query->question->refused;
    run_print(&query->table, "%s %s%s%s\n", what, alike ? "truthy" : "falsy", alike ? "" : ": ", alike ? "" : why);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, what);
        mmgr_line_put(output_line, ": ");
        mmgr_line_put(output_line, alike ? "truthy" : "falsy");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return alike;
}

// the blob passed to the run and the channel closed, as every way out does
static void fixed_closed(FixedQuery *query)
{
    if (!run_closed(&query->table))
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "orior_klq_identity fixed: ");
        mmgr_line_put(output_line, query->table.path.c_str());
        mmgr_line_put(output_line, " could not be written");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    run_channel_close();
}

// The fixed registers queried (Open 6.2), over the kernel the system accepted, read whole from the run's kernel blob
// (kernel_blob.h) as the frame reads it. The probe place: each place from the opening's beginning is put containing the
// encoding the unreached places contain, the last place's, and the first whose kernel answers the case's second word on
// every case is the probe place, nothing then writing the register the kernel stores: `run answers <s> probe place`.
// Zero: the probe place's own instruction is put writing the top of the register field, and the kernel's own closing
// stores, which read that register, read it back in the record's second word; the top reads 0 where that word is 0 on
// every case whatever the probe place wrote: `run answers <n> fixed zero`. No other register is queried for it: a
// question changes one place no form of the machine contains, the gate's bound, and the kernel's store is matched by no
// form once changed. What each register contains: for each register n the kernel uses, the probe place's instruction is
// put with n as its first source and zero as its second, and the stored answer is n's value, read against the thread's
// number and the case's words (fixed_read.h): `run answers <n> fixed lane_number`, `fixed record`, `fixed index`, `case
// word 0` and `case word 1`, each type the lowest register containing it. The place a variant changes is put with its
// stall the longest and its yield bit clear, the one way the gate lets an instruction no form contains through and the
// vendor's reader takes it, as the walks' chains are put. Each query and each reading is a line of the run's blob
// fixed.txt. 0 where the rows are written or the run is dry, 1 otherwise with the reason printed
int identity_fixed(const char *engine, const char *answers, const char *ksc, const std::string &folder,
                   const char *const *carrier)
{
    (void)engine;
    (void)answers;
    const unsigned char *code = NULL;
    unsigned int places = 0u;
    unsigned int registers = 0u;
    if (!frame_kernel_read(&code, &places, &registers))
    {
        return 1;
    }
    std::vector<unsigned char> kernel(code, code + (16u * (size_t)places));
    std::map<std::string, StallField> fields = stall_fields(ksc);
    if ((fields.count("stall") == 0u) || (fields.count("rd") == 0u) || (fields.count("ra") == 0u) ||
        (fields.count("rb") == 0u))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "orior_klq_identity fixed: ");
            mmgr_line_put(output_line, ksc);
            mmgr_line_put(output_line, " gives no stall field or not every register field");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    const StallField stall = fields["stall"];
    const StallField rd = fields["rd"];
    const StallField ra = fields["ra"];
    const StallField rb = fields["rb"];
    s_yield = (fields.count("yield") != 0u) ? fields["yield"] : StallField{0u, 0u};
    const unsigned long long longest = (1ULL << stall.bits) - 1ULL;
    const unsigned long long top = (1ULL << rd.bits) - 1ULL;
    const unsigned int from = fixed_opening_read(ksc);
    if (!run_channel_open(carrier, 60000000ULL))
    {
        return 1;
    }
    record_open(ksc, "fixed");
    static RunQuestion s_question;
    s_question.place = RUN_PLACE_NONE;
    s_question.registers = registers;
    FixedQuery query = {std::vector<unsigned int>(), &s_question, 0ULL, {(folder + "/fixed.txt"), 0, std::string()}, 0};
    stall_cases(&s_question, &query.places);
    // the probe place
    unsigned long long unreached_low = 0ULL;
    unsigned long long unreached_high = 0ULL;
    place_words(kernel, places - 1u, &unreached_low, &unreached_high);
    std::vector<std::string> second(IDENTITY_CASES, "-");
    for (unsigned int at = 0u; at < s_question.cases; at += 1u)
    {
        char word[24];
        {
            VerbaLinea buffer_line = mmgr_line_over(word, sizeof(word));
            mmgr_line_hex(&buffer_line, s_question.word[at][1], 1u);
            (void)mmgr_line_finish(&buffer_line);
        }
        second[query.places[at]] = word;
    }
    unsigned int probe_place = places;
    for (unsigned int place = from; (place < places) && (probe_place == places) && !query.dry; place += 1u)
    {
        unsigned long long low = 0ULL;
        unsigned long long high = 0ULL;
        place_words(kernel, place, &low, &high);
        if ((low == unreached_low) && (high == unreached_high))
        {
            continue;
        }
        char what[32];
        {
            VerbaLinea buffer_line = mmgr_line_over(what, sizeof(what));
            mmgr_line_put(&buffer_line, "probe place ");
            mmgr_line_decimal(&buffer_line, place, 1u);
            (void)mmgr_line_finish(&buffer_line);
        }
        if (fixed_alike(&query, place_put(kernel, place, unreached_low, unreached_high), second, what))
        {
            probe_place = place;
        }
    }
    if (query.dry)
    {
        fixed_closed(&query);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "orior_klq_identity fixed: the run is dry, and nothing is written");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    if (probe_place == places)
    {
        fixed_closed(&query);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line,
                          "orior_klq_identity fixed: no place answers the case's second word, and nothing is written");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    char row[64];
    {
        VerbaLinea buffer_line = mmgr_line_over(row, sizeof(row));
        mmgr_line_put(&buffer_line, "run answers ");
        mmgr_line_hex(&buffer_line, probe_place, 8u);
        mmgr_line_put(&buffer_line, " probe place");
        (void)mmgr_line_finish(&buffer_line);
    }
    if (!ksc_answers_write(ksc, "probe", std::vector<std::string>{row}))
    {
        fixed_closed(&query);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "orior_klq_identity fixed: ");
            mmgr_line_put(output_line, ksc);
            mmgr_line_put(output_line, " could not be written");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the probe place is place ");
        mmgr_line_decimal(output_line, probe_place, 1u);
        mmgr_line_put(output_line, " of ");
        mmgr_line_decimal(output_line, places, 1u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    // zero: the probe place's own instruction put writing the top of the register field, the one change, and the
    // kernel's own closing stores, reading that register, read it back in the record's second word. The register the
    // kernel stores is then left as the probe place found it, the case's second word, and the top reads 0 where the
    // record is that word alone on every case, whatever the probe place wrote
    std::vector<unsigned char> zero_variant(kernel);
    stall_bits_write(&zero_variant, probe_place, rd, top);
    place_stalled_longest(&zero_variant, probe_place, stall, longest);
    char what[32];
    {
        VerbaLinea buffer_line = mmgr_line_over(what, sizeof(what));
        mmgr_line_put(&buffer_line, "zero ");
        mmgr_line_decimal(&buffer_line, top, 1u);
        (void)mmgr_line_finish(&buffer_line);
    }
    if (!fixed_alike(&query, zero_variant, second, what))
    {
        fixed_closed(&query);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "orior_klq_identity fixed: register ");
            mmgr_line_decimal(output_line, top, 1u);
            mmgr_line_put(output_line,
                          ", the field's top, does not read 0 whatever is written to it, and nothing more is written");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    const unsigned long long zero = top;
    {
        VerbaLinea buffer_line = mmgr_line_over(row, sizeof(row));
        mmgr_line_put(&buffer_line, "run answers ");
        mmgr_line_hex(&buffer_line, zero, 8u);
        mmgr_line_put(&buffer_line, " fixed zero");
        (void)mmgr_line_finish(&buffer_line);
    }
    std::vector<std::string> fixed_rows = {row};
    std::vector<std::string> case_rows;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  zero is register ");
        mmgr_line_decimal(output_line, zero, 1u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    // what each register contains at the probe place, each type's row the lowest register containing it
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
        stall_bits_write(&variant, probe_place, ra, number);
        stall_bits_write(&variant, probe_place, rb, zero);
        place_stalled_longest(&variant, probe_place, stall, longest);
        if (!question_read(variant, &s_question, &query.queries))
        {
            run_print(&query.table, "register %u refused: %s\n", number, s_question.refused);
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  register ");
                mmgr_line_decimal(output_line, number, 1u);
                mmgr_line_put(output_line, ": refused");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            continue;
        }
        const FixedKind type = fixed_type(s_question.answered, word_0.data(), word_1.data(), s_question.cases);
        const char *const question = fixed_type_question(type);
        run_print(&query.table, "register %u contains %s\n", number,
                  (type == FIXED_NOTHING) ? "nothing fixed" : question);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  register ");
            mmgr_line_decimal(output_line, number, 1u);
            mmgr_line_put(output_line, ": ");
            mmgr_line_put(output_line, (type == FIXED_NOTHING) ? "nothing fixed" : question);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        if ((type == FIXED_NOTHING) || (given.count(type) != 0u))
        {
            continue;
        }
        given.insert(type);
        {
            VerbaLinea buffer_line = mmgr_line_over(row, sizeof(row));
            mmgr_line_put(&buffer_line, "run answers ");
            mmgr_line_hex(&buffer_line, number, 8u);
            mmgr_line_put(&buffer_line, " ");
            mmgr_line_put(&buffer_line, question);
            (void)mmgr_line_finish(&buffer_line);
        }
        ((mmgr_text_starts(question, "fixed", PLATFORM_TEXT_TO_TERMINATOR)) ? fixed_rows : case_rows).push_back(row);
    }
    fixed_closed(&query);
    if (!ksc_answers_write(ksc, "fixed", fixed_rows) || !ksc_answers_write(ksc, "case word", case_rows))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "orior_klq_identity fixed: ");
            mmgr_line_put(output_line, ksc);
            mmgr_line_put(output_line, " could not be written");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "orior_klq_identity fixed: the probe place at place ");
        mmgr_line_decimal(output_line, probe_place, 1u);
        mmgr_line_put(output_line, " of ");
        mmgr_line_decimal(output_line, places, 1u);
        mmgr_line_put(output_line, ", zero register ");
        mmgr_line_decimal(output_line, zero, 1u);
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal(output_line, given.size(), 1u);
        mmgr_line_put(output_line, " readings fixed at the probe place, over ");
        mmgr_line_decimal(output_line, query.queries, 1u);
        mmgr_line_put(output_line, " queries, written to ");
        mmgr_line_put(output_line, ksc);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 0;
}
