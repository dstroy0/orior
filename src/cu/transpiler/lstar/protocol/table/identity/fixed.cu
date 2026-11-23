// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// fixed.cu: the fixed registers queried of the part: the probe place of the kernel the system accepted, the register
// that reads 0 whatever is written to it, and what each register contains at the probe place (Open 6.2)
#include "../../../../../../platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "fixed_read.h"
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
            (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = s_row.answer, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = "answers", .ci = EMBED_FALSE)) &&
            (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = s_row.question, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = "opening least", .ci = EMBED_FALSE)))
        {
            // the row's word is a place, at max eight hex digits
            from = (unsigned int)s_row.value;
        }
    }
    ksc_free(&file);
    return from;
}

// the two words of place `place` of `kernel`, low then high
static void place_words(const IdentityCode *kernel, unsigned int place, unsigned long long *low,
                        unsigned long long *high)
{
    const unsigned char *const bytes = (const unsigned char *)kernel->cell;
    *low = 0ULL;
    *high = 0ULL;
    for (unsigned int byte = 0u; byte < 8u; byte += 1u)
    {
        *low |= (unsigned long long)bytes[(place * 16u) + byte] << (8u * byte);
        *high |= (unsigned long long)bytes[(place * 16u) + 8u + byte] << (8u * byte);
    }
}

// `variant` made `kernel` with the words `low` and `high` at place `place`
static void place_put(const IdentityCode *kernel, unsigned int place, unsigned long long low, unsigned long long high,
                      IdentityCode *variant)
{
    (void)identity_code_copied(variant, kernel);
    unsigned char *const bytes = (unsigned char *)variant->cell;
    for (unsigned int byte = 0u; byte < 8u; byte += 1u)
    {
        bytes[(place * 16u) + byte] = (unsigned char)((low >> (8u * byte)) & 0xffULL);
        bytes[(place * 16u) + 8u + byte] = (unsigned char)((high >> (8u * byte)) & 0xffULL);
    }
}

// the yield field as the part's .ksc gives it, none where it gives no row
static StallField s_yield = {0u, 0u};

// Place `place` of `variant` put to stall the longest with its yield bit clear. The gate lets an instruction no form
// passes through with the longest stall alone. The vendor's reader takes the longest stall with the yield bit clear
// alone, the two bits together an encoding it calls illegal
static void place_stalled_longest(IdentityCode *variant, unsigned int place, StallField stall,
                                  unsigned long long longest)
{
    stall_bits_write(variant, place, stall, longest);
    stall_bits_write(variant, place, s_yield, 0ULL);
}

// what the mode records while it queries. The cases' host places, the question, the queries counted, the blob
// fixed.txt, and whether the run is dry
struct FixedQuery
{
    unsigned int places[RUN_CASES_MAX];
    RunQuestion *question;
    unsigned long long queries;
    RunWritten table;
    int dry;
};

// 1 where the part answers `variant` alike with `reference` on every case. The query is a line of fixed.txt named by
// `what`, and the carrier's refusal notes a dry run
static int fixed_alike(FixedQuery *query, const IdentityCode *variant, const HostCases *reference, const char *what)
{
    const int alike = question_alike(variant, reference, query->places, query->question, &query->queries);
    query->dry =
        query->dry || (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = query->question->refused,
                                  .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "skipped dry", .ci = EMBED_FALSE));
    const char *const why = (query->question->outcome == RUN_ANSWERED) ? "apart" : query->question->refused;
    VerbaLinea *const line = &query->table.text.line;
    mmgr_line_put(line, what);
    mmgr_line_put(line, " ");
    mmgr_line_put(line, alike ? "truthy" : "falsy");
    mmgr_line_put(line, alike ? "" : ": ");
    mmgr_line_put(line, alike ? "" : why);
    mmgr_line_ch(line, '\n');
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  ");
    mmgr_line_put(output_line, what);
    mmgr_line_put(output_line, ": ");
    mmgr_line_put(output_line, alike ? "truthy" : "falsy");
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return alike;
}

// a line on the output stream: the mode's name, then `first`, then `second`
static void fixed_said(const char *first, const char *second)
{
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "orior_klq_identity fixed: ");
    mmgr_line_put(output_line, first);
    mmgr_line_put(output_line, second);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
}

