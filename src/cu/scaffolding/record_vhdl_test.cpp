// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// The record machine's lane as VHDL checked against the host oracle word for word (engine_table.md item 11(f), VHDL a
// language of the register lane). record_test's programs, drawn from the same stream in the same order, are
// encoded, arranged and run by cycle_record_run_host over their first VHDL_TEST_LANES lanes. VhdlTarget
// (codegen/rulesets/vhdl.krs) writes each as VHDL, and GHDL analyzes and runs it under record_vhdl_bench.vhd. The bench
// reads a memory image that the test arranges as the device arranges its launch (record_image.h). The emitted
// cycle_program_unit runs the lanes as the device's resident kernel runs them, and the test compares its records with
// the host's word for word. Synthesis takes the program unit whole. The lines give the same input digests as the host
// test's. A program whose lane the code generator does not write counts as not supported, and neither passes nor
// fails.
// Where the caller gives a construction set, the set refines each program's lane (engine_table M23). The loop splits
// the lane at the budgets that it proposes, checks each schedule against the host, and keeps the least cost among those
// that pass.
//
// The test runs in three stages. First it writes each schedule of each program, a job, into a folder of its own under
// the work folder. Then the platform's workers run GHDL and Yosys on every job at once, one worker for each processor.
// Last, the test reads what each job gave, in the order of the programs, and makes its checks.
#include "../../../includes/file_defs/krep/krep.h"
#include "../../../utils/test/src/cu/engine/analysis/cycle/record_image.h"
#include "../../platform/platform.h"
#include "../engine/engine_note.h"
#include "cellularum_laboro/cellularum_linea.h"
#include "memoria_operor/memoria_linea.h"
#include "vhdl_target.h"
#include "yosys_script.h"

#include <stddef.h>

// The lanes of a program that GHDL runs: the first lanes of the host test's, which the host oracle runs as well
#define VHDL_TEST_LANES 256u

static_assert(VHDL_TEST_LANES <= HOST_TEST_LANES, "the VHDL test runs at max the host test's lanes");

// the programs of record_image_programs, and the rounds of the refinement loop of one program, each with room past it
#define VHDL_TEST_PROGRAMS_MAX 16u
#define VHDL_TEST_ROUNDS_MAX 40u

// the jobs of the test: the two ends and the rounds of each program
#define VHDL_TEST_JOBS_MAX (VHDL_TEST_PROGRAMS_MAX * (2u + VHDL_TEST_ROUNDS_MAX))

// the workers that run the jobs at max
#define VHDL_TEST_WORKERS_MAX 64u

// the memory that synthesis takes a lane over, in words: synthesis reads no image, and a small memory keeps the
// netlist small
#define VHDL_TEST_SYNTHESIS_WORDS 256u

// the bytes of what a check names, its terminator among them
#define VHDL_TEST_WHAT_BYTES 512u

// the bytes a script or a lane's text holds at max
#define VHDL_TEST_TEXT_BYTES (1ULL << 30u)

// the types of a job: the lane split where it must be, every form alone, and a round of the refinement loop
#define VHDL_JOB_SPLIT 0u
#define VHDL_JOB_ALONE 1u
#define VHDL_JOB_ROUND 2u

struct VhdlResults
{
    unsigned int checks;
    unsigned int failed;
    unsigned int unsupported;
};

// Where GHDL runs: the folder that holds the jobs' folders, and the bench's file. 1 where it also synthesizes each
// lane, with the Yosys script that each job's folder receives. Also 1 where the caller gave a construction set, its
// forms' costs (a ScheduleCost each over the cell of their names), and the dearest of them. That cost bounds the
// budgets that the refinement loop proposes
struct VhdlPlace
{
    const char *work;
    const char *bench;
    int synthesis;
    const char *script;
    unsigned long long script_length;
    int constructed;
    PlatformArray costs;
    char *names;
    unsigned int budget;
};

// What one program keeps until the test reads its jobs: its name, steps and out limbs, and the host's records and
// their digest. Also the inputs' digest, 1 where the host must error on it, and 1 where program() is its lane split
// where it must be. Its jobs are the `jobs` after `first_job`. `room` is 0 where the table of jobs could not hold
// them all
struct VhdlProgram
{
    char name[VHDL_TEST_WHAT_BYTES];
    unsigned int steps;
    unsigned int out_limbs;
    unsigned int *host_records;
    unsigned long long record_words;
    int host_ran;
    unsigned long long host_digest;
    unsigned long long inputs;
    int errors;
    int split_same;
    unsigned int first_job;
    unsigned int jobs;
    int room;
};

// One schedule of one program as a job: its program, its type, its budget and round where it is a round, and the
// schedule's report. Also its lines of VHDL and the image's words. `written` is 1 where its folder holds its files.
// The two statuses are the exit codes of GHDL's run and of the synthesis, -1 where they did not run
struct VhdlJob
{
    unsigned int program;
    unsigned int type;
    unsigned int budget;
    unsigned int round;
    ScheduleReport report;
    unsigned long long lines;
    unsigned long long words;
    int written;
    long status;
    long synthesis_status;
};

// The programs and the jobs of the run. The calls of this module share them, and the workers read the jobs while the
// test waits for them
struct VhdlTable
{
    VhdlProgram program[VHDL_TEST_PROGRAMS_MAX];
    unsigned int programs;
    VhdlJob job[VHDL_TEST_JOBS_MAX];
    unsigned int jobs;
};

static VhdlTable s_vhdl_table;

