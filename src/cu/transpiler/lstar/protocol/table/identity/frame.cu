// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame.cu: the frame asked of the part: the container the vendor's writer makes loaded and returned from, and the
// least opening a kernel runs with (The frame asked, Open 6.3)
#include "frame_walk.h"
#include "identity_internal.h"

// the kernel the system accepted, as the vendor's writer hands it to the run in its kernel mode: `kernel <places>
// <registers> <from>` then one line a place, low word then high. Every place is read, whatever place the writer
// names: the frame's asks start from the whole kernel. 1, or 0 with the reason printed
static int frame_kernel_read(std::vector<unsigned char> *code, unsigned int *registers)
{
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    if ((qry_run() == NULL) || !qry_latest(qry_run(), "kernel.txt", &bytes, &size, NULL))
    {
        printf("klq_identity frame: the run holds no kernel.txt; the vendor's writer hands it in its kernel mode\n");
        return 0;
    }
    std::istringstream lines(std::string((const char *)bytes, (size_t)size));
    std::string line;
    unsigned int places = 0u;
    unsigned int from = 0u;
    if (!std::getline(lines, line) || (sscanf(line.c_str(), "kernel %u %u %u", &places, registers, &from) < 2) ||
        (places == 0u))
    {
        printf("klq_identity frame: kernel.txt holds no kernel this reads\n");
        return 0;
    }
    code->clear();
    for (unsigned int place = 0u; place < places; place += 1u)
    {
        unsigned long long low = 0ull;
        unsigned long long high = 0ull;
        if (!std::getline(lines, line) || (sscanf(line.c_str(), "%llx %llx", &low, &high) != 2))
        {
            printf("klq_identity frame: kernel.txt holds %u places, fewer than its head says\n", place);
            return 0;
        }
        for (unsigned int byte = 0u; byte < 8u; byte += 1u)
        {
            code->push_back((unsigned char)((low >> (8u * byte)) & 0xffull));
        }
        for (unsigned int byte = 0u; byte < 8u; byte += 1u)
        {
            code->push_back((unsigned char)((high >> (8u * byte)) & 0xffull));
        }
    }
    return 1;
}

// what one variant of the kernel is put as and what came of it, for the walk's `holds`: the whole kernel's own
// answers are the reference every variant is held to
struct FrameAsk
{
    const std::vector<unsigned char> *kernel;
    const std::vector<std::string> *reference;
    const std::vector<unsigned int> *places;
    RunQuestion *question;
    unsigned long long *asks;
    RunWritten *table;
};

// 1 where the kernel with its first `dropped` places let go answers every case as the whole kernel does
static int frame_holds(unsigned long long dropped, void *context)
{
    FrameAsk *const ask = (FrameAsk *)context;
    const std::vector<unsigned char> variant(ask->kernel->begin() + (std::ptrdiff_t)(dropped * 16ull),
                                             ask->kernel->end());
    const int alike = question_alike(variant, *ask->reference, *ask->places, ask->question, ask->asks);
    run_print(ask->table, "opening %llu %s%s%s\n", dropped, alike ? "truthy" : "falsy", alike ? "" : ": ",
              alike ? "" : ask->question->refused);
    printf("  opening from %llu: %s\n", dropped, alike ? "truthy" : "falsy");
    return alike;
}

// The frame asked (Open 6.3): the kernel the system accepted, its body the ladder's relation at its slot, is put whole
// over every case with no host value to hold it to, and an answer on every case means the container the vendor's
// writer makes is loaded and returned from: `run answers 00000001 container returned`. Its answers are then the
// reference: the kernel with its first k places dropped is put, k walked by halving to the greatest that still answers
// every case as the whole kernel does, the least opening a kernel runs with: `run answers <k> opening least`. Each
// variant and its verdict is a line of the run's blob frame.txt. 0 where both rows are written or the run is dry, 1
// otherwise with the reason printed
int identity_frame(const char *engine, const char *answers, const char *ksc, const std::string &folder,
                   const char *const *carrier)
{
    (void)engine;
    (void)answers;
    std::vector<unsigned char> kernel;
    unsigned int registers = 0u;
    if (!frame_kernel_read(&kernel, &registers))
    {
        return 1;
    }
    const unsigned int places = (unsigned int)(kernel.size() / 16u);
    if (!run_channel_open(carrier, 60000000ull))
    {
        return 1;
    }
    record_open(ksc, "frame");
    static RunQuestion s_question;
    s_question.slot = RUN_SLOT_NONE;
    s_question.registers = registers;
    std::vector<unsigned int> case_places;
    stall_cases(&s_question, &case_places);
    // no host value holds the whole kernel: the ask is whether the part answers it at all
    const std::vector<std::string> nothing(IDENTITY_CASES, "-");
    unsigned long long asks = 0ull;
    RunWritten table = {(folder + "/frame.txt"), 0, std::string()};
    const int returned = question_alike(kernel, nothing, case_places, &s_question, &asks);
    run_print(&table, "container %s%s%s\n", returned ? "returned" : "not returned", returned ? "" : ": ",
              returned ? "" : s_question.refused);
    const int dry = strncmp(s_question.refused, "skipped dry", 11u) == 0;
    if (!returned)
    {
        run_closed(&table);
        run_channel_close();
        printf("klq_identity frame: %s\n", dry ? "the run is dry, and nothing is written"
                                             : "the container was not returned from, and nothing is written");
        return dry ? 0 : 1;
    }
    if (!ksc_answers_write(ksc, "container", std::vector<std::string>{"run answers 00000001 container returned"}))
    {
        printf("klq_identity frame: %s could not be written\n", ksc);
        run_channel_close();
        return 1;
    }
    printf("  the container returned, over %u cases\n", s_question.cases);
    // the whole kernel's own answers, case by case, are what every variant is held to
    std::vector<std::string> reference(IDENTITY_CASES, "-");
    for (unsigned int place = 0u; place < s_question.cases; place += 1u)
    {
        char answered[24];
        snprintf(answered, sizeof(answered), "%llx", s_question.answered[place]);
        reference[case_places[place]] = answered;
    }
    FrameAsk ask = {&kernel, &reference, &case_places, &s_question, &asks, &table};
    const unsigned long long from = frame_greatest(frame_holds, &ask, 0ull, (unsigned long long)places);
    if (!run_closed(&table))
    {
        printf("klq_identity frame: %s/frame.txt could not be written\n", folder.c_str());
        run_channel_close();
        return 1;
    }
    run_channel_close();
    char row[64];
    snprintf(row, sizeof(row), "run answers %08llx opening least", from);
    if (!ksc_answers_write(ksc, "opening", std::vector<std::string>{row}))
    {
        printf("klq_identity frame: %s could not be written\n", ksc);
        return 1;
    }
    printf("klq_identity frame: the least opening begins at place %llu of %u, over %llu asks, written to %s\n", from,
           places, asks, ksc);
    return 0;
}
