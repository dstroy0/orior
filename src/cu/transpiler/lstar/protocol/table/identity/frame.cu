// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame.cu: the frame queried of the part: the container the vendor's writer makes loaded and returned from, and the
// least opening a kernel runs with (Open 6.3)
#include "../../../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "frame_walk.h"
#include "identity_internal.h"
#include "memoria_operor/memoria_operor.h"

int frame_kernel_read(const unsigned char **code, unsigned int *places, unsigned int *registers)
{
    const unsigned char *kernel = NULL;
    unsigned long long kernel_size = 0ULL;
    KernelBlobHead head;
    if (!qry_member_blob(KERNEL_BLOB_NAME, &kernel, &kernel_size, NULL) || (kernel_size < sizeof(head)))
    {
        return 0;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &head, .src = kernel, .bytes = sizeof(head));
    if ((head.places == 0u) || ((kernel_size - sizeof(head)) < (16ULL * head.places)))
    {
        return 0;
    }
    *code = kernel + sizeof(head);
    *places = head.places;
    *registers = head.registers;
    return 1;
}

// what one variant of the kernel goes as and what came of it, for the walk's `predicate`. The whole kernel's own
// answers are the reference the walk checks every variant against
struct FrameQuery
{
    const IdentityCode *kernel;
    const HostCases *reference;
    const unsigned int *places;
    RunQuestion *question;
    unsigned long long *queries;
    RunWritten *table;
};

// 1 where the kernel with its first `dropped` places let go answers every case as the whole kernel does
static int frame_matches(unsigned long long dropped, void *context)
{
    FrameQuery *const query = (FrameQuery *)context;
    IdentityCode variant;
    identity_code_begun(&variant, PLATFORM_TIER_TEMPORARY);
    const unsigned long long from = dropped * 16ULL;
    const unsigned long long kept = (from < query->kernel->count) ? (query->kernel->count - from) : 0ULL;
    if (platform_array_counted(&variant, kept) && (kept != 0ULL))
    {
        // a kernel is at max the pool's bytes, which a size_t counts
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = variant.cell, .src = (const unsigned char *)query->kernel->cell + from,
                   .bytes = (size_t)kept);
    }
    const int alike = question_alike(&variant, query->reference, query->places, query->question, query->queries);
    platform_array_ended(&variant);
    VerbaLinea *const line = &query->table->text.line;
    mmgr_line_put(line, "opening ");
    mmgr_line_decimal(line, dropped, 1u);
    mmgr_line_put(line, " ");
    mmgr_line_put(line, alike ? "truthy" : "falsy");
    mmgr_line_put(line, alike ? "" : ": ");
    mmgr_line_put(line, alike ? "" : query->question->refused);
    mmgr_line_ch(line, '\n');
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  opening from ");
    mmgr_line_decimal(output_line, dropped, 1u);
    mmgr_line_put(output_line, ": ");
    mmgr_line_put(output_line, alike ? "truthy" : "falsy");
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return alike;
}

// a line on the output stream: the frame's name, then `first`, then `second`
static void frame_said(const char *first, const char *second)
{
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "orior_klq_identity frame: ");
    mmgr_line_put(output_line, first);
    mmgr_line_put(output_line, second);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
}

// the bytes of one answer of the whole kernel, its terminator among them: a 64-bit value in hex
#define FRAME_ANSWER_BYTES 24u

// the places of the cases the frame goes over, and the whole kernel's answer to each, which the reference points into
static unsigned int s_frame_places[RUN_CASES_MAX];
static char s_frame_answers[IDENTITY_CASES][FRAME_ANSWER_BYTES];