// The construction set at `path` into `place`: each form's cost by its name, and the largest. 0 where the call cannot
// read it whole
static int vhdl_construction(const char *path, VhdlPlace *place)
{
    KrepFormTable table;
    EngineError error;
    mmgr_bytes_set(&error, 0, sizeof(error));
    if (krep_forms_read(path, &table, &error) == 0)
    {
        return 0;
    }
    const unsigned long long name_bytes = 8ULL * table.name_words;
    place->names = (char *)platform_cell_cleared(name_bytes + 1ULL, 1ULL, platform_place_g_program_owner, 0);
    if (place->names == NULL)
    {
        krep_forms_release(&table);
        return 0;
    }
    for (unsigned long long at = 0ULL; at < name_bytes; at += 1ULL)
    {
        place->names[at] = (char)((table.words[table.forms + (at / 8ULL)] >> (8ULL * (at % 8ULL))) & 0xFFULL);
    }
    platform_array_begun(&place->costs, sizeof(ScheduleCost), platform_place_g_program_owner, 0);
    // the set's costs in one take at their full size
    int ok = platform_array_counted(&place->costs, table.forms) && platform_array_counted(&place->costs, 0ULL);
    unsigned long long start = 0ULL;
    for (unsigned long long form = 0ULL; (ok != 0) && (form < table.forms) && (start < name_bytes); form += 1ULL)
    {
        // a form's cost is a path's length in cells, far under 2^32
        const ScheduleCost cost = {&place->names[start], (unsigned int)table.words[form]};
        ok = platform_array_appended(&place->costs, &cost);
        place->budget = (cost.cost > place->budget) ? cost.cost : place->budget;
        start += mmgr_text_length(&place->names[start], (size_t)(name_bytes - start)) + 1ULL;
    }
    krep_forms_release(&table);
    place->constructed = ok;
    return ok;
}

static void vhdl_check(VhdlResults *results, int passed, const char *what)
{
    results->checks += 1u;
    results->failed += passed ? 0u : 1u;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, passed ? "ok  " : "FAIL");
        mmgr_line_put(output_line, " ");
        mmgr_line_put(output_line, what);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
}

// a check whose words are `name` then `rest`
static void vhdl_named_check(VhdlResults *results, int passed, const char *name, const char *rest)
{
    char what[VHDL_TEST_WHAT_BYTES];
    VerbaLinea line = mmgr_line_over(what, sizeof(what));
    mmgr_line_put(&line, name);
    mmgr_line_put(&line, rest);
    (void)mmgr_line_finish(&line);
    vhdl_check(results, passed, what);
}

// the file `name` in the folder `work` into `path`, `room` bytes of it
static const char *vhdl_path(const char *work, const char *name, char *path, size_t room)
{
    VerbaLinea line = mmgr_line_over(path, room);
    mmgr_line_put(&line, work);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, name);
    (void)mmgr_line_finish(&line);
    return path;
}

// the folder of job `job` into `path`, `room` bytes of it
static const char *vhdl_job_folder(const VhdlPlace *place, unsigned int job, char *path, size_t room)
{
    VerbaLinea line = mmgr_line_over(path, room);
    mmgr_line_put(&line, place->work);
    mmgr_line_put(&line, "/job_");
    mmgr_line_decimal(&line, job, 1u);
    (void)mmgr_line_finish(&line);
    return path;
}

// the number after `label` on the first line of `path` that contains it; 0 where none does
static unsigned long long vhdl_after(const char *path, const char *label)
{
    PlatformFile *const file = platform_file_open(path, "rb");
    if (file == NULL)
    {
        return 0ULL;
    }
    char line[1024];
    unsigned long long found = 0ULL;
    while ((found == 0ULL) && (platform_file_text_read(line, sizeof(line), file) != NULL))
    {
        const char *const at = mmgr_text_find(line, label, PLATFORM_TEXT_TO_TERMINATOR);
        found = (at != NULL)
                    ? mmgr_text_unsigned_in(at + mmgr_text_length(label, PLATFORM_TEXT_TO_TERMINATOR), NULL, 10)
                    : 0ULL;
    }
    platform_file_close(file);
    return found;
}

// the lines of `path` that contain `label`
static unsigned long long vhdl_lines(const char *path, const char *label)
{
    PlatformFile *const file = platform_file_open(path, "rb");
    if (file == NULL)
    {
        return 0ULL;
    }
    char line[1024];
    unsigned long long found = 0ULL;
    while (platform_file_text_read(line, sizeof(line), file) != NULL)
    {
        found += (mmgr_text_find(line, label, PLATFORM_TEXT_TO_TERMINATOR) != NULL) ? 1ULL : 0ULL;
    }
    platform_file_close(file);
    return found;
}

// the first 20 lines of the log at `path` on stdout, as head shows them. A read that ends in no newline is part of a
// line
static void vhdl_log_shown(const char *path)
{
    PlatformFile *const log = platform_file_open(path, "r");
    char log_text[1024];
    unsigned int shown = 0u;
    while ((log != NULL) && (shown < 20u) && (platform_file_text_read(log_text, (int)sizeof(log_text), log) != NULL))
    {
        mmgr_line_put(platform_line_out(), log_text);
        const size_t length = mmgr_text_length(log_text, PLATFORM_TEXT_TO_TERMINATOR);
        shown += ((length != 0u) && (log_text[length - 1u] == '\n')) ? 1u : 0u;
    }
    if (log != NULL)
    {
        (void)platform_file_close(log);
    }
    platform_line_flushed();
}

// the newlines of a text
static unsigned long long vhdl_line_count(const char *text, unsigned long long length)
{
    unsigned long long lines = 0ULL;
    for (unsigned long long at = 0ULL; at < length; at += 1ULL)
    {
        lines += (text[at] == '\n') ? 1ULL : 0ULL;
    }
    return lines;
}

