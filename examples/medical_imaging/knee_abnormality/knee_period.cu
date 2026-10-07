// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// The knee's period pass over sealed sets:
//   - series are grouped by exact extent, and each group keeps its own null top per axis;
//   - a group of N series has rank * N axes, and its draws D are the least with D + 1 > rank * N: D = rank * N;
//   - draw k uses series k mod N of its group, in set order, seeded by that series' root and k (period_draw);
//   - each axis's top is the strongest null height over the group's D draws, and period_read reads every series
//     against its group's tops with no draws of its own.
// Every load, every draw and every read is one job on the device's tessera daemon. The draws and readings are written
// as they finish. A stopped pass resumes from its files.
//
//   knee_period --daemon <tessera_daemon> --out <dir> <set> [<set> ...]
//   knee_period ingest --daemon <tessera_daemon> --source <zip> --set <dir> --list <file>   (knee_ingest below)
#include "engine.h"
#include "obsignatio.h"
#include "period.h"
#include "tessera.h"
#include "compression.h"
#include "device_pool.h"
#include "tower.h"

#include <cuda_runtime.h>

#include <chrono>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define KNEE_RANK 3u

#define KNEE_HOLDING_MICROSECONDS 2000000ull
#define KNEE_SWEEP_MICROSECONDS 20000ull
#define KNEE_IDLE_MICROSECONDS 5000000ull

typedef struct
{
    const char *set;
    char *name;
    unsigned long long extent[4];
    unsigned int group;
    unsigned long long place;
} KneeSeries;

typedef struct
{
    unsigned long long extent[4];
    unsigned long long count;
    unsigned long long draws;
    PeriodMargin top[KNEE_RANK];
    unsigned long long drawn;
} KneeGroup;

typedef struct
{
    TesseraClient *client;
    TesseraTicket ticket;
    unsigned long long whole;
} KneeJob;

typedef struct
{
    char *bytes;
    size_t used;
    size_t room;
} KneeText;

static const char *s_daemon = NULL;

