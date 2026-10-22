// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// curve.cu: the curve: the cost of registers to a task, timed on the part at each count of threads
#include "identity_internal.h"

// `band` widened to hold `time`: 1 where it had to widen, 0 where it held `time` already
static int curve_band_widened(CurveBand *band, unsigned long long time)
{
    const int widened = (time < band->low) || (time > band->high);
    band->low = std::min(band->low, time);
    band->high = std::max(band->high, time);
    return widened;
}

// one point of a curve asked of the part: `code` declaring `registers`, in blocks of `threads`, as many blocks as
// CURVE_THREADS takes, timed over CURVE_LAUNCHES. 1 where it answers alike with the host, its time a launch into
// `nanoseconds`; 0 where it answers apart or the part refuses it. Every point asked is a line of `table`
static int curve_point(const std::vector<unsigned char> &code, const std::vector<std::string> &host,
                       const std::vector<unsigned int> &places, RunQuestion *question, unsigned long long *asks,
                       const std::string &task, unsigned int threads, unsigned int registers,
                       unsigned long long *nanoseconds, RunWritten *table)
{
    question->registers = registers;
    question->threads = threads;
    question->blocks = CURVE_THREADS / threads;
    question->launches = CURVE_LAUNCHES;
    const int alike = question_alike(code, host, places, question, asks);
    *nanoseconds = alike ? (question->nanoseconds / CURVE_LAUNCHES) : 0ull;
    run_print(table, "%s threads %u blocks %u registers %u: %s %llu ns a launch%s%s\n", task.c_str(), threads,
              question->blocks, registers, alike ? "alike" : "apart", *nanoseconds, alike ? "" : ", ",
              alike ? "" : question->refused);
    return alike;
}