// the FNV-1a digest of the `length` bytes of `text`
static unsigned long long vhdl_text_digest(const char *text, unsigned long long length)
{
    unsigned long long digest = HOST_TEST_FNV_BASIS;
    for (unsigned long long at = 0ULL; at < length; at += 1ULL)
    {
        digest = (digest ^ (unsigned char)text[at]) * 0x100000001B3ULL;
    }
    return digest;
}

// ---- the first stage: each schedule written as a job ----

// The `length` bytes of `text` as a job of program `program`. The call makes its folder, with the program as
// program.vhd and the image as memory.txt. Where the place synthesizes, the folder also gets the Yosys script as
// synth.ys. 0 where the table of jobs is full
static int vhdl_job_added(const VhdlPlace *place, unsigned int program, unsigned int type, unsigned int budget,
                          unsigned int round, const char *text, unsigned long long length, const ScheduleReport *report,
                          const RecordImage *image)
{
    VhdlTable *const table = &s_vhdl_table;
    if (table->jobs >= VHDL_TEST_JOBS_MAX)
    {
        return 0;
    }
    const unsigned int index = table->jobs;
    VhdlJob *const job = &table->job[index];
    mmgr_bytes_set(job, 0, sizeof(*job));
    job->program = program;
    job->type = type;
    job->budget = budget;
    job->round = round;
    job->report = *report;
    job->lines = (text != NULL) ? vhdl_line_count(text, length) : 0ULL;
    job->words = image->memory.count;
    job->status = -1L;
    job->synthesis_status = -1L;
    char folder[PLATFORM_PATH_BYTES];
    char program_path[PLATFORM_PATH_BYTES];
    char memory_path[PLATFORM_PATH_BYTES];
    char script_path[PLATFORM_PATH_BYTES];
    (void)vhdl_job_folder(place, index, folder, sizeof(folder));
    (void)vhdl_path(folder, "program.vhd", program_path, sizeof(program_path));
    (void)vhdl_path(folder, "memory.txt", memory_path, sizeof(memory_path));
    (void)vhdl_path(folder, "synth.ys", script_path, sizeof(script_path));
    job->written = (text != NULL) && (length != 0ULL) && platform_folder_made(folder) &&
                   platform_file_written(program_path, text, length) && record_image_write(memory_path, image) &&
                   ((place->synthesis == 0) || platform_file_written(script_path, place->script, place->script_length));
    table->jobs += 1u;
    return 1;
}

// The lane split at the two ends of what a construction set can require, each written as a job. One end splits the
// lane only where it must, and the other puts every form alone in a state of its own. `program_text` is program()'s
// lane, which the first end must equal
static void vhdl_ends_written(const VhdlPlace *place, unsigned int index, VhdlProgram *program,
                              const EngineRecordLayout *layout, VhdlTarget &generator, const TargetInfo *target,
                              const char *program_text, unsigned long long program_length, const RecordImage *image)
{
    const ScheduleModel ends[2] = {{NULL, 0u, 0u, SCHEDULE_UNBOUNDED, SCHEDULE_UNBOUNDED},
                                   {NULL, 0u, 1u, 1u, SCHEDULE_UNBOUNDED}};
    PlatformText scheduled;
    platform_text_begun(&scheduled, VHDL_TEST_TEXT_BYTES, platform_place_g_program_owner, PLATFORM_TIER_MOVABLE);
    for (unsigned int end = 0u; end < 2u; end += 1u)
    {
        ScheduleReport report = {0u, 0u, 0u};
        unsigned int places = 0u;
        unsigned int live = 0u;
        (void)generator.scheduled(layout, target, "", &scheduled, &places, &live, ends[end], &report);
        const unsigned long long scheduled_length = platform_text_size(&scheduled);
        const char *const scheduled_text = platform_text_bytes(&scheduled);
        if (end == 0u)
        {
            program->split_same = (program_text != NULL) && (scheduled_text != NULL) &&
                                  (program_length == scheduled_length) &&
                                  (mmgr_bytes_compare(program_text, scheduled_text, (size_t)program_length) == 0);
        }
        const unsigned int job = s_vhdl_table.jobs;
        program->room = program->room &&
                        vhdl_job_added(place, index, end, 0u, 0u, scheduled_text, scheduled_length, &report, image);
        // where program() is not the split end, its lane goes beside the job's program.vhd as whole.vhd, for a diff
        if ((end == 0u) && (program->split_same == 0) && (program->room != 0) && (program_text != NULL))
        {
            char folder[PLATFORM_PATH_BYTES];
            char whole_path[PLATFORM_PATH_BYTES];
            (void)vhdl_job_folder(place, job, folder, sizeof(folder));
            (void)vhdl_path(folder, "whole.vhd", whole_path, sizeof(whole_path));
            (void)platform_file_written(whole_path, program_text, program_length);
        }
    }
    platform_text_ended(&scheduled);
}

