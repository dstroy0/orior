// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// curve.cu: the curve: the cost of registers to a task, timed on the part at each count of threads
#include "../../../../../../../includes/platform/platform.h"
#include "identity_internal.h"

// `band` widened to contain `time`: 1 where it had to widen, 0 where it contained `time` already
static int curve_band_widened(CurveBand *band, unsigned long long time)
{
    const int widened = (time < band->low) || (time > band->high);
    band->low = (time < band->low) ? time : band->low;
    band->high = (time > band->high) ? time : band->high;
    return widened;
}

// what every point of one task goes with. Its code, the host's answers, the cases' places, the question, the queries
// counted, the task, the count of threads, and the blob curve.txt
struct CurveTask
{
    const IdentityCode *code;
    const HostCases *host;
    const unsigned int *places;
    RunQuestion *question;
    unsigned long long *queries;
    const char *task;
    unsigned int threads;
    RunWritten *table;
};

// One point of a curve queried of the part. The task's code that declares `registers`, in blocks of its threads, as
// many blocks as CURVE_THREADS takes, timed over CURVE_LAUNCHES. 1 where it answers alike with the host, its time a
// launch into `nanoseconds`, and 0 where it answers apart or the part refuses it. Every point queried is a line of the
// table
static int curve_point(const CurveTask *task, unsigned int registers, unsigned long long *nanoseconds)
{
    RunQuestion *const question = task->question;
    question->registers = registers;
    question->threads = task->threads;
    question->blocks = CURVE_THREADS / task->threads;
    question->launches = CURVE_LAUNCHES;
    const int alike = question_alike(task->code, task->host, task->places, question, task->queries);
    *nanoseconds = alike ? (question->nanoseconds / CURVE_LAUNCHES) : 0ULL;
    VerbaLinea *const line = &task->table->text.line;
    mmgr_line_put(line, task->task);
    mmgr_line_put(line, " threads ");
    mmgr_line_decimal(line, task->threads, 1u);
    mmgr_line_put(line, " blocks ");
    mmgr_line_decimal(line, question->blocks, 1u);
    mmgr_line_put(line, " registers ");
    mmgr_line_decimal(line, registers, 1u);
    mmgr_line_put(line, ": ");
    mmgr_line_put(line, alike ? "alike" : "apart");
    mmgr_line_put(line, " ");
    mmgr_line_decimal(line, *nanoseconds, 1u);
    mmgr_line_put(line, " ns a launch");
    mmgr_line_put(line, alike ? "" : ", ");
    mmgr_line_put(line, alike ? "" : question->refused);
    mmgr_line_ch(line, '\n');
    return alike;
}

// a line on the output stream: the mode's name, then `first`, then `second`
static void curve_said(const char *first, const char *second)
{
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "orior_klq_identity curve: ");
    mmgr_line_put(output_line, first);
    mmgr_line_put(output_line, second);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
}

// the bytes of one row the curve writes, its terminator among them
#define CURVE_ROW_BYTES 128u

struct CurveRow
{
    char text[CURVE_ROW_BYTES];
};

// one count of registers a walk queried and the time a launch it took, 0 where it answered apart
struct CurveTime
{
    unsigned long long count;
    unsigned long long time;
};

// the time `times` holds of `count`, the latest it queried, and 0 where it queried none
static unsigned long long curve_time_of(const PlatformArray *times, unsigned long long count)
{
    for (unsigned long long at = times->count; at > 0ULL; at -= 1ULL)
    {
        const CurveTime *const time = (const CurveTime *)platform_array_at(times, at - 1ULL);
        if (time->count == count)
        {
            return time->time;
        }
    }
    return 0ULL;
}

// the places of the cases the curve goes over
static unsigned int s_curve_places[RUN_CASES_MAX];