// the blob passed to the run and the channel closed, as every way out does, and what the query holds given back
static void fixed_closed(FixedQuery *query)
{
    if (!run_closed(&query->table))
    {
        fixed_said(query->table.path.text, " could not be written");
    }
    run_written_ended(&query->table);
    run_channel_close();
}

// the rows the mode writes at max: the fixed zero and one for each type a register contains
#define FIXED_ROWS_MAX (FIXED_CASE_WORD_1 + 2u)

// the bytes of one row the mode writes, its terminator among them
#define FIXED_ROW_BYTES 64u

// rows the mode writes to the .ksc, each a text of its own and a pointer to it
struct FixedRows
{
    char text[FIXED_ROWS_MAX][FIXED_ROW_BYTES];
    const char *row[FIXED_ROWS_MAX];
    unsigned int count;
};

// the terminated `row` put after the rows of `rows`, where they hold it
static void fixed_row_added(FixedRows *rows, const char *row)
{
    if (rows->count < FIXED_ROWS_MAX)
    {
        (void)identity_name_set(rows->text[rows->count], FIXED_ROW_BYTES, row, PLATFORM_TEXT_TO_TERMINATOR);
        rows->row[rows->count] = rows->text[rows->count];
        rows->count += 1u;
    }
}

// what the mode queries over, shared by its walk: the query, the case's second word for each case, and the answers
// that point into it
static FixedQuery s_fixed_query;
static char s_fixed_words[IDENTITY_CASES][24];