// 1 where an earlier round of this program wrote the `length` bytes of `text`. That is a job among `rounds` of the
// same length and digest whose program.vhd holds the same bytes
static int vhdl_round_before(const VhdlPlace *place, const unsigned int *rounds, const unsigned long long *digests,
                             unsigned int count, const char *text, unsigned long long length, unsigned long long digest)
{
    int before = 0;
    for (unsigned int at = 0u; (before == 0) && (at < count); at += 1u)
    {
        const VhdlJob *const job = &s_vhdl_table.job[rounds[at]];
        if ((digests[at] != digest) || (job->written == 0))
        {
            continue;
        }
        char folder[PLATFORM_PATH_BYTES];
        char program_path[PLATFORM_PATH_BYTES];
        (void)vhdl_job_folder(place, rounds[at], folder, sizeof(folder));
        (void)vhdl_path(folder, "program.vhd", program_path, sizeof(program_path));
        unsigned long long size = 0ULL;
        // the earlier text is ephemeral work, given back before the next
        char *const earlier =
            ((platform_file_size(program_path, &size) != 0) && (size == length))
                ? (char *)platform_cell_taken(size + 1ULL, platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY)
                : NULL;
        unsigned long long read = 0ULL;
        before = (earlier != NULL) && platform_file_read(program_path, earlier, size, &read) && (read == length) &&
                 (mmgr_bytes_compare(earlier, text, (size_t)length) == 0);
        platform_cell_released(earlier, platform_place_g_program_owner);
    }
    return before;
}

// The refinement loop (engine_table M23), where the caller gave a construction set, each round written as a job. The
// generator is the schedule, and its knob is the budget that splits a state. The loop proposes the budget from 1, and
// doubles it until it passes four times the dearest form. The loop does not try again a budget whose lane is the text
// of an earlier round
static void vhdl_rounds_written(const VhdlPlace *place, unsigned int index, VhdlProgram *program,
                                const EngineRecordLayout *layout, VhdlTarget &generator, const TargetInfo *target,
                                const RecordImage *image)
{
    unsigned int rounds[VHDL_TEST_ROUNDS_MAX];
    unsigned long long digests[VHDL_TEST_ROUNDS_MAX];
    unsigned int count = 0u;
    PlatformText scheduled;
    platform_text_begun(&scheduled, VHDL_TEST_TEXT_BYTES, platform_place_g_program_owner, PLATFORM_TIER_MOVABLE);
    for (unsigned int budget = 1u; (budget <= (4u * place->budget)) && (count < VHDL_TEST_ROUNDS_MAX); budget *= 2u)
    {
        // a construction set names far fewer than 2^32 forms
        const ScheduleModel model = {(const ScheduleCost *)place->costs.cell, (unsigned int)place->costs.count, 0u,
                                     budget, SCHEDULE_UNBOUNDED};
        ScheduleReport report = {0u, 0u, 0u};
        unsigned int places = 0u;
        unsigned int live = 0u;
        (void)generator.scheduled(layout, target, "", &scheduled, &places, &live, model, &report);
        const unsigned long long scheduled_length = platform_text_size(&scheduled);
        const char *const scheduled_text = platform_text_bytes(&scheduled);
        const unsigned long long digest =
            (scheduled_text != NULL) ? vhdl_text_digest(scheduled_text, scheduled_length) : 0ULL;
        if ((scheduled_text != NULL) &&
            vhdl_round_before(place, rounds, digests, count, scheduled_text, scheduled_length, digest))
        {
            continue;
        }
        const unsigned int job = s_vhdl_table.jobs;
        const int added = (program->room != 0) && vhdl_job_added(place, index, VHDL_JOB_ROUND, budget, count + 1u,
                                                                 scheduled_text, scheduled_length, &report, image);
        program->room = added;
        if (added != 0)
        {
            rounds[count] = job;
            digests[count] = digest;
            count += 1u;
        }
    }
    platform_text_ended(&scheduled);
}

// what each run of a program is given: the results and the place
struct VhdlRun
{
    VhdlResults *results;
    const VhdlPlace *place;
};