static unsigned long long knee_now_microseconds(void)
{
    // a steady clock's count is non-negative and re-signs exactly
    return (unsigned long long)std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

static int knee_text_add(KneeText *text, const char *format, ...)
{
    va_list measure;
    va_start(measure, format);
    const int wanted = vsnprintf(NULL, 0u, format, measure);
    va_end(measure);
    if (wanted < 0)
    {
        return 0;
    }
    // a non-negative length re-signs exactly
    const size_t needed = text->used + (size_t)wanted + 1u;
    if (needed > text->room)
    {
        size_t room = (text->room != 0u) ? text->room : 64u;
        while (room < needed)
        {
            room *= 2u;
        }
        char *const grown = (char *)realloc(text->bytes, room);
        if (grown == NULL)
        {
            return 0;
        }
        text->bytes = grown;
        text->room = room;
    }
    va_list write;
    va_start(write, format);
    vsnprintf(text->bytes + text->used, text->room - text->used, format, write);
    va_end(write);
    text->used += (size_t)wanted;
    return 1;
}

static char *knee_line(FILE *file)
{
    // a line of any length: the room doubles until the line ends
    size_t room = 64u;
    size_t at = 0u;
    char *line = (char *)malloc(room);
    int read = 0;
    while ((line != NULL) && ((read = fgetc(file)) != EOF) && (read != '\n'))
    {
        if ((at + 1u) >= room)
        {
            char *const grown = (char *)realloc(line, room * 2u);
            if (grown == NULL)
            {
                free(line);
                return NULL;
            }
            line = grown;
            room *= 2u;
        }
        line[at] = (char)read;
        at += 1u;
    }
    if ((line == NULL) || ((read == EOF) && (at == 0u)))
    {
        free(line);
        return NULL;
    }
    line[at] = '\0';
    if ((at != 0u) && (line[at - 1u] == '\r'))
    {
        line[at - 1u] = '\0';
    }
    return line;
}

static int knee_job_submit(const char *kind, const unsigned long long *shape, unsigned long long declared, KneeJob *job)
{
    // the signum is the job's kind and the lattice's shape: jobs alike in what they hold share a kept peak
    memset(job, 0, sizeof(*job));
    EngineError error;
    memset(&error, 0, sizeof(error));
    int device = 0;
    cudaDeviceProp properties;
    if ((cudaGetDevice(&device) != cudaSuccess) || (cudaGetDeviceProperties(&properties, device) != cudaSuccess))
    {
        fprintf(stderr, "  tessera: %s: the device did not answer who it is\n", kind);
        return 0;
    }
    TesseraJobAsk ask;
    memset(&ask, 0, sizeof(ask));
    memcpy(ask.device, properties.uuid.bytes, TESSERA_DEVICE_BYTES);
#if defined(_WIN32)
    memcpy(&ask.luid, properties.luid, sizeof(ask.luid));
#endif
    const size_t named = strlen(kind) + 1u;
    const size_t request_bytes = named + (KNEE_RANK * sizeof(unsigned long long));
    unsigned char *const request = (unsigned char *)malloc(request_bytes);
    if (request == NULL)
    {
        return 0;
    }
    memcpy(request, kind, named);
    memcpy(request + named, shape, KNEE_RANK * sizeof(unsigned long long));
    const ObsignatioSignumRequest signum = {request, request_bytes, NULL, OBSIGNATIO_MODE_HASH, ask.signum.bytes,
                                            ENGINE_SIGNUM_BYTES, &error};
    const long hashed = obsignatio_signum(&signum);
    free(request);
    if ((hashed != 0L) || (declared == 0ull))
    {
        fprintf(stderr, "  tessera: %s: no job could be asked\n", kind);
        return 0;
    }
    ask.declared = declared;
    ask.holding_microseconds = KNEE_HOLDING_MICROSECONDS;
    ask.sweep_microseconds = KNEE_SWEEP_MICROSECONDS;
    ask.idle_microseconds = KNEE_IDLE_MICROSECONDS;
    ask.daemon_path = s_daemon;
    ask.error = &error;
    if (tessera_job_submit(&ask, &job->client, &job->ticket) != 0L)
    {
        fprintf(stderr, "  tessera: %s: the daemon (%s) did not take the job\n", kind, s_daemon);
        return 0;
    }
    if (job->ticket.asked != 0u)
    {
        const long answered = tessera_job_wait(job->client, &job->ticket, &error);
        if ((answered != 0L) || (job->ticket.lost != 0u))
        {
            fprintf(stderr, "  tessera: %s was held past its holding time and lost (ticket in %s)\n", kind,
                    job->ticket.lost_path);
            if (answered == 0L)
            {
                tessera_job_precalc_kept(job->client, &error);
            }
            job->client = NULL;
            return 0;
        }
    }
    job->whole = declared + job->ticket.standing;
    return 1;
}

static int knee_job_release(const char *kind, KneeJob *job)
{
    EngineError error;
    memset(&error, 0, sizeof(error));
    if ((job->client == NULL) || (tessera_job_release(job->client, &job->ticket, &error) != 0L))
    {
        fprintf(stderr, "  tessera: %s did not release\n", kind);
        return 0;
    }
    job->client = NULL;
    if (job->ticket.last_peak > job->whole)
    {
        printf("  tessera: %s peaked at %llu bytes, over the %llu it held and declared\n", kind, job->ticket.last_peak,
               job->whole);
    }
    return 1;
}

static void knee_shape(const KneeSeries *series, unsigned long long shape[KNEE_RANK])
{
    for (unsigned int axis = 0u; axis < KNEE_RANK; axis += 1u)
    {
        shape[axis] = series->extent[axis + 1u];
    }
}

// a series' crystal, lowered and proved against its seal, as one job holding the tower's and the coder's pools
static unsigned short *knee_load(const KneeSeries *series, EngineSignum *root)
{
    unsigned long long shape[KNEE_RANK];
    knee_shape(series, shape);
    const unsigned long long lanes = shape[0] * shape[1] * shape[2];
    KneeJob job;
    if (!knee_job_submit("knee crystal load", shape, tower_reserve_bytes(lanes) + compression_reserve_bytes(lanes),
                         &job))
    {
        return NULL;
    }
    EngineError error;
    memset(&error, 0, sizeof(error));
    unsigned long long extent[4] = {0ull, 0ull, 0ull, 0ull};
    unsigned short *volume = NULL;
    const long loaded = engine_iapx_load(series->set, series->name, extent, &volume, root, NULL, &error);
    const int released = knee_job_release("knee crystal load", &job);
    if ((loaded != 0L) || !released || (memcmp(extent, series->extent, sizeof(extent)) != 0))
    {
        fprintf(stderr, "  %s: the crystal did not load (kind %u, module %u, site %u)\n", series->name, error.kind,
                error.module, error.site);
        free(volume);
        return NULL;
    }
    return volume;
}

static unsigned long long knee_allocation_bytes(unsigned long long bytes)
{
    DevicePoolPlan plan = {0ull, 0ull, 0};
    device_pool_plan_slice(&plan, bytes);
    return device_pool_plan_bytes(&plan);
}

// the device bytes of one period call on this shape: the lanes, in their own allocation and so paged alone, and the
// period's pool for that many voxels and lags
static unsigned long long knee_period_bytes(const unsigned long long shape[KNEE_RANK])
{
    const unsigned long long voxels = shape[0] * shape[1] * shape[2];
    return knee_allocation_bytes(voxels * sizeof(unsigned short)) +
           period_reserve_bytes(voxels, period_agreement_entries(KNEE_RANK, shape));
}

typedef struct
{
    PeriodMargin heights[KNEE_RANK];
    PeriodMeasurement reading;
    unsigned long long microseconds;
} KneeCall;

// one period call as one job: the lanes go up, the call runs, the lanes are freed
static int knee_period_call(const KneeSeries *series, const unsigned short *volume, const EngineSignum *root,
                            const char *kind, unsigned long long draw, const PeriodMargin *null_top, KneeCall *call)
{
    unsigned long long shape[KNEE_RANK];
    knee_shape(series, shape);
    const unsigned long long voxels = shape[0] * shape[1] * shape[2];
    KneeJob job;
    if (!knee_job_submit(kind, shape, knee_period_bytes(shape), &job))
    {
        return 0;
    }
    EngineError error;
    memset(&error, 0, sizeof(error));
    unsigned short *device_lanes = NULL;
    int good = (cudaMalloc((void **)&device_lanes, (size_t)voxels * sizeof(unsigned short)) == cudaSuccess)
            && (cudaMemcpy(device_lanes, volume, (size_t)voxels * sizeof(unsigned short), cudaMemcpyHostToDevice)
                == cudaSuccess);
    PeriodRequest request;
    memset(&request, 0, sizeof(request));
    request.device_lanes = device_lanes;
    request.rank = KNEE_RANK;
    memcpy(request.extent, shape, sizeof(shape));
    request.content = *root;
    request.null_top = null_top;
    request.measurement = &call->reading;
    request.error = &error;
    const unsigned long long started = knee_now_microseconds();
    if (good)
    {
        good = (null_top == NULL) ? (period_draw(&request, draw, call->heights) == 0L) : (period_read(&request) == 0L);
    }
    call->microseconds = knee_now_microseconds() - started;
    cudaFree(device_lanes);
    const int released = knee_job_release(kind, &job);
    if (!good)
    {
        fprintf(stderr, "  %s: %s refused (kind %u, module %u, site %u)\n", series->name, kind, error.kind,
                error.module, error.site);
    }
    return good && released;
}

static int knee_header(KneeText *text, unsigned long long series, const KneeGroup *groups, unsigned int group_count)
{
    int good = knee_text_add(text, "# series %llu groups %u rank %u\n", series, group_count, KNEE_RANK);
    for (unsigned int group = 0u; good && (group < group_count); group += 1u)
    {
        good = knee_text_add(text, "# group %u extent %llu %llu %llu %llu series %llu draws %llu\n", group,
                             groups[group].extent[0], groups[group].extent[1], groups[group].extent[2],
                             groups[group].extent[3], groups[group].count, groups[group].draws);
    }
    return good;
}

// a written file is kept only when its header is this run's, line for line; otherwise its lines were made under other
// groups, and the run refuses and mixes none of them. Each body line is handed to `take`.
typedef int (*KneeLineTake)(const char *line, void *context);

static int knee_resume(const char *path, const KneeText *header, KneeLineTake take, void *context, int *existed)
{
    FILE *const old = fopen(path, "rb");
    *existed = (old != NULL);
    if (old == NULL)
    {
        return 1;
    }
    KneeText seen = {NULL, 0u, 0u};
    int good = 1;
    char *line = NULL;
    while (good && ((line = knee_line(old)) != NULL))
    {
        good = (line[0] == '#') ? knee_text_add(&seen, "%s\n", line) : take(line, context);
        free(line);
    }
    fclose(old);
    if (good && ((seen.used != header->used) || (memcmp(seen.bytes, header->bytes, header->used) != 0)))
    {
        fprintf(stderr, "  %s was written under other groups or tops; move it aside to start again\n", path);
        good = 0;
    }
    free(seen.bytes);
    return good;
}

static int knee_name_order(const void *one, const void *other)
{
    const KneeSeries *const *const left = (const KneeSeries *const *)one;
    const KneeSeries *const *const right = (const KneeSeries *const *)other;
    return strcmp((*left)->name, (*right)->name);
}

typedef struct
{
    KneeSeries *series;
    KneeSeries **by_name;
    unsigned long long count;
    KneeGroup *groups;
    unsigned char *done;
} KneeRun;

static KneeSeries *knee_find(const KneeRun *run, const char *name)
{
    KneeSeries key;
    key.name = (char *)name;
    KneeSeries *const wanted = &key;
    KneeSeries **const found = (KneeSeries **)bsearch(&wanted, run->by_name, (size_t)run->count, sizeof(KneeSeries *),
                                                      knee_name_order);
    return (found != NULL) ? *found : NULL;
}

static int knee_parse_margin(const char *text, PeriodMargin *margin, const char **after)
{
    char *end = NULL;
    margin->numerator = strtoull(text, &end, 10);
    if ((end == text) || (*end != '/'))
    {
        return 0;
    }
    const char *const denominator = end + 1;
    margin->denominator = strtoull(denominator, &end, 10);
    *after = end;
    return end != denominator;
}

// the tops keep the strongest height per axis; one group's heights on an axis share their denominator, the pairs at
// every lag, and the numerators compare exactly
static int knee_fold(KneeGroup *group, const PeriodMargin heights[KNEE_RANK])
{
    for (unsigned int axis = 0u; axis < KNEE_RANK; axis += 1u)
    {
        if ((group->drawn != 0ull) && (group->top[axis].denominator != heights[axis].denominator))
        {
            fprintf(stderr, "  a group's heights on axis %u differ in their pairs (%llu against %llu)\n", axis,
                    group->top[axis].denominator, heights[axis].denominator);
            return 0;
        }
        group->top[axis].denominator = heights[axis].denominator;
        const unsigned long long held = group->top[axis].numerator;
        group->top[axis].numerator = (heights[axis].numerator > held) ? heights[axis].numerator : held;
    }
    group->drawn += 1ull;
    return 1;
}

// a draw line: series, k, then each axis's height as numerator/denominator, then the call's microseconds
static int knee_take_draw(const char *line, void *context)
{
    KneeRun *const run = (KneeRun *)context;
    const char *const tab = strchr(line, '\t');
    if (tab == NULL)
    {
        return 0;
    }
    char *const name = (char *)malloc((size_t)(tab - line) + 1u);
    if (name == NULL)
    {
        return 0;
    }
    memcpy(name, line, (size_t)(tab - line));
    name[tab - line] = '\0';
    KneeSeries *const series = knee_find(run, name);
    free(name);
    char *end = NULL;
    const unsigned long long draw = strtoull(tab + 1, &end, 10);
    PeriodMargin heights[KNEE_RANK];
    const char *at = end;
    int good = (series != NULL) && (end != (tab + 1));
    for (unsigned int axis = 0u; good && (axis < KNEE_RANK); axis += 1u)
    {
        good = (*at == '\t') && knee_parse_margin(at + 1, &heights[axis], &at);
    }
    KneeGroup *const group = good ? &run->groups[series->group] : NULL;
    good = good && (draw < group->draws) && ((draw % group->count) == series->place);
    const unsigned long long turn = good ? (draw / group->count) : 0ull;
    const unsigned long long index = good ? ((unsigned long long)(series - run->series) * KNEE_RANK) + turn : 0ull;
    good = good && (run->done[index] == 0u);
    if (!good)
    {
        fprintf(stderr, "  a draw line does not fit this run's groups: %s\n", line);
        return 0;
    }
    run->done[index] = 1u;
    return knee_fold(group, heights);
}

// a reading line starts with its series; the rest is kept as written
static int knee_take_reading(const char *line, void *context)
{
    KneeRun *const run = (KneeRun *)context;
    const char *const tab = strchr(line, '\t');
    if (tab == NULL)
    {
        return 0;
    }
    char *const name = (char *)malloc((size_t)(tab - line) + 1u);
    if (name == NULL)
    {
        return 0;
    }
    memcpy(name, line, (size_t)(tab - line));
    name[tab - line] = '\0';
    KneeSeries *const series = knee_find(run, name);
    free(name);
    if ((series == NULL) || (run->done[series - run->series] != 0u))
    {
        fprintf(stderr, "  a reading line does not fit this run's series: %s\n", line);
        return 0;
    }
    run->done[series - run->series] = 1u;
    return 1;
}

static int knee_path(char **out, const char *directory, const char *leaf)
{
    KneeText text = {NULL, 0u, 0u};
    const int good = knee_text_add(&text, "%s/%s", directory, leaf);
    *out = text.bytes;
    return good;
}

// the ingest: every series named in the list, one a line, read from the source and sealed into the set as a crystal,
// one job each. A series whose crystal's head already reads is kept as it is, and a stopped ingest resumes from the
// set. A series that does not ingest is reported and the next one runs; the exit is 1 where any did not
//
//   knee_period ingest --daemon <tessera_daemon> --source <zip> --set <dir> --list <file>
static int knee_ingest(int argc, char **argv)
{
    const char *source = NULL;
    const char *set = NULL;
    const char *list = NULL;
    for (int at = 1; (at + 1) < argc; at += 2)
    {
        const char *const value = argv[at + 1];
        s_daemon = (strcmp(argv[at], "--daemon") == 0) ? value : s_daemon;
        source = (strcmp(argv[at], "--source") == 0) ? value : source;
        set = (strcmp(argv[at], "--set") == 0) ? value : set;
        list = (strcmp(argv[at], "--list") == 0) ? value : list;
    }
    FILE *const named = (list != NULL) ? fopen(list, "rb") : NULL;
    if ((s_daemon == NULL) || (source == NULL) || (set == NULL) || (named == NULL))
    {
        fprintf(stderr,
                "usage: knee_period ingest --daemon <tessera_daemon> --source <zip> --set <dir> --list <file>\n");
        if (named != NULL)
        {
            fclose(named);
        }
        return 2;
    }
    unsigned long long count = 0ull;
    unsigned long long kept = 0ull;
    unsigned long long sealed = 0ull;
    unsigned long long failed = 0ull;
    char *sample = NULL;
    while ((sample = knee_line(named)) != NULL)
    {
        if (sample[0] == '\0')
        {
            free(sample);
            continue;
        }
        count += 1ull;
        EngineError error;
        memset(&error, 0, sizeof(error));
        unsigned long long extent[4] = {0ull, 0ull, 0ull, 0ull};
        if (engine_iapx_head(set, sample, extent, &error) == 0L)
        {
            kept += 1ull;
            free(sample);
            continue;
        }
        memset(&error, 0, sizeof(error));
        unsigned long long lanes = 0ull;
        int good = engine_source_lanes(source, sample, &lanes, &error) == 0L;
        const unsigned long long shape[KNEE_RANK] = {lanes, 0ull, 0ull};
        KneeJob job;
        good = good && knee_job_submit("knee ingest", shape,
                                       knee_allocation_bytes(lanes * sizeof(unsigned short)) +
                                           tower_reserve_bytes(lanes) + compression_reserve_bytes(lanes),
                                       &job);
        const unsigned long long started = knee_now_microseconds();
        if (good)
        {
            memset(&error, 0, sizeof(error));
            EngineSampleRecord record;
            memset(&record, 0, sizeof(record));
            EngineSetReport report;
            memset(&report, 0, sizeof(report));
            report.samples = &record;
            char *const samples[1] = {sample};
            const EngineIngestRequest ingest = {source, set, samples, 1u, NULL, 0u, &error, &report};
            good = (engine_ingest_set(&ingest) == 0L) && (report.sealed == 1ull);
            good = knee_job_release("knee ingest", &job) && good;
        }
        const unsigned long long spent = knee_now_microseconds() - started;
        if (good)
        {
            sealed += 1ull;
            printf("  ingest %llu  %s  %llu lanes  %llu us\n", count, sample, lanes, spent);
        }
        else
        {
            failed += 1ull;
            fprintf(stderr, "  %s did not ingest (kind %u, module %u, site %u)\n", sample, error.kind, error.module,
                    error.site);
            printf("  ingest %llu  %s  did not ingest\n", count, sample);
        }
        free(sample);
    }
    fclose(named);
    printf("  the ingest is done: %llu series named, %llu kept, %llu sealed, %llu did not ingest\n", count, kept,
           sealed, failed);
    return (failed == 0ull) ? 0 : 1;
}

int main(int argc, char **argv)
{
    if ((argc > 1) && (strcmp(argv[1], "ingest") == 0))
    {
#if defined(_WIN32)
        // the card is shared: the ingest runs below every normal process
        SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
#endif
        setvbuf(stdout, NULL, _IONBF, 0u);
        int device = 0;
        if ((cudaGetDevice(&device) != cudaSuccess) || (cudaSetDevice(device) != cudaSuccess)
            || (cudaFree(0) != cudaSuccess))
        {
            fprintf(stderr, "  the device's context was not made\n");
            return 1;
        }
        return knee_ingest(argc - 1, argv + 1);
    }
    const char *out = NULL;
    int first_set = 1;
    while ((first_set + 1) < argc)
    {
        if (strcmp(argv[first_set], "--daemon") == 0)
        {
            s_daemon = argv[first_set + 1];
        }
        else if (strcmp(argv[first_set], "--out") == 0)
        {
            out = argv[first_set + 1];
        }
        else
        {
            break;
        }
        first_set += 2;
    }
    if ((s_daemon == NULL) || (out == NULL) || (first_set >= argc))
    {
        fprintf(stderr, "usage: knee_period --daemon <tessera_daemon> --out <dir> <set> [<set> ...]\n");
        return 2;
    }
#if defined(_WIN32)
    // the card is shared: this pass runs below every normal process
    SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
#endif
    setvbuf(stdout, NULL, _IONBF, 0u);

    // the device's context is made before the first job is asked. Its bytes stand in what submit measures, and no
    // job's peak carries them
    int device = 0;
    if ((cudaGetDevice(&device) != cudaSuccess) || (cudaSetDevice(device) != cudaSuccess)
        || (cudaFree(0) != cudaSuccess))
    {
        fprintf(stderr, "  the device's context was not made\n");
        return 1;
    }

    // every series of every set, in the sets' order and each set's own sorted order
    KneeRun run;
    memset(&run, 0, sizeof(run));
    for (int set = first_set; set < argc; set += 1)
    {
        char **names = NULL;
        const unsigned int count = engine_set_samples(argv[set], &names);
        KneeSeries *const grown = (KneeSeries *)realloc(run.series, (size_t)(run.count + count) * sizeof(KneeSeries));
        if (grown == NULL)
        {
            return 1;
        }
        run.series = grown;
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            KneeSeries *const series = &run.series[run.count];
            memset(series, 0, sizeof(*series));
            series->set = argv[set];
            series->name = names[at];
            EngineError error;
            memset(&error, 0, sizeof(error));
            if (engine_iapx_head(argv[set], names[at], series->extent, &error) != 0L)
            {
                fprintf(stderr, "  %s/%s: the head did not read\n", argv[set], names[at]);
                return 1;
            }
            if (series->extent[0] != 1ull)
            {
                fprintf(stderr, "  %s/%s has %llu frames; the knee reads 3 spatial axes on one frame\n", argv[set],
                        names[at], series->extent[0]);
                return 1;
            }
            run.count += 1ull;
        }
        printf("  %s: %u series\n", argv[set], count);
    }
    run.by_name = (KneeSeries **)malloc((size_t)run.count * sizeof(KneeSeries *));
    run.groups = (KneeGroup *)calloc((size_t)run.count, sizeof(KneeGroup));
    run.done = (unsigned char *)calloc((size_t)run.count * KNEE_RANK, 1u);
    if ((run.count == 0ull) || (run.by_name == NULL) || (run.groups == NULL) || (run.done == NULL))
    {
        fprintf(stderr, "  no series, or no room to hold them\n");
        return 1;
    }
    for (unsigned long long at = 0ull; at < run.count; at += 1ull)
    {
        run.by_name[at] = &run.series[at];
    }
    qsort(run.by_name, (size_t)run.count, sizeof(KneeSeries *), knee_name_order);
    for (unsigned long long at = 1ull; at < run.count; at += 1ull)
    {
        if (strcmp(run.by_name[at - 1ull]->name, run.by_name[at]->name) == 0)
        {
            fprintf(stderr, "  %s is named in two sets\n", run.by_name[at]->name);
            return 1;
        }
    }

    // the groups, in the order their first series appears
    unsigned int group_count = 0u;
    for (unsigned long long at = 0ull; at < run.count; at += 1ull)
    {
        KneeSeries *const series = &run.series[at];
        unsigned int group = 0u;
        while ((group < group_count) && (memcmp(run.groups[group].extent, series->extent, sizeof(series->extent)) != 0))
        {
            group += 1u;
        }
        if (group == group_count)
        {
            memcpy(run.groups[group].extent, series->extent, sizeof(series->extent));
            group_count += 1u;
        }
        series->group = group;
        series->place = run.groups[group].count;
        run.groups[group].count += 1ull;
    }
    unsigned long long all_draws = 0ull;
    for (unsigned int group = 0u; group < group_count; group += 1u)
    {
        run.groups[group].draws = KNEE_RANK * run.groups[group].count;
        all_draws += run.groups[group].draws;
    }
    KneeText header = {NULL, 0u, 0u};
    if (!knee_header(&header, run.count, run.groups, group_count))
    {
        return 1;
    }
    printf("%s", header.bytes);

    // the draws: series-major, each crystal loaded once for its rank turns k = place + turn * N
    char *draws_path = NULL;
    int draws_existed = 0;
    if (!knee_path(&draws_path, out, "draws.tsv")
        || !knee_resume(draws_path, &header, knee_take_draw, &run, &draws_existed))
    {
        return 1;
    }
    unsigned long long drawn = 0ull;
    for (unsigned int group = 0u; group < group_count; group += 1u)
    {
        drawn += run.groups[group].drawn;
    }
    printf("  draws: %llu of %llu already written\n", drawn, all_draws);
    FILE *const draws_file = fopen(draws_path, "ab");
    if ((draws_file == NULL)
        || (!draws_existed && (fwrite(header.bytes, 1u, header.used, draws_file) != header.used)))
    {
        fprintf(stderr, "  %s did not open for writing\n", draws_path);
        return 1;
    }
    unsigned long long spent = 0ull;
    unsigned long long calls = 0ull;
    for (unsigned long long at = 0ull; at < run.count; at += 1ull)
    {
        const KneeSeries *const series = &run.series[at];
        KneeGroup *const group = &run.groups[series->group];
        unsigned int left = 0u;
        for (unsigned int turn = 0u; turn < KNEE_RANK; turn += 1u)
        {
            left += (run.done[(at * KNEE_RANK) + turn] == 0u) ? 1u : 0u;
        }
        if (left == 0u)
        {
            continue;
        }
        EngineSignum root;
        unsigned short *const volume = knee_load(series, &root);
        if (volume == NULL)
        {
            return 1;
        }
        for (unsigned int turn = 0u; turn < KNEE_RANK; turn += 1u)
        {
            if (run.done[(at * KNEE_RANK) + turn] != 0u)
            {
                continue;
            }
            const unsigned long long draw = series->place + (turn * group->count);
            KneeCall call;
            memset(&call, 0, sizeof(call));
            if (!knee_period_call(series, volume, &root, "knee period draw", draw, NULL, &call)
                || !knee_fold(group, call.heights))
            {
                free(volume);
                return 1;
            }
            fprintf(draws_file, "%s\t%llu", series->name, draw);
            for (unsigned int axis = 0u; axis < KNEE_RANK; axis += 1u)
            {
                fprintf(draws_file, "\t%llu/%llu", call.heights[axis].numerator, call.heights[axis].denominator);
            }
            fprintf(draws_file, "\t%llu\n", call.microseconds);
            fflush(draws_file);
            run.done[(at * KNEE_RANK) + turn] = 1u;
            drawn += 1ull;
            spent += call.microseconds;
            calls += 1ull;
        }
        free(volume);
        printf("  draws %llu/%llu  group %u  %s  %llu us a call so far\n", drawn, all_draws, series->group,
               series->name, spent / calls);
    }
    fclose(draws_file);

    // the tops, and the header the readings are made under
    KneeText tops = {NULL, 0u, 0u};
    int good = knee_text_add(&tops, "%s", header.bytes);
    for (unsigned int group = 0u; good && (group < group_count); group += 1u)
    {
        good = (run.groups[group].drawn == run.groups[group].draws);
        for (unsigned int axis = 0u; good && (axis < KNEE_RANK); axis += 1u)
        {
            good = knee_text_add(&tops, "# top %u axis %u %llu/%llu\n", group, axis,
                                 run.groups[group].top[axis].numerator, run.groups[group].top[axis].denominator);
        }
    }
    char *tops_path = NULL;
    if (!good)
    {
        fprintf(stderr, "  a group's draws are not all written; no tops are taken\n");
        return 1;
    }
    FILE *const tops_file = knee_path(&tops_path, out, "tops.tsv") ? fopen(tops_path, "wb") : NULL;
    if ((tops_file == NULL) || (fwrite(tops.bytes, 1u, tops.used, tops_file) != tops.used))
    {
        fprintf(stderr, "  the tops were not written\n");
        return 1;
    }
    fclose(tops_file);
    printf("%s", tops.bytes);

    // the readings: every series against its group's tops, with no draws of its own
    memset(run.done, 0, (size_t)run.count * KNEE_RANK);
    KneeText readings_header = {NULL, 0u, 0u};
    char *readings_path = NULL;
    int readings_existed = 0;
    if (!knee_text_add(&readings_header, "%s# series\tgroup\t(per axis: period\tcandidate\tmargin\tlags\tat\tbeside"
                                         "\tdouble\tbeside double)\tmicroseconds\n", tops.bytes)
        || !knee_path(&readings_path, out, "periods.tsv")
        || !knee_resume(readings_path, &readings_header, knee_take_reading, &run, &readings_existed))
    {
        return 1;
    }
    unsigned long long read_count = 0ull;
    for (unsigned long long at = 0ull; at < run.count; at += 1ull)
    {
        read_count += run.done[at];
    }
    FILE *const readings_file = fopen(readings_path, "ab");
    if ((readings_file == NULL)
        || (!readings_existed
            && (fwrite(readings_header.bytes, 1u, readings_header.used, readings_file) != readings_header.used)))
    {
        fprintf(stderr, "  %s did not open for writing\n", readings_path);
        return 1;
    }
    for (unsigned long long at = 0ull; at < run.count; at += 1ull)
    {
        if (run.done[at] != 0u)
        {
            continue;
        }
        const KneeSeries *const series = &run.series[at];
        EngineSignum root;
        unsigned short *const volume = knee_load(series, &root);
        KneeCall call;
        memset(&call, 0, sizeof(call));
        if ((volume == NULL)
            || !knee_period_call(series, volume, &root, "knee period read", 0ull, run.groups[series->group].top, &call))
        {
            free(volume);
            return 1;
        }
        free(volume);
        fprintf(readings_file, "%s\t%u", series->name, series->group);
        for (unsigned int axis = 0u; axis < KNEE_RANK; axis += 1u)
        {
            const PeriodAxis *const held = &call.reading.axis[axis];
            fprintf(readings_file, "\t%llu\t%llu\t%llu/%llu\t%llu\t%llu\t%llu\t%llu\t%llu", held->period,
                    held->candidate, held->margin.numerator, held->margin.denominator, held->lags,
                    held->agreement_at_candidate,
                    held->agreement_beside_candidate, held->agreement_at_double, held->agreement_beside_double);
        }
        fprintf(readings_file, "\t%llu\n", call.microseconds);
        fflush(readings_file);
        read_count += 1ull;
        printf("  read %llu/%llu  %s  P %llu %llu %llu\n", read_count, run.count, series->name,
               call.reading.axis[0].period, call.reading.axis[1].period, call.reading.axis[2].period);
    }
    fclose(readings_file);
    printf("  the period pass is done: %llu draws, %llu readings\n", drawn, read_count);
    return 0;
}