// The cost of registers to a task, queried of the part, which times it. A task is a chain of ours the host also
// computes, with every stall the longest. For each count of threads a block contains, from 1, each twice the one
// before, the task goes over CURVE_THREADS threads in all. Its curve is its time against the registers its container
// declares: from the fewest it answers alike with to the max the register field names.
//
// The walk queries the fewest in blocks of one thread. It walks up from the registers the chain names until it answers
// alike, and halves down from there. Where the chain names none, it walks up from one past the highest number its
// register fields allow. It is the task's at every count of threads. The fewest's band is the least and the max of
// its times. The walk queries it again until it sustains CURVE_SUSTAIN queries in a row and does not widen.
//
// A count of registers is truthy where the task answers alike and its time a launch stays within the band. It is falsy
// where it answers apart or the part refuses the launch. It is falsy where its time is past the band twice running,
// and the fewest bounced between stays in the band. Where the bounced fewest strays past the band itself, the band
// widens and the walk queries the count again. A declaration of more registers takes residency and never gives it:
// every count past a falsy one is falsy. The walk halves to find the knee, the max registers truthy.
//
// The counts of threads end where the part answers no count of registers. Each knee goes to the .ksc as the part's
// answer on the run channel, `run answers <registers> curve <task> <threads>`. Each task's count of threads whose
// fewest registers run soonest goes as `run answers <threads> curve <task> threads`. The run's blob curve.txt contains
// every point queried
int identity_curve(const char *engine, const char *answers, const char *ksc, const char *folder,
                   const IdentityName *tasks, unsigned long long task_count, const char *const *carrier)
{
    StallFields fields;
    stall_fields(ksc, &fields);
    const StallField *const stall_field = field_of(&fields, "stall");
    const StallField *const rd_field = field_of(&fields, "rd");
    const StallField *const ra_field = field_of(&fields, "ra");
    const StallField *const rb_field = field_of(&fields, "rb");
    if ((stall_field == NULL) || (rd_field == NULL) || (ra_field == NULL) || (rb_field == NULL))
    {
        curve_said(ksc, " gives no stall field or not every register field");
        stall_fields_ended(&fields);
        return 1;
    }
    const StallField stall = *stall_field;
    const StallField registers[3] = {*rd_field, *ra_field, *rb_field};
    stall_fields_ended(&fields);
    const unsigned long long longest = (1ULL << stall.bits) - 1ULL;
    // a register field is far below 32 bits
    const unsigned int highest = (unsigned int)((1ULL << registers[0].bits) - 1ULL);
    HostAnswers host;
    if (!host_answers_read(answers, 1, &host))
    {
        curve_said(answers, ": the pool cannot meet the host's answers");
        return 1;
    }
    if (!run_channel_open(carrier, 60000000ULL))
    {
        host_answers_ended(&host);
        return 1;
    }
    record_open(ksc, "curve");
    RunWritten table;
    run_written_begun(&table, identity_path_joined(folder, "/curve.txt").text, 0);
    // a chain of ours contains no probe place, and the cross reads it whole (measuring_stick_query.c)
    static RunQuestion s_question;
    s_question.place = RUN_PLACE_NONE;
    stall_cases(&s_question, s_curve_places);
    unsigned long long queries = 0ULL;
    PlatformArray rows;
    platform_array_begun(&rows, sizeof(CurveRow), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    IdentityCode code;
    identity_code_begun(&code, PLATFORM_TIER_TEMPORARY);
    PlatformArray links;
    platform_array_begun(&links, sizeof(Link), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    PlatformArray times;
    platform_array_begun(&times, sizeof(CurveTime), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    for (unsigned long long task_at = 0ULL; task_at < task_count; task_at += 1ULL)
    {
        const char *const task = tasks[task_at].text;
        const HostCases *const task_host = host_cases_of(&host, task);
        if ((task_host == NULL) || !chain_code_read(engine, task, &code, &links))
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, task);
            mmgr_line_put(output_line, ": no chain of ours the host computes");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
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
        unsigned int soonest_threads = 0u;
        unsigned long long soonest = 0ULL;
        // the walk for the fewest starts at the registers the chain names where the engine gives them
        const unsigned int mark = chain_registers(engine, task);
        unsigned int fewest = (mark != 0u) ? mark : (named + 1u);
        CurveTask point = {&code, task_host, s_curve_places, &s_question, &queries, task, 1u, &table};
        for (unsigned int threads = 1u; threads <= CURVE_THREADS; threads *= 2u)
        {
            point.threads = threads;
            // The fewest registers the task answers alike with, walked up in blocks of one thread until the part
            // answers yes. The registers code names are its own whatever its blocks contain. A count of threads the
            // part refuses at the fewest it refuses at every count, since more registers take more of it
            (void)platform_array_counted(&times, 0ULL);
            const auto declaring_alike = [&](long long count) -> int {
                unsigned long long time = 0ULL;
                // a count of registers is below 2^32
                const int alike = curve_point(&point, (unsigned int)count, &time);
                const CurveTime kept = {(unsigned long long)count, time};
                (void)platform_array_appended(&times, &kept);
                return alike;
            };
            int answered = 0;
            if (threads == 1u)
            {
                // A register field stores an immediate's bits as well as a register's number. The count it answers at
                // is a bound, and the walk halves under it to find the fewest. Code that answers alike where it
                // declares some count answers alike where it declares more, in a block of one thread that every count
                // fits
                const long long found = walk_step(declaring_alike, (long long)fewest, 1, (long long)highest);
                answered = (found <= (long long)highest);
                // the fewest lies within the register field, below 2^32
                fewest = answered ? (unsigned int)walk_halved(declaring_alike, found, 0) : fewest;
            }
            else
            {
                answered = declaring_alike((long long)fewest);
            }
            const unsigned long long first = curve_time_of(&times, fewest);
            if (!answered)
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  ");
                mmgr_line_put(output_line, task);
                mmgr_line_put(output_line, ": blocks of ");
                mmgr_line_decimal(output_line, threads, 1u);
                mmgr_line_put(output_line, " threads answer alike at no count of registers");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
                break;
            }
            // the band of the fewest registers' times, queried again until it sustains CURVE_SUSTAIN queries in a row
            // without widening
            CurveBand band = {first, first};
            unsigned int sustained = 0u;
            for (unsigned int step = 0u; (sustained < CURVE_SUSTAIN) && (step < CURVE_QUERIES_MAX); step += 1u)
            {
                unsigned long long again = 0ULL;
                if (curve_point(&point, fewest, &again))
                {
                    sustained = curve_band_widened(&band, again) ? 0u : (sustained + 1u);
                }
            }
            // A point inside the band is truthy, and one that answers apart or that the part refuses is falsy. A point
            // past the band bounces off the fewest. Where the fewest strays past the band too, the band widens and the
            // walk queries the point again against it. Where the fewest stays within it, a point past the band twice
            // running is falsy
            const auto within_band = [&](long long count) -> int {
                unsigned int past = 0u;
                for (unsigned int step = 0u; step < CURVE_QUERIES_MAX; step += 1u)
                {
                    unsigned long long time = 0ULL;
                    // a count of registers is below 2^32
                    if (!curve_point(&point, (unsigned int)count, &time))
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
                    unsigned long long bounced = 0ULL;
                    if (curve_point(&point, fewest, &bounced) && curve_band_widened(&band, bounced))
                    {
                        past = 0u;
                    }
                }
                return 0;
            };
            // the knee lies within the register field, below 2^32
            const unsigned int truthy =
                (unsigned int)walk_halved(within_band, (long long)fewest, (long long)highest + 1);
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  ");
                mmgr_line_put(output_line, task);
                mmgr_line_put(output_line, ", blocks of ");
                mmgr_line_decimal(output_line, threads, 1u);
                mmgr_line_put(output_line, " threads: ");
                mmgr_line_decimal(output_line, band.low, 1u);
                mmgr_line_put(output_line, " to ");
                mmgr_line_decimal(output_line, band.high, 1u);
                mmgr_line_put(output_line, " ns a launch at ");
                mmgr_line_decimal(output_line, fewest, 1u);
                mmgr_line_put(output_line, " registers, the knee at ");
                mmgr_line_decimal(output_line, truthy, 1u);
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            CurveRow row;
            VerbaLinea row_line = mmgr_line_over(row.text, sizeof(row.text));
            mmgr_line_put(&row_line, "run answers ");
            mmgr_line_hex(&row_line, truthy, 8u);
            mmgr_line_put(&row_line, " curve ");
            mmgr_line_put(&row_line, task);
            mmgr_line_put(&row_line, " ");
            mmgr_line_decimal(&row_line, threads, 1u);
            (void)mmgr_line_finish(&row_line);
            (void)platform_array_appended(&rows, &row);
            if ((soonest_threads == 0u) || (band.low < soonest))
            {
                soonest_threads = threads;
                soonest = band.low;
            }
        }
        if (soonest_threads != 0u)
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  ");
                mmgr_line_put(output_line, task);
                mmgr_line_put(output_line, ": soonest in blocks of ");
                mmgr_line_decimal(output_line, soonest_threads, 1u);
                mmgr_line_put(output_line, " threads, ");
                mmgr_line_decimal(output_line, soonest, 1u);
                mmgr_line_put(output_line, " ns a launch");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            CurveRow row;
            VerbaLinea row_line = mmgr_line_over(row.text, sizeof(row.text));
            mmgr_line_put(&row_line, "run answers ");
            mmgr_line_hex(&row_line, soonest_threads, 8u);
            mmgr_line_put(&row_line, " curve ");
            mmgr_line_put(&row_line, task);
            mmgr_line_put(&row_line, " threads");
            (void)mmgr_line_finish(&row_line);
            (void)platform_array_appended(&rows, &row);
        }
    }
    platform_array_ended(&times);
    platform_array_ended(&code);
    platform_array_ended(&links);
    host_answers_ended(&host);
    const int closed = run_closed(&table);
    run_written_ended(&table);
    if (!closed)
    {
        curve_said(folder, "/curve.txt could not be written");
        run_channel_close();
        platform_array_ended(&rows);
        return 1;
    }
    run_channel_close();
    // each task's rows in place of that task's alone, every other task's curve retained
    const char **const own =
        (const char **)platform_cell_taken(((rows.count != 0ULL) ? rows.count : 1ULL) * sizeof(const char *),
                                           platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    for (unsigned long long task_at = 0ULL; (own != NULL) && (task_at < task_count); task_at += 1ULL)
    {
        const char *const task = tasks[task_at].text;
        char step[CARRIER_TEXT_MAX];
        VerbaLinea step_line = mmgr_line_over(step, sizeof(step));
        mmgr_line_put(&step_line, "curve ");
        mmgr_line_put(&step_line, task);
        (void)mmgr_line_finish(&step_line);
        char marked[CARRIER_TEXT_MAX + 2u];
        VerbaLinea marked_line = mmgr_line_over(marked, sizeof(marked));
        mmgr_line_ch(&marked_line, ' ');
        mmgr_line_put(&marked_line, step);
        mmgr_line_ch(&marked_line, ' ');
        (void)mmgr_line_finish(&marked_line);
        unsigned int own_count = 0u;
        for (unsigned long long at = 0ULL; at < rows.count; at += 1ULL)
        {
            const CurveRow *const row = (const CurveRow *)platform_array_at(&rows, at);
            if (identity_has(row->text, marked))
            {
                own[own_count] = row->text;
                own_count += 1u;
            }
        }
        if (!ksc_answers_write(ksc, step, own, own_count))
        {
            curve_said(ksc, " could not be written");
            platform_cell_released(own, platform_place_g_program_owner);
            platform_array_ended(&rows);
            return 1;
        }
    }
    if (own == NULL)
    {
        curve_said(ksc, ": the pool cannot meet the rows, and nothing is written");
        platform_array_ended(&rows);
        return 1;
    }
    platform_cell_released(own, platform_place_g_program_owner);
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "orior_klq_identity curve: ");
    mmgr_line_decimal(output_line, task_count, 1u);
    mmgr_line_put(output_line, " tasks over ");
    mmgr_line_decimal(output_line, queries, 1u);
    mmgr_line_put(output_line, " queries, ");
    mmgr_line_decimal(output_line, rows.count, 1u);
    mmgr_line_put(output_line, " answers written to ");
    mmgr_line_put(output_line, ksc);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    platform_array_ended(&rows);
    return 0;
}