// The program arranged and run on the host over the test's lanes, and written as VHDL. Each of its schedules goes as
// a job over the same atoms. `errors` is 1 for a program the host must error: the lane as VHDL must then error on a
// lane
static void vhdl_run(void *context, const HostProgram *host_program, int reuse, unsigned int *const *atoms,
                     const unsigned long long *bodies, const unsigned int *index, int errors)
{
    VhdlResults *const results = ((VhdlRun *)context)->results;
    const VhdlPlace *const place = ((VhdlRun *)context)->place;
    VhdlTable *const table = &s_vhdl_table;
    if (table->programs >= VHDL_TEST_PROGRAMS_MAX)
    {
        vhdl_named_check(results, 0, host_program->name, " has room in the test's table of programs");
        return;
    }
    const unsigned int program_index = table->programs;
    VhdlProgram *const program = &table->program[program_index];
    mmgr_bytes_set(program, 0, sizeof(*program));
    program->errors = errors;
    program->room = 1;
    VerbaLinea name_line = mmgr_line_over(program->name, sizeof(program->name));
    mmgr_line_put(&name_line, host_program->name);
    mmgr_line_put(&name_line, reuse ? " (registers reused)" : "");
    (void)mmgr_line_finish(&name_line);
    HostLoaded loaded;
    if (host_load(host_program, reuse, &loaded) == 0)
    {
        vhdl_named_check(results, 0, program->name, " is arranged");
        return;
    }
    const EngineRecordLayout *const layout = &loaded.layout;
    program->steps = layout->steps;
    program->out_limbs = layout->out_limbs;
    program->record_words = (unsigned long long)VHDL_TEST_LANES * layout->out_limbs;
    // the host's records are a resource of the run: the test reads them in its last stage
    program->host_records =
        (unsigned int *)platform_cell_cleared((program->record_words != 0ULL) ? program->record_words : 1ULL,
                                              sizeof(unsigned int), platform_place_g_program_owner, 0);
    if (program->host_records == NULL)
    {
        vhdl_named_check(results, 0, program->name, " has room for the host's records");
        host_free(&loaded);
        return;
    }
    CycleRecordHostRequest request;
    mmgr_bytes_set(&request, 0, sizeof(request));
    request.layout = layout;
    for (unsigned int member = 0u; member < layout->members; member += 1u)
    {
        request.in[member] = atoms[member];
        request.bodies[member] = bodies[member];
    }
    request.index = index;
    request.count = VHDL_TEST_LANES;
    request.out = program->host_records;
    request.error = &loaded.error;
    const long ran = cycle_record_run_host(&request);
    program->inputs = HOST_TEST_FNV_BASIS;
    for (unsigned int member = 0u; member < layout->members; member += 1u)
    {
        program->inputs ^= host_digest(atoms[member], bodies[member] * layout->in_limbs[member]);
    }
    VhdlTarget &generator = vhdl_target();
    const TargetInfo target = {0ULL, 0, 0, 0, 0, ""};
    unsigned int places = 0u;
    unsigned int live = 0u;
    PlatformText text;
    platform_text_begun(&text, VHDL_TEST_TEXT_BYTES, platform_place_g_program_owner, PLATFORM_TIER_MOVABLE);
    const int wrote = (generator.ruleset(1) != NULL) && generator.program(layout, &target, "", &text, &places, &live);
    RecordImage image;
    mmgr_bytes_set(&image, 0, sizeof(image));
    const int imaged = (wrote != 0) && record_image(layout, atoms, bodies, index, VHDL_TEST_LANES, &image);
    if (wrote == 0)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, program->name);
            mmgr_line_put(output_line, ": ");
            mmgr_line_decimal(output_line, layout->steps, 1u);
            mmgr_line_put(output_line,
                          " steps, not supported in VHDL (a step the lane does not support, or its ruleset errored)");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        results->unsupported += 1u;
    }
    else if (imaged == 0)
    {
        vhdl_named_check(results, 0, program->name, " has room for its memory image");
    }
    else
    {
        program->host_ran = ran == (long)VHDL_TEST_LANES;
        program->host_digest = program->host_ran ? host_digest(program->host_records, program->record_words) : 0ULL;
        program->first_job = table->jobs;
        // the call reads program()'s lane once, before the schedules' texts come
        const unsigned long long program_length = platform_text_size(&text);
        const char *const program_text = platform_text_bytes(&text);
        vhdl_ends_written(place, program_index, program, layout, generator, &target, program_text, program_length,
                          &image);
        if (place->constructed != 0)
        {
            vhdl_rounds_written(place, program_index, program, layout, generator, &target, &image);
        }
        program->jobs = table->jobs - program->first_job;
        table->programs += 1u;
    }
    record_image_ended(&image);
    platform_text_ended(&text);
    if ((wrote == 0) || (imaged == 0))
    {
        platform_cell_released(program->host_records, platform_place_g_program_owner);
    }
    host_free(&loaded);
}

// ---- the second stage: the jobs run by the platform's workers ----

// GHDL's analysis and run of one job in its folder, and its synthesis where the place requires it and the run ended
// well. Each program writes into the job's own logs
static void vhdl_job_run(const VhdlPlace *place, unsigned int index)
{
    VhdlJob *const job = &s_vhdl_table.job[index];
    if (job->written == 0)
    {
        return;
    }
    char folder[PLATFORM_PATH_BYTES];
    char ghdl_log[PLATFORM_PATH_BYTES];
    (void)vhdl_job_folder(place, index, folder, sizeof(folder));
    (void)vhdl_path(folder, "ghdl.log", ghdl_log, sizeof(ghdl_log));
    char words[32];
    VerbaLinea words_line = mmgr_line_over(words, sizeof(words));
    mmgr_line_put(&words_line, "-gwords=");
    mmgr_line_decimal(&words_line, job->words, 1u);
    (void)mmgr_line_finish(&words_line);
    const char *const analysis[] = {"ghdl", "-a", "--std=08", "program.vhd", place->bench, NULL};
    const char *const run[] = {"ghdl", "--elab-run", "--std=08", "record_vhdl_bench", words, NULL};
    long status = platform_program_run_in(analysis, folder, ghdl_log, NULL, 0);
    status = (status == 0L) ? platform_program_run_in(run, folder, ghdl_log, NULL, 1) : status;
    job->status = status;
    if ((place->synthesis == 0) || (status != 0L))
    {
        return;
    }
    // GHDL writes the Verilog of the program to synth.v and its errors to synth.log. Yosys then reads synth.ys
    char synthesis_words[32];
    VerbaLinea synthesis_line = mmgr_line_over(synthesis_words, sizeof(synthesis_words));
    mmgr_line_put(&synthesis_line, "-gwords=");
    mmgr_line_decimal(&synthesis_line, VHDL_TEST_SYNTHESIS_WORDS, 1u);
    (void)mmgr_line_finish(&synthesis_line);
    const char *const synthesis[] = {
        "ghdl", "--synth", "--std=08", synthesis_words, "--out=verilog", "program.vhd", "-e", "cycle_program_unit",
        NULL};
    const char *const yosys[] = {"yosys", "-s", "synth.ys", NULL};
    char verilog[PLATFORM_PATH_BYTES];
    char synthesis_log[PLATFORM_PATH_BYTES];
    char yosys_log[PLATFORM_PATH_BYTES];
    (void)vhdl_path(folder, "synth.v", verilog, sizeof(verilog));
    (void)vhdl_path(folder, "synth.log", synthesis_log, sizeof(synthesis_log));
    (void)vhdl_path(folder, "yosys.log", yosys_log, sizeof(yosys_log));
    long synthesized = platform_program_run_in(synthesis, folder, verilog, synthesis_log, 0);
    synthesized = (synthesized == 0L) ? platform_program_run_in(yosys, folder, yosys_log, NULL, 0) : synthesized;
    job->synthesis_status = synthesized;
}