int identity_frame(const char *engine, const char *answers, const char *ksc, const char *folder,
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
    IdentityCode kernel;
    identity_code_begun(&kernel, PLATFORM_TIER_TEMPORARY);
    if (platform_array_counted(&kernel, 16ULL * places) && (places != 0u))
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = kernel.cell, .src = code, .bytes = 16u * (size_t)places);
    }
    if (!run_channel_open(carrier, 60000000ULL))
    {
        platform_array_ended(&kernel);
        return 1;
    }
    record_open(ksc, "frame");
    static RunQuestion s_question;
    s_question.place = RUN_PLACE_NONE;
    s_question.registers = registers;
    stall_cases(&s_question, s_frame_places);
    // no host value checks the whole kernel: the query is whether the part answers it at all
    static HostCases s_nothing;
    host_cases_filled(&s_nothing, "-");
    unsigned long long queries = 0ULL;
    RunWritten table;
    run_written_begun(&table, identity_path_joined(folder, "/frame.txt").text, 0);
    const int returned = question_alike(&kernel, &s_nothing, s_frame_places, &s_question, &queries);
    mmgr_line_put(&table.text.line, "container ");
    mmgr_line_put(&table.text.line, returned ? "returned" : "not returned");
    mmgr_line_put(&table.text.line, returned ? "" : ": ");
    mmgr_line_put(&table.text.line, returned ? "" : s_question.refused);
    mmgr_line_ch(&table.text.line, '\n');
    const int dry = EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = s_question.refused,
                               .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "skipped dry", .ci = EMBED_FALSE);
    if (!returned)
    {
        (void)run_closed(&table);
        run_written_ended(&table);
        run_channel_close();
        frame_said(dry ? "the run is dry, and nothing is written"
                       : "the container was not returned from, and nothing is written",
                   "");
        platform_array_ended(&kernel);
        return dry ? 0 : 1;
    }
    if (!ksc_answer_write(ksc, "container", "run answers 00000001 container returned"))
    {
        frame_said(ksc, " could not be written");
        run_written_ended(&table);
        run_channel_close();
        platform_array_ended(&kernel);
        return 1;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the container returned, over ");
        mmgr_line_decimal(output_line, s_question.cases, 1u);
        mmgr_line_put(output_line, " cases");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    // the whole kernel's own answers, case by case, are what the walk checks every variant against
    static HostCases s_reference;
    host_cases_filled(&s_reference, "-");
    for (unsigned int place = 0u; place < s_question.cases; place += 1u)
    {
        const unsigned int at = s_frame_places[place];
        VerbaLinea buffer_line = mmgr_line_over(s_frame_answers[at], sizeof(s_frame_answers[at]));
        mmgr_line_hex(&buffer_line, s_question.answered[place], 1u);
        (void)mmgr_line_finish(&buffer_line);
        s_reference.answer[at] = s_frame_answers[at];
    }
    FrameQuery query = {&kernel, &s_reference, s_frame_places, &s_question, &queries, &table};
    const unsigned long long from = frame_greatest(frame_matches, &query, 0ULL, (unsigned long long)places);
    platform_array_ended(&kernel);
    const int closed = run_closed(&table);
    run_written_ended(&table);
    if (!closed)
    {
        frame_said(folder, "/frame.txt could not be written");
        run_channel_close();
        return 1;
    }
    run_channel_close();
    char row[64];
    VerbaLinea row_line = mmgr_line_over(row, sizeof(row));
    mmgr_line_put(&row_line, "run answers ");
    mmgr_line_hex(&row_line, from, 8u);
    mmgr_line_put(&row_line, " opening least");
    (void)mmgr_line_finish(&row_line);
    if (!ksc_answer_write(ksc, "opening", row))
    {
        frame_said(ksc, " could not be written");
        return 1;
    }
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "orior_klq_identity frame: the least opening begins at place ");
    mmgr_line_decimal(output_line, from, 1u);
    mmgr_line_put(output_line, " of ");
    mmgr_line_decimal(output_line, places, 1u);
    mmgr_line_put(output_line, ", over ");
    mmgr_line_decimal(output_line, queries, 1u);
    mmgr_line_put(output_line, " queries, written to ");
    mmgr_line_put(output_line, ksc);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return 0;
}
