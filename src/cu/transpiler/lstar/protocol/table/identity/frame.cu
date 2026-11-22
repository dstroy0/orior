// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame.cu: the frame queried of the part: the container the vendor's writer makes loaded and returned from, and the
// least opening a kernel runs with (Open 6.3)
#include "frame_walk.h"
#include "../../../../../../platform/platform.h"
#include "memoria_operor/memoria_linea.h"
#include "cellularum_laboro/cellularum_linea.h"
#include "identity_internal.h"

// the kernel the system accepted, as the vendor's writer appends it to the run in its kernel mode (kernel_blob.h): the
// latest blob of KERNEL_BLOB_NAME read where the run stores it, every place of it whatever place the writer names,
// into `code`, `places` of them, and the registers a thread of it uses into `registers`. The frame's queries and the
// fixed registers' start from the whole kernel. 1, or 0 where the run contains no kernel this reads
int frame_kernel_read(const unsigned char **code, unsigned int *places, unsigned int *registers)
{
    const unsigned char *kernel = NULL;
    unsigned long long kernel_size = 0ULL;
    KernelBlobHead head;
    if (!qry_member_blob(KERNEL_BLOB_NAME, &kernel, &kernel_size, NULL) || (kernel_size < sizeof(head)))
    {
        return 0;
    }
    mmgr_bytes_copy(&head, kernel, sizeof(head));
    if ((head.places == 0u) || ((kernel_size - sizeof(head)) < (16ULL * head.places)))
    {
        return 0;
    }
    *code = kernel + sizeof(head);
    *places = head.places;
    *registers = head.registers;
    return 1;
}

// what one variant of the kernel is put as and what came of it, for the walk's `predicate`: the whole kernel's own
// answers are the reference every variant is checked against
struct FrameQuery
{
    const std::vector<unsigned char> *kernel;
    const std::vector<std::string> *reference;
    const std::vector<unsigned int> *places;
    RunQuestion *question;
    unsigned long long *queries;
    RunWritten *table;
};

// 1 where the kernel with its first `dropped` places let go answers every case as the whole kernel does
static int frame_matches(unsigned long long dropped, void *context)
{
    FrameQuery *const query = (FrameQuery *)context;
    const std::vector<unsigned char> variant(query->kernel->begin() + (std::ptrdiff_t)(dropped * 16ULL),
                                             query->kernel->end());
    const int alike = question_alike(variant, *query->reference, *query->places, query->question, query->queries);
    {
        TextNote text_note;
        text_note_begun(&text_note, &query->table->text);
        mmgr_line_put(&text_note.line, "opening ");
        mmgr_line_decimal(&text_note.line, dropped, 1u);
        mmgr_line_put(&text_note.line, " ");
        mmgr_line_put(&text_note.line, alike ? "truthy" : "falsy");
        mmgr_line_put(&text_note.line, alike ? "" : ": ");
        mmgr_line_put(&text_note.line, alike ? "" : query->question->refused);
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  opening from ");
        mmgr_line_decimal(output_line, dropped, 1u);
        mmgr_line_put(output_line, ": ");
        mmgr_line_put(output_line, alike ? "truthy" : "falsy");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return alike;
}

// The frame queried (Open 6.3): the kernel the system accepted, its body the ladder's relation at its probe place, is
// put whole over every case with no host value to check it against, and an answer on every case means the container the
// vendor's writer makes is loaded and returned from: `run answers 00000001 container returned`. Its answers are then
// the reference: the kernel with its first k places dropped is put, k walked by halving to the greatest that still
// answers every case as the whole kernel does, the least opening a kernel runs with: `run answers <k> opening least`.
// Each variant and its verdict is a line of the run's blob frame.txt. 0 where both rows are written or the run is dry,
// 1 otherwise with the reason printed
int identity_frame(const char *engine, const char *answers, const char *ksc, const std::string &folder,
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
    if (!run_channel_open(carrier, 60000000ULL))
    {
        return 1;
    }
    record_open(ksc, "frame");
    static RunQuestion s_question;
    s_question.place = RUN_PLACE_NONE;
    s_question.registers = registers;
    std::vector<unsigned int> case_places;
    stall_cases(&s_question, &case_places);
    // no host value checks the whole kernel: the query is whether the part answers it at all
    const std::vector<std::string> nothing(IDENTITY_CASES, "-");
    unsigned long long queries = 0ULL;
    RunWritten table = {(folder + "/frame.txt"), 0, std::string()};
    const int returned = question_alike(kernel, nothing, case_places, &s_question, &queries);
    {
        TextNote text_note;
        text_note_begun(&text_note, &(&table)->text);
        mmgr_line_put(&text_note.line, "container ");
        mmgr_line_put(&text_note.line, returned ? "returned" : "not returned");
        mmgr_line_put(&text_note.line, returned ? "" : ": ");
        mmgr_line_put(&text_note.line, returned ? "" : s_question.refused);
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    const int dry = mmgr_text_starts(s_question.refused, "skipped dry", PLATFORM_TEXT_TO_TERMINATOR);
    if (!returned)
    {
        run_closed(&table);
        run_channel_close();
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "orior_klq_identity frame: ");
            mmgr_line_put(output_line, dry ? "the run is dry, and nothing is written"
                                           : "the container was not returned from, and nothing is written");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return dry ? 0 : 1;
    }
    if (!ksc_answers_write(ksc, "container", std::vector<std::string>{"run answers 00000001 container returned"}))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "orior_klq_identity frame: ");
            mmgr_line_put(output_line, ksc);
            mmgr_line_put(output_line, " could not be written");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        run_channel_close();
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
    // the whole kernel's own answers, case by case, are what every variant is checked against
    std::vector<std::string> reference(IDENTITY_CASES, "-");
    for (unsigned int place = 0u; place < s_question.cases; place += 1u)
    {
        char answered[24];
        {
            VerbaLinea buffer_line = mmgr_line_over(answered, sizeof(answered));
            mmgr_line_hex(&buffer_line, s_question.answered[place], 1u);
            (void)mmgr_line_finish(&buffer_line);
        }
        reference[case_places[place]] = answered;
    }
    FrameQuery query = {&kernel, &reference, &case_places, &s_question, &queries, &table};
    const unsigned long long from = frame_greatest(frame_matches, &query, 0ULL, (unsigned long long)places);
    if (!run_closed(&table))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "orior_klq_identity frame: ");
            mmgr_line_put(output_line, folder.c_str());
            mmgr_line_put(output_line, "/frame.txt could not be written");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        run_channel_close();
        return 1;
    }
    run_channel_close();
    char row[64];
    {
        VerbaLinea buffer_line = mmgr_line_over(row, sizeof(row));
        mmgr_line_put(&buffer_line, "run answers ");
        mmgr_line_hex(&buffer_line, from, 8u);
        mmgr_line_put(&buffer_line, " opening least");
        (void)mmgr_line_finish(&buffer_line);
    }
    if (!ksc_answers_write(ksc, "opening", std::vector<std::string>{row}))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "orior_klq_identity frame: ");
            mmgr_line_put(output_line, ksc);
            mmgr_line_put(output_line, " could not be written");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    {
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
    }
    return 0;
}