// a worker's share of the jobs: every `stride`th job from `first`
struct VhdlTurn
{
    const VhdlPlace *place;
    unsigned int first;
    unsigned int stride;
};

static void vhdl_turn_run(void *context)
{
    const VhdlTurn *const turn = (const VhdlTurn *)context;
    for (unsigned int index = turn->first; index < s_vhdl_table.jobs; index += turn->stride)
    {
        vhdl_job_run(turn->place, index);
    }
}

// the workers of the second stage and their shares. The calls of this module share them
static PlatformWorker s_vhdl_workers[VHDL_TEST_WORKERS_MAX];
static VhdlTurn s_vhdl_turns[VHDL_TEST_WORKERS_MAX];

// Every job run, one worker for each processor at max. A share whose worker the system refused runs on this thread
static void vhdl_jobs_run(const VhdlPlace *place)
{
    const unsigned int processors = platform_processors();
    unsigned int count = (processors < VHDL_TEST_WORKERS_MAX) ? processors : VHDL_TEST_WORKERS_MAX;
    count = (count < s_vhdl_table.jobs) ? count : s_vhdl_table.jobs;
    int started[VHDL_TEST_WORKERS_MAX];
    for (unsigned int worker = 0u; worker < count; worker += 1u)
    {
        s_vhdl_turns[worker] = {place, worker, count};
        started[worker] = platform_worker_started(&s_vhdl_workers[worker], vhdl_turn_run, &s_vhdl_turns[worker]);
    }
    for (unsigned int worker = 0u; worker < count; worker += 1u)
    {
        if (started[worker] == 0)
        {
            vhdl_turn_run(&s_vhdl_turns[worker]);
        }
    }
    for (unsigned int worker = 0u; worker < count; worker += 1u)
    {
        if (started[worker] != 0)
        {
            platform_worker_joined(&s_vhdl_workers[worker]);
        }
    }
}

// ---- the third stage: what each job gave, read and checked ----

// What one job gave. Its read is 1 where the test read the records of GHDL's run whole. It holds the lanes that
// errored, the records' words in a cell of the program's record words, and the clocks the lanes ran. Where GHDL and
// Yosys both ran, synthesized is 1, with the memories that Yosys inferred and the read ports that it registered. Its
// cells and its longest path between registers, in cells, come with them. Its cost is the clocks times that path, the
// lanes' run in cells of delay, or the clocks where the test did not synthesize it
struct VhdlTrial
{
    int read;
    unsigned int error;
    unsigned int *records;
    unsigned long long clocks;
    int synthesized;
    unsigned long long memories;
    unsigned long long registered;
    unsigned long long cells;
    unsigned long long path;
    unsigned long long cost;
};

// The job's records, errors and clocks read from its folder, and its synthesis's logs where it synthesized. The
// memories Yosys inferred are its $mem_v2 cells where its synthesis stops before the fine stage. The generic fine
// stage maps each memory to flops. Yosys registered a read port where it took the flops its word lands in into the
// port, as a block RAM's read does. The caller gives back trial->records
static void vhdl_job_read(const VhdlPlace *place, unsigned int index, const VhdlProgram *program, VhdlTrial *trial)
{
    const VhdlJob *const job = &s_vhdl_table.job[index];
    mmgr_bytes_set(trial, 0, sizeof(*trial));
    // the records are ephemeral work, given back before the next job's
    trial->records = (unsigned int *)platform_cell_cleared(
        (program->record_words != 0ULL) ? program->record_words : 1ULL, sizeof(unsigned int),
        platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    char folder[PLATFORM_PATH_BYTES];
    char records_path[PLATFORM_PATH_BYTES];
    char ghdl_log[PLATFORM_PATH_BYTES];
    (void)vhdl_job_folder(place, index, folder, sizeof(folder));
    (void)vhdl_path(folder, "records.txt", records_path, sizeof(records_path));
    (void)vhdl_path(folder, "ghdl.log", ghdl_log, sizeof(ghdl_log));
    RecordImage shape;
    mmgr_bytes_set(&shape, 0, sizeof(shape));
    shape.record_words = program->record_words;
    trial->read = (trial->records != NULL) && (job->status == 0L) &&
                  record_image_read(records_path, &shape, &trial->error, trial->records, &trial->clocks);
    if (trial->read == 0)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  GHDL did not run the lane (status ");
            mmgr_line_decimal_signed(output_line, job->status);
            mmgr_line_put(output_line, "); its log begins:");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        vhdl_log_shown(ghdl_log);
    }
    if ((place->synthesis != 0) && (trial->read != 0))
    {
        char yosys_log[PLATFORM_PATH_BYTES];
        char memories_log[PLATFORM_PATH_BYTES];
        (void)vhdl_path(folder, "yosys.log", yosys_log, sizeof(yosys_log));
        (void)vhdl_path(folder, "memories.log", memories_log, sizeof(memories_log));
        trial->memories = vhdl_after(memories_log, "$mem_v2");
        trial->registered = vhdl_lines(yosys_log, "merging output FF");
        trial->cells = vhdl_after(yosys_log, "Number of cells:");
        trial->path = vhdl_after(yosys_log, "(length=");
        trial->synthesized = (job->synthesis_status == 0L) && (trial->cells != 0ULL);
    }
    trial->cost = trial->clocks * (trial->synthesized ? trial->path : 1ULL);
}

// The lines and checks of one schedule. Its run goes against the host's, or must error where the host errors on the
// run. Where the test synthesized it, the lines also give what synthesis gave. Returns 1 where its run is the host's
static int vhdl_report(VhdlResults *results, const VhdlPlace *place, const char *what, const VhdlTrial *trial,
                       const VhdlProgram *program)
{
    const int same =
        (trial->records != NULL) && (mmgr_bytes_compare(trial->records, program->host_records,
                                                        (size_t)(program->record_words * sizeof(unsigned int))) == 0);
    const int passed = program->errors ? (trial->read && !program->host_ran && (trial->error != 0u))
                                       : (trial->read && program->host_ran && (trial->error == 0u) && same);
    vhdl_named_check(results, passed, what,
                     program->errors ? " as VHDL errors on a lane where the host errors on the run"
                                     : " as VHDL writes the host's records word for word");
    if (place->synthesis && trial->read)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, what);
            mmgr_line_put(output_line, ", synthesized over ");
            mmgr_line_decimal(output_line, VHDL_TEST_SYNTHESIS_WORDS, 1u);
            mmgr_line_put(output_line, " words: ");
            mmgr_line_decimal(output_line, trial->memories, 1u);
            mmgr_line_put(output_line, " memories inferred, ");
            mmgr_line_decimal(output_line, trial->registered, 1u);
            mmgr_line_put(output_line, " read ports registered, ");
            mmgr_line_decimal(output_line, trial->cells, 1u);
            mmgr_line_put(output_line, " cells, the longest path ");
            mmgr_line_decimal(output_line, trial->path, 1u);
            mmgr_line_put(output_line, " cells; ");
            mmgr_line_decimal(output_line, trial->clocks, 1u);
            mmgr_line_put(output_line, " clocks, cost ");
            mmgr_line_decimal(output_line, trial->cost, 1u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        vhdl_named_check(results, trial->synthesized, what, " is synthesized by GHDL and Yosys");
    }
    return passed;
}