// The fixed registers queried (Open 6.2), over the kernel the system accepted, read whole from the run's kernel blob
// (kernel_blob.h) as the frame reads it.
//
// The probe place. The mode puts each place from the opening's start with the encoding the unreached places contain,
// the last place's. The first whose kernel answers the case's second word on every case is the probe place, where
// nothing then writes the register the kernel stores: `run answers <s> probe place`.
//
// Zero. The mode puts the probe place's own instruction to write the top of the register field. The kernel's own
// closing stores, which read that register, read it back in the record's second word. The top reads 0 where that word
// is 0 on every case whatever the probe place wrote: `run answers <n> fixed zero`. The mode queries no other register
// for it. A question changes one place no form of the machine contains, the gate's bound, and no form matches the
// kernel's store once it changes.
//
// What each register contains. For each register n the kernel uses, the mode puts the probe place's instruction with n
// as its first source and zero as its second. The stored answer is n's value, read against the thread's number and the
// case's words (fixed_read.h). Its rows are `run answers <n> fixed lane_number`, `fixed record`, `fixed index`, `case
// word 0` and `case word 1`, each type the lowest register that holds it.
//
// The mode puts the place a variant changes with its stall the longest and its yield bit clear. The gate lets an
// instruction no form contains through only that way, and the vendor's reader takes it. The walks put their chains
// that way too. Each query and each reading is a line of the run's blob fixed.txt. 0 where the mode writes the rows or
// the run is dry, 1 otherwise with the reason printed
int identity_fixed(const char *engine, const char *answers, const char *ksc, const char *folder,
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
    StallFields fields;
    stall_fields(ksc, &fields);
    const StallField *const stall_field = field_of(&fields, "stall");
    const StallField *const rd_field = field_of(&fields, "rd");
    const StallField *const ra_field = field_of(&fields, "ra");
    const StallField *const rb_field = field_of(&fields, "rb");
    if ((stall_field == NULL) || (rd_field == NULL) || (ra_field == NULL) || (rb_field == NULL))
    {
        fixed_said(ksc, " gives no stall field or not every register field");
        stall_fields_ended(&fields);
        return 1;
    }
    const StallField stall = *stall_field;
    const StallField rd = *rd_field;
    const StallField ra = *ra_field;
    const StallField rb = *rb_field;
    const StallField *const yield_field = field_of(&fields, "yield");
    const StallField no_field = {0u, 0u};
    s_yield = (yield_field != NULL) ? *yield_field : no_field;
    stall_fields_ended(&fields);
    const unsigned long long longest = (1ULL << stall.bits) - 1ULL;
    const unsigned long long top = (1ULL << rd.bits) - 1ULL;
    const unsigned int from = fixed_opening_read(ksc);
    if (!run_channel_open(carrier, 60000000ULL))
    {
        return 1;
    }
    IdentityCode kernel;
    identity_code_begun(&kernel, PLATFORM_TIER_TEMPORARY);
    if (platform_array_counted(&kernel, 16ULL * places) && (places != 0u))
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = kernel.cell, .src = code, .bytes = 16u * (size_t)places);
    }
    record_open(ksc, "fixed");
    static RunQuestion s_question;
    s_question.place = RUN_PLACE_NONE;
    s_question.registers = registers;
    FixedQuery &query = s_fixed_query;
    query.question = &s_question;
    query.queries = 0ULL;
    query.dry = 0;
    run_written_begun(&query.table, identity_path_joined(folder, "/fixed.txt").text, 0);
    stall_cases(&s_question, query.places);
    // the probe place
    unsigned long long unreached_low = 0ULL;
    unsigned long long unreached_high = 0ULL;
    place_words(&kernel, places - 1u, &unreached_low, &unreached_high);
    static HostCases s_second;
    host_cases_filled(&s_second, "-");
    for (unsigned int at = 0u; at < s_question.cases; at += 1u)
    {
        const unsigned int place = query.places[at];
        VerbaLinea buffer_line = mmgr_line_over(s_fixed_words[place], sizeof(s_fixed_words[place]));
        mmgr_line_hex(&buffer_line, s_question.word[at][1], 1u);
        (void)mmgr_line_finish(&buffer_line);
        s_second.answer[place] = s_fixed_words[place];
    }
    const HostCases *const second = &s_second;
    IdentityCode variant;
    identity_code_begun(&variant, PLATFORM_TIER_TEMPORARY);
    unsigned int probe_place = places;
    for (unsigned int place = from; (place < places) && (probe_place == places) && !query.dry; place += 1u)
    {
        unsigned long long low = 0ULL;
        unsigned long long high = 0ULL;
        place_words(&kernel, place, &low, &high);
        if ((low == unreached_low) && (high == unreached_high))
        {
            continue;
        }
        char what[32];
        VerbaLinea buffer_line = mmgr_line_over(what, sizeof(what));
        mmgr_line_put(&buffer_line, "probe place ");
        mmgr_line_decimal(&buffer_line, place, 1u);
        (void)mmgr_line_finish(&buffer_line);
        place_put(&kernel, place, unreached_low, unreached_high, &variant);
        if (fixed_alike(&query, &variant, second, what))
        {
            probe_place = place;
        }
    }
    if (query.dry || (probe_place == places))
    {
        const int dry = query.dry;
        fixed_closed(&query);
        platform_array_ended(&variant);
        platform_array_ended(&kernel);
        fixed_said(dry ? "the run is dry, and nothing is written"
                       : "no place answers the case's second word, and nothing is written",
                   "");
        return dry ? 0 : 1;
    }
    char row[64];
    VerbaLinea row_line = mmgr_line_over(row, sizeof(row));
    mmgr_line_put(&row_line, "run answers ");
    mmgr_line_hex(&row_line, probe_place, 8u);
    mmgr_line_put(&row_line, " probe place");
    (void)mmgr_line_finish(&row_line);
    if (!ksc_answer_write(ksc, "probe", row))
    {
        fixed_closed(&query);
        platform_array_ended(&variant);
        platform_array_ended(&kernel);
        fixed_said(ksc, " could not be written");
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
    // Zero: the probe place's own instruction put writing the top of the register field, the one change. The kernel's
    // own closing stores, which read that register, read it back in the record's second word. The register the kernel
    // stores keeps what the probe place found, the case's second word. The top reads 0 where the record is that word
    // alone on every case, whatever the probe place wrote
    (void)identity_code_copied(&variant, &kernel);
    stall_bits_write(&variant, probe_place, rd, top);
    place_stalled_longest(&variant, probe_place, stall, longest);
    char what[32];
    VerbaLinea what_line = mmgr_line_over(what, sizeof(what));
    mmgr_line_put(&what_line, "zero ");
    mmgr_line_decimal(&what_line, top, 1u);
    (void)mmgr_line_finish(&what_line);
    const int zero_alike = fixed_alike(&query, &variant, second, what);
    if (!zero_alike)
    {
        fixed_closed(&query);
        platform_array_ended(&variant);
        platform_array_ended(&kernel);
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "orior_klq_identity fixed: register ");
        mmgr_line_decimal(output_line, top, 1u);
        mmgr_line_put(output_line,
                      ", the field's top, does not read 0 whatever is written to it, and nothing more is written");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
        return 1;
    }
    const unsigned long long zero = top;
    row_line = mmgr_line_over(row, sizeof(row));
    mmgr_line_put(&row_line, "run answers ");
    mmgr_line_hex(&row_line, zero, 8u);
    mmgr_line_put(&row_line, " fixed zero");
    (void)mmgr_line_finish(&row_line);
    static FixedRows s_fixed_rows;
    static FixedRows s_case_rows;
    s_fixed_rows.count = 0u;
    s_case_rows.count = 0u;
    fixed_row_added(&s_fixed_rows, row);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  zero is register ");
        mmgr_line_decimal(output_line, zero, 1u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    // what each register contains at the probe place, each type's row the lowest register that holds it
    unsigned int word_0[RUN_CASES_MAX];
    unsigned int word_1[RUN_CASES_MAX];
    for (unsigned int at = 0u; at < s_question.cases; at += 1u)
    {
        word_0[at] = s_question.word[at][0];
        word_1[at] = s_question.word[at][1];
    }
    // the types the mode reads a register as so far, each once
    int given[FIXED_CASE_WORD_1 + 1u] = {0};
    unsigned int given_count = 0u;
    for (unsigned int number = 0u; number < registers; number += 1u)
    {
        (void)identity_code_copied(&variant, &kernel);
        stall_bits_write(&variant, probe_place, ra, number);
        stall_bits_write(&variant, probe_place, rb, zero);
        place_stalled_longest(&variant, probe_place, stall, longest);
        if (!question_read(&variant, &s_question, &query.queries))
        {
            VerbaLinea *const line = &query.table.text.line;
            mmgr_line_put(line, "register ");
            mmgr_line_decimal(line, number, 1u);
            mmgr_line_put(line, " refused: ");
            mmgr_line_put(line, s_question.refused);
            mmgr_line_ch(line, '\n');
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  register ");
            mmgr_line_decimal(output_line, number, 1u);
            mmgr_line_put(output_line, ": refused");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
            continue;
        }
        const FixedType type = fixed_type(s_question.answered, word_0, word_1, s_question.cases);
        const char *const question = fixed_type_question(type);
        VerbaLinea *const line = &query.table.text.line;
        mmgr_line_put(line, "register ");
        mmgr_line_decimal(line, number, 1u);
        mmgr_line_put(line, " contains ");
        mmgr_line_put(line, (type == FIXED_NOTHING) ? "nothing fixed" : question);
        mmgr_line_ch(line, '\n');
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  register ");
            mmgr_line_decimal(output_line, number, 1u);
            mmgr_line_put(output_line, ": ");
            mmgr_line_put(output_line, (type == FIXED_NOTHING) ? "nothing fixed" : question);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        if ((type == FIXED_NOTHING) || (given[type] != 0))
        {
            continue;
        }
        given[type] = 1;
        given_count += 1u;
        row_line = mmgr_line_over(row, sizeof(row));
        mmgr_line_put(&row_line, "run answers ");
        mmgr_line_hex(&row_line, number, 8u);
        mmgr_line_put(&row_line, " ");
        mmgr_line_put(&row_line, question);
        (void)mmgr_line_finish(&row_line);
        fixed_row_added(EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = question, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                   .other = "fixed", .ci = EMBED_FALSE)
                            ? &s_fixed_rows
                            : &s_case_rows,
                        row);
    }
    platform_array_ended(&variant);
    platform_array_ended(&kernel);
    fixed_closed(&query);
    const int written = ksc_answers_write(ksc, "fixed", s_fixed_rows.row, s_fixed_rows.count) &&
                        ksc_answers_write(ksc, "case word", s_case_rows.row, s_case_rows.count);
    if (!written)
    {
        fixed_said(ksc, " could not be written");
        return 1;
    }
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "orior_klq_identity fixed: the probe place at place ");
    mmgr_line_decimal(output_line, probe_place, 1u);
    mmgr_line_put(output_line, " of ");
    mmgr_line_decimal(output_line, places, 1u);
    mmgr_line_put(output_line, ", zero register ");
    mmgr_line_decimal(output_line, zero, 1u);
    mmgr_line_put(output_line, ", ");
    mmgr_line_decimal(output_line, given_count, 1u);
    mmgr_line_put(output_line, " readings fixed at the probe place, over ");
    mmgr_line_decimal(output_line, query.queries, 1u);
    mmgr_line_put(output_line, " queries, written to ");
    mmgr_line_put(output_line, ksc);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return 0;
}