// The cost of registers to a task, asked of the part by timing it. A task is a chain of ours the host also computes,
// with every stall the longest. For each count of threads a block holds, from 1 and doubling, the task is launched
// over CURVE_THREADS threads in all, and its curve is its time against the registers its container declares: from the
// fewest it answers alike with to the most the register field names. The fewest is asked in blocks of one thread,
// walked up from the registers the chain names, or where it gives none from one past the highest number its
// register fields hold, until it answers alike and halved down from there, and is the task's at every count of
// threads. The fewest's band is the least and the most of its times, asked
// again until it holds CURVE_SUSTAIN asks in a row without widening. A count of registers is truthy where the task
// answers alike and its time a launch keeps within the band, and falsy where it answers apart, the part refuses the
// launch, or its time is past the band twice running with the fewest bounced between and holding to the band; where
// the bounced fewest strays past the band itself, the band widens and the count is asked again. Declaring more
// registers takes residency and never gives it: every count past a falsy one is falsy, and the knee, the most
// registers truthy, is found by halving. The counts of threads end where the part answers no count of registers. Each
// knee is written to the .ksc as the part's answer on the run channel, `run answers <registers> curve <task>
// <threads>`, and each task's count of threads whose fewest registers run soonest as `run answers <threads> curve
// <task> threads`; the run's blob curve.txt holds every point asked
int identity_curve(const char *engine, const char *answers, const char *ksc, const std::string &folder,
                          const std::vector<std::string> &tasks, const char *const *carrier)
{
    std::map<std::string, StallField> fields = stall_fields(ksc);
    if ((fields.count("stall") == 0u) || (fields.count("rd") == 0u) || (fields.count("ra") == 0u) ||
        (fields.count("rb") == 0u))
    {
        printf("orior_klq_identity curve: %s gives no stall field or not every register field\n", ksc);
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
    record_open(ksc, "curve");
    RunWritten table = {(folder + "/curve.txt"), 0, std::string()};
    // a chain of ours holds no slot, and the cross reads it whole (measuring_stick_query.c)
    static RunQuestion s_question;
    s_question.slot = RUN_SLOT_NONE;
    std::vector<unsigned int> places;
    stall_cases(&s_question, &places);
    unsigned long long asks = 0ull;
    std::vector<std::string> rows;
    for (const std::string &task : tasks)
    {
        std::vector<unsigned char> code;
        std::vector<Link> links;
        if ((host.count(task) == 0u) || !chain_code_read(engine, task, &code, &links))
        {
            printf("  %s: no chain of ours the host computes\n", task.c_str());
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
        unsigned int soonest_threads = 0u;
        unsigned long long soonest = 0ull;
        // the walk for the fewest starts at the registers the chain names where the engine gives them
        const unsigned int mark = chain_registers(engine, task);
        unsigned int fewest = (mark != 0u) ? mark : (named + 1u);
        for (unsigned int threads = 1u; threads <= CURVE_THREADS; threads *= 2u)
        {
            // the fewest registers the task answers alike with, walked up in blocks of one thread until the part says
            // yes; the registers code names are its own whatever its blocks hold, and a count of threads the part
            // refuses at the fewest it refuses at every count, since more registers take more of it
            std::map<long long, unsigned long long> times;
            const auto declaring_alike = [&](long long count) -> int {
                unsigned long long time = 0ull;
                const int alike = curve_point(code, host[task], places, &s_question, &asks, task, threads,
                                              (unsigned int)count, &time, &table);
                times[count] = time;
                return alike;
            };
            int answered = 0;
            if (threads == 1u)
            {
                // a register field holds an immediate's bits as well as a register's number: the count it answers at
                // is a bound, and the fewest is found under it by halving. Code that answers alike declaring some
                // count answers alike declaring more, in a block of one thread that every count fits
                const long long found = walk_step(declaring_alike, (long long)fewest, 1, (long long)highest);
                answered = (found <= (long long)highest);
                fewest = answered ? (unsigned int)walk_halved(declaring_alike, found, 0) : fewest;
            }
            else
            {
                answered = declaring_alike((long long)fewest);
            }
            const unsigned long long first = times[(long long)fewest];
            if (!answered)
            {
                printf("  %s: blocks of %u threads answer alike at no count of registers\n", task.c_str(), threads);
                break;
            }
            // the band of the fewest registers' times, asked again until it holds CURVE_SUSTAIN asks in a row without
            // widening
            CurveBand band{first, first};
            unsigned int held = 0u;
            for (unsigned int asked = 0u; (held < CURVE_SUSTAIN) && (asked < CURVE_ASKS_MOST); asked += 1u)
            {
                unsigned long long again = 0ull;
                if (curve_point(code, host[task], places, &s_question, &asks, task, threads, fewest, &again, &table))
                {
                    held = curve_band_widened(&band, again) ? 0u : (held + 1u);
                }
            }
            // a point inside the band is truthy, and one that answers apart or is refused is falsy. A point past the
            // band is bounced off the fewest: where the fewest strays past the band too, the band widens and the point
            // is asked again against it; where the fewest holds, a point past the band twice running is falsy
            const auto within_band = [&](long long count) -> int {
                unsigned int past = 0u;
                for (unsigned int asked = 0u; asked < CURVE_ASKS_MOST; asked += 1u)
                {
                    unsigned long long time = 0ull;
                    if (!curve_point(code, host[task], places, &s_question, &asks, task, threads, (unsigned int)count,
                                     &time, &table))
                    {
                        return 0;
                    }
                    if (time <= band.high)
                    {
                        return 1;
                    }
                    past += 1u;
                    if (past == 2u)
                    {
                        return 0;
                    }
                    unsigned long long bounced = 0ull;
                    if (curve_point(code, host[task], places, &s_question, &asks, task, threads, fewest, &bounced,
                                    &table) &&
                        curve_band_widened(&band, bounced))
                    {
                        past = 0u;
                    }
                }
                return 0;
            };
            const unsigned int truthy = (unsigned int)walk_halved(within_band, (long long)fewest, (long long)highest + 1);
            printf("  %s, blocks of %u threads: %llu to %llu ns a launch at %u registers, the knee at %u\n",
                   task.c_str(), threads, band.low, band.high, fewest, truthy);
            char row[128];
            snprintf(row, sizeof(row), "run answers %08x curve %s %u", truthy, task.c_str(), threads);
            rows.push_back(row);
            if ((soonest_threads == 0u) || (band.low < soonest))
            {
                soonest_threads = threads;
                soonest = band.low;
            }
        }
        if (soonest_threads != 0u)
        {
            printf("  %s: soonest in blocks of %u threads, %llu ns a launch\n", task.c_str(), soonest_threads, soonest);
            char row[128];
            snprintf(row, sizeof(row), "run answers %08x curve %s threads", soonest_threads, task.c_str());
            rows.push_back(row);
        }
    }
    if (!run_closed(&table))
    {
        printf("orior_klq_identity curve: %s/curve.txt could not be written\n", folder.c_str());
        run_channel_close();
        return 1;
    }
    run_channel_close();
    // each task's rows in place of that task's alone, every other task's curve kept
    for (const std::string &task : tasks)
    {
        const std::string asked = "curve " + task;
        std::vector<std::string> own;
        std::copy_if(rows.begin(), rows.end(), std::back_inserter(own), [&](const std::string &row) {
            return row.find(" " + asked + " ") != std::string::npos;
        });
        if (!ksc_answers_write(ksc, asked, own))
        {
            printf("orior_klq_identity curve: %s could not be written\n", ksc);
            return 1;
        }
    }
    printf("orior_klq_identity curve: %zu tasks over %llu asks, %zu answers written to %s\n", tasks.size(), asks,
           rows.size(), ksc);
    return 0;
}