// the line of an end of the lane: its steps, out limbs, lines of VHDL and states. Then the digests of its inputs and
// of its records as VHDL and on the host
static void vhdl_end_shown(const char *what, const VhdlProgram *program, const VhdlJob *job, const VhdlTrial *trial)
{
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  ");
    mmgr_line_put(output_line, what);
    mmgr_line_put(output_line, ": ");
    mmgr_line_decimal(output_line, program->steps, 1u);
    mmgr_line_put(output_line, " steps, ");
    mmgr_line_decimal(output_line, program->out_limbs, 1u);
    mmgr_line_put(output_line, " out limbs, ");
    mmgr_line_decimal(output_line, job->lines, 1u);
    mmgr_line_put(output_line, " lines of VHDL, ");
    mmgr_line_decimal(output_line, job->report.states, 1u);
    mmgr_line_put(output_line, " states, inputs ");
    mmgr_line_hex(output_line, program->inputs, 16u);
    mmgr_line_put(output_line, ", records ");
    mmgr_line_hex(output_line, trial->read ? host_digest(trial->records, program->record_words) : 0ULL, 16u);
    mmgr_line_put(output_line, " as VHDL, ");
    mmgr_line_hex(output_line, program->host_digest, 16u);
    mmgr_line_put(output_line, " on the host, ");
    mmgr_line_decimal(output_line, trial->error, 1u);
    mmgr_line_put(output_line, " lanes errored as VHDL");
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
}

// the line of a round of the refinement loop: its states, the max cost one chains, and the forms alone past the
// budget. Then the digest of its records as VHDL
static void vhdl_round_shown(const char *what, const VhdlProgram *program, const VhdlJob *job, const VhdlTrial *trial)
{
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  ");
    mmgr_line_put(output_line, what);
    mmgr_line_put(output_line, ": ");
    mmgr_line_decimal(output_line, job->report.states, 1u);
    mmgr_line_put(output_line, " states (the max cost one chains ");
    mmgr_line_decimal(output_line, job->report.max, 1u);
    mmgr_line_put(output_line, ", ");
    mmgr_line_decimal(output_line, job->report.over, 1u);
    mmgr_line_put(output_line, " forms alone past the budget), records ");
    mmgr_line_hex(output_line, trial->read ? host_digest(trial->records, program->record_words) : 0ULL, 16u);
    mmgr_line_put(output_line, " as VHDL");
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
}

// Each job of one program read and checked, in the order the first stage wrote them. Where the caller gave a
// construction set, the line of the refinement loop: the least cost among the rounds whose lanes are the host's
static void vhdl_program_checked(VhdlResults *results, const VhdlPlace *place, const VhdlProgram *program)
{
    static const char *const s_end_names[2] = {"split where it must be", "every form alone"};
    vhdl_named_check(results, program->room, program->name, " has room in the test's table of jobs");
    unsigned long long end_costs[2] = {0ULL, 0ULL};
    unsigned long long best_cost = 0ULL;
    unsigned int best_budget = 0u;
    unsigned int rounds = 0u;
    for (unsigned int index = program->first_job; index < (program->first_job + program->jobs); index += 1u)
    {
        const VhdlJob *const job = &s_vhdl_table.job[index];
        char what[VHDL_TEST_WHAT_BYTES];
        VerbaLinea what_line = mmgr_line_over(what, sizeof(what));
        mmgr_line_put(&what_line, program->name);
        if (job->type == VHDL_JOB_ROUND)
        {
            mmgr_line_put(&what_line, ", round ");
            mmgr_line_decimal(&what_line, job->round, 1u);
            mmgr_line_put(&what_line, " at budget ");
            mmgr_line_decimal(&what_line, job->budget, 1u);
        }
        else
        {
            mmgr_line_put(&what_line, ", ");
            mmgr_line_put(&what_line, s_end_names[job->type]);
        }
        (void)mmgr_line_finish(&what_line);
        if (job->type == VHDL_JOB_SPLIT)
        {
            vhdl_named_check(results, program->split_same, program->name,
                             ": program() is the lane split where it must be");
        }
        VhdlTrial trial;
        vhdl_job_read(place, index, program, &trial);
        if (job->type == VHDL_JOB_ROUND)
        {
            vhdl_round_shown(what, program, job, &trial);
        }
        else
        {
            vhdl_end_shown(what, program, job, &trial);
        }
        const int passed = vhdl_report(results, place, what, &trial, program);
        if (job->type != VHDL_JOB_ROUND)
        {
            end_costs[job->type] = passed ? trial.cost : 0ULL;
        }
        else
        {
            rounds += 1u;
            if (passed && ((best_budget == 0u) || (trial.cost < best_cost)))
            {
                best_cost = trial.cost;
                best_budget = job->budget;
            }
        }
        platform_cell_released(trial.records, platform_place_g_program_owner);
    }
    if (place->constructed != 0)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, program->name);
        mmgr_line_put(output_line, ": the refinement loop retained budget ");
        mmgr_line_decimal(output_line, best_budget, 1u);
        mmgr_line_put(output_line, " at cost ");
        mmgr_line_decimal(output_line, best_cost, 1u);
        mmgr_line_put(output_line, " over ");
        mmgr_line_decimal(output_line, rounds, 1u);
        mmgr_line_put(output_line, " rounds; split where it must be costs ");
        mmgr_line_decimal(output_line, end_costs[0], 1u);
        mmgr_line_put(output_line, ", every form alone ");
        mmgr_line_decimal(output_line, end_costs[1], 1u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
}

int program_main(int argc, char **argv)
{
    // each line reaches the log as the test writes it, where stdout is a file
    platform_line_out_every_write();
    VhdlPlace place;
    mmgr_bytes_set(&place, 0, sizeof(place));
    int usable = argc >= 3;
    for (int at = 3; usable && (at < argc); at += 1)
    {
        const int construction =
            (mmgr_text_equal(argv[at], "--construction", PLATFORM_TEXT_TO_TERMINATOR)) && ((at + 1) < argc);
        place.synthesis = place.synthesis || (mmgr_text_equal(argv[at], "--synthesis", PLATFORM_TEXT_TO_TERMINATOR));
        usable = construction ? vhdl_construction(argv[at + 1], &place)
                              : (mmgr_text_equal(argv[at], "--synthesis", PLATFORM_TEXT_TO_TERMINATOR));
        at += construction ? 1 : 0;
    }
    if (!usable)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line,
                          "usage: record_vhdl_test <work folder> <record_vhdl_bench.vhd> [--synthesis] [--construction "
                          "<vhdl.kcs>], the construction set read whole");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    place.work = argv[1];
    place.bench = argv[2];
    // the Yosys script, the same for every job, which each job's folder receives
    PlatformText script;
    platform_text_begun(&script, VHDL_TEST_TEXT_BYTES, platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
    if (place.synthesis != 0)
    {
        place.synthesis = yosys_script().synthesis("synth.v", "cycle_program_unit", "memories.log", &script);
        place.script = platform_text_bytes(&script);
        place.script_length = platform_text_size(&script);
    }
    VhdlResults results = {0u, 0u, 0u};
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  record vhdl test: ");
        mmgr_line_decimal(output_line, VHDL_TEST_LANES, 1u);
        mmgr_line_put(output_line, " lanes a program, ORIOR_EXACT_LIMBS ");
        mmgr_line_decimal(output_line, (unsigned int)ORIOR_EXACT_LIMBS, 1u);
        mmgr_line_put(output_line, ", ");
        if (place.constructed)
        {
            mmgr_line_put(output_line, "a construction set of ");
            mmgr_line_decimal(output_line, place.costs.count, 1u);
            mmgr_line_put(output_line, " forms");
        }
        else
        {
            mmgr_line_put(output_line, "no construction set");
        }
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    vhdl_check(&results, platform_folder_made(place.work), "the work folder is made");
    VhdlRun run = {&results, &place};
    record_image_programs(&run, vhdl_run);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  record vhdl test: ");
        mmgr_line_decimal(output_line, s_vhdl_table.jobs, 1u);
        mmgr_line_put(output_line, " jobs of ");
        mmgr_line_decimal(output_line, s_vhdl_table.programs, 1u);
        mmgr_line_put(output_line, " programs on ");
        mmgr_line_decimal(output_line, platform_processors(), 1u);
        mmgr_line_put(output_line, " processors");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    vhdl_jobs_run(&place);
    for (unsigned int program = 0u; program < s_vhdl_table.programs; program += 1u)
    {
        vhdl_program_checked(&results, &place, &s_vhdl_table.program[program]);
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  record vhdl test: ");
        mmgr_line_decimal(output_line, results.checks, 1u);
        mmgr_line_put(output_line, " checks, ");
        mmgr_line_decimal(output_line, results.failed, 1u);
        mmgr_line_put(output_line, " failed, ");
        mmgr_line_decimal(output_line, results.unsupported, 1u);
        mmgr_line_put(output_line, " programs not supported in VHDL");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    // the host's records go back in the reverse order of their takes
    for (unsigned int program = s_vhdl_table.programs; program > 0u; program -= 1u)
    {
        platform_cell_released(s_vhdl_table.program[program - 1u].host_records, platform_place_g_program_owner);
    }
    platform_text_ended(&script);
    platform_array_ended(&place.costs);
    platform_cell_released(place.names, platform_place_g_program_owner);
    return (results.failed == 0u) ? 0 : 1;
}
