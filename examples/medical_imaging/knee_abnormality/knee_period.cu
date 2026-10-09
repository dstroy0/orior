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
//   knee_period flatten --daemon <tessera_daemon> --readings <periods.tsv> --set <dir> [--list <file>]
//   knee_period spacing --daemon <tessera_daemon> --set <dir> --out <spacing.tsv>
//   knee_period ladder --daemon <tessera_daemon> --readings <periods.tsv> --spacing <spacing.tsv> --set <dir>
//                      --out <ladder.tsv> [--bodies <dir>] [--list <file>]
#include "engine.h"
#include "body_overlap.h"
#include "cycle.h"
#include "dicom.h"
#include "flatten.h"
#include "heaviest_matching.h"
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

// a series' crystal, lowered and proved against its seal, as one job holding the tower's and the coder's pools; `side`
// takes its side bytes where it is not NULL
static unsigned short *knee_load(const KneeSeries *series, EngineSignum *root, EngineSideBytes *side)
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
    const long loaded = engine_iapx_load(series->set, series->name, extent, &volume, root, side, &error);
    const int released = knee_job_release("knee crystal load", &job);
    if ((loaded != 0L) || !released || (memcmp(extent, series->extent, sizeof(extent)) != 0))
    {
        fprintf(stderr, "  %s: the crystal did not load (kind %u, module %u, site %u)\n", series->name, error.kind,
                error.module, error.site);
        free(volume);
        if ((loaded == 0L) && (side != NULL))
        {
            engine_side_release(side);
        }
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

typedef struct
{
    char **names;
    unsigned long long count;
    unsigned long long room;
} KneeNames;

// the ingest: every series named in the list, one a line, read from the source and sealed into the set as a crystal,
// one job each. A series whose crystal's head already reads is kept as it is, and a stopped ingest resumes from the
// set. A series that does not ingest is reported and the next one runs; the exit is 1 where any did not. While one
// series is sealed the next one's span of the source is read (engine_source_prefetch), and a list in the order the
// series lie in the source reads it start to end
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
    // the list whole: the series after each one is known while it is sealed
    KneeNames listed = {NULL, 0ull, 0ull};
    char *line = NULL;
    int held = 1;
    while (held && ((line = knee_line(named)) != NULL))
    {
        if (line[0] == '\0')
        {
            free(line);
            continue;
        }
        if (listed.count == listed.room)
        {
            listed.room = (listed.room != 0ull) ? (listed.room * 2ull) : 1024ull;
            char **const grown = (char **)realloc(listed.names, (size_t)listed.room * sizeof(char *));
            held = (grown != NULL);
            listed.names = held ? grown : listed.names;
        }
        if (held)
        {
            listed.names[listed.count] = line;
            listed.count += 1ull;
        }
    }
    fclose(named);
    if (!held)
    {
        fprintf(stderr, "  %s: no room to hold the list\n", list);
        return 1;
    }
    unsigned long long count = 0ull;
    unsigned long long kept = 0ull;
    unsigned long long sealed = 0ull;
    unsigned long long failed = 0ull;
    // the series whose span was last asked for ahead of it, past the list where none was
    unsigned long long asked_ahead = listed.count;
    for (unsigned long long at = 0ull; at < listed.count; at += 1ull)
    {
        char *const sample = listed.names[at];
        count += 1ull;
        EngineError error;
        memset(&error, 0, sizeof(error));
        unsigned long long extent[4] = {0ull, 0ull, 0ull, 0ull};
        if (engine_iapx_head(set, sample, extent, &error) == 0L)
        {
            kept += 1ull;
            continue;
        }
        memset(&error, 0, sizeof(error));
        // this series' span is asked for where the last series did not ask for it, and the next series not yet sealed
        // is asked for behind it: the source is read from one span into the next while each series is unpacked and
        // sealed
        EngineError ahead;
        memset(&ahead, 0, sizeof(ahead));
        if (asked_ahead != at)
        {
            engine_source_prefetch(source, sample, &ahead);
        }
        for (unsigned long long next = at + 1ull; next < listed.count; next += 1ull)
        {
            memset(&ahead, 0, sizeof(ahead));
            unsigned long long next_extent[4] = {0ull, 0ull, 0ull, 0ull};
            if (engine_iapx_head(set, listed.names[next], next_extent, &ahead) != 0L)
            {
                memset(&ahead, 0, sizeof(ahead));
                engine_source_prefetch(source, listed.names[next], &ahead);
                asked_ahead = next;
                break;
            }
        }
        unsigned long long lanes = 0ull;
        const unsigned long long reading = knee_now_microseconds();
        int good = engine_source_lanes(source, sample, &lanes, &error) == 0L;
        const unsigned long long shape[KNEE_RANK] = {lanes, 0ull, 0ull};
        KneeJob job;
        const unsigned long long asking = knee_now_microseconds();
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
            printf("  ingest %llu  %s  %llu lanes  %llu us  (read %llu us, job asked %llu us)\n", count, sample, lanes,
                   spent, asking - reading, started - asking);
        }
        else
        {
            failed += 1ull;
            fprintf(stderr, "  %s did not ingest (kind %u, module %u, site %u)\n", sample, error.kind, error.module,
                    error.site);
            printf("  ingest %llu  %s  did not ingest\n", count, sample);
        }
    }
    for (unsigned long long at = 0ull; at < listed.count; at += 1ull)
    {
        free(listed.names[at]);
    }
    free(listed.names);
    printf("  the ingest is done: %llu series named, %llu kept, %llu sealed, %llu did not ingest\n", count, kept,
           sealed, failed);
    return (failed == 0ull) ? 0 : 1;
}

typedef struct
{
    char *name;
    unsigned int period[KNEE_RANK];
} KneePeriod;

static int knee_period_order(const void *left, const void *right)
{
    return strcmp(((const KneePeriod *)left)->name, ((const KneePeriod *)right)->name);
}

// each series' P per axis from a pass's periods.tsv: the name, the group, then eight fields an axis with P first
static KneePeriod *knee_periods_read(const char *path, unsigned long long *count)
{
    *count = 0ull;
    FILE *const file = fopen(path, "rb");
    KneePeriod *periods = NULL;
    unsigned long long room = 0ull;
    char *line = NULL;
    int ok = (file != NULL);
    while (ok && ((line = knee_line(file)) != NULL))
    {
        if ((line[0] == '#') || (line[0] == '\0'))
        {
            free(line);
            continue;
        }
        if (*count == room)
        {
            room = (room != 0ull) ? (room * 2ull) : 1024ull;
            KneePeriod *const grown = (KneePeriod *)realloc(periods, (size_t)room * sizeof(KneePeriod));
            ok = (grown != NULL);
            periods = ok ? grown : periods;
        }
        KneePeriod *const period = ok ? &periods[*count] : NULL;
        char *field = line;
        unsigned int at = 0u;
        while (ok && (field != NULL))
        {
            char *const tab = strchr(field, '\t');
            if (tab != NULL)
            {
                *tab = '\0';
            }
            const unsigned int axis = (at >= 2u) ? ((at - 2u) / 8u) : KNEE_RANK;
            if ((axis < KNEE_RANK) && (((at - 2u) % 8u) == 0u))
            {
                char *end = NULL;
                const unsigned long long value = strtoull(field, &end, 10);
                ok = (end != field) && (*end == '\0') && (value <= 0xFFFFFFFFull);
                period->period[axis] = ok ? (unsigned int)value : 0u;
            }
            field = (tab != NULL) ? (tab + 1) : NULL;
            at += 1u;
        }
        ok = ok && (at >= (2u + (8u * KNEE_RANK)));
        if (ok)
        {
            period->name = line;
            *count += 1ull;
        }
        else
        {
            fprintf(stderr, "  %s: a reading line does not hold a name, a group and P on each axis\n", path);
            free(line);
        }
    }
    if (file != NULL)
    {
        fclose(file);
    }
    if (!ok)
    {
        for (unsigned long long at = 0ull; at < *count; at += 1ull)
        {
            free(periods[at].name);
        }
        free(periods);
        *count = 0ull;
        return NULL;
    }
    qsort(periods, (size_t)*count, sizeof(KneePeriod), knee_period_order);
    return periods;
}

// the residual's own check of a series' orders, on one voxel: its key is encoded from the orders alone, and the orders
// it refuses (an odd background order, or orders wider than its limbs) are refused here as they would be in the set
static int knee_orders_held(const FlattenOrders *orders, EngineError *error)
{
    unsigned short voxel = 0u;
    int offset_halves[ENGINE_AXES] = {0, 0, 0};
    EngineResidualRequest request;
    memset(&request, 0, sizeof(request));
    request.volume = &voxel;
    request.depth = 1u;
    request.height = 1u;
    request.width = 1u;
    memcpy(request.smooth_orders, orders->smooth, sizeof(request.smooth_orders));
    memcpy(request.background_orders, orders->background, sizeof(request.background_orders));
    memcpy(request.comb, orders->comb, sizeof(request.comb));
    request.unit_sweep = ENGINE_RESIDUAL_BY_UNIT_SWEEP;
    request.offset_halves = offset_halves;
    request.error = error;
    const unsigned int *device_residual = NULL;
    return engine_residual(&request, &device_residual) == 0L;
}

// the flatten: each series of a set lowered to its residual by its own orders and cut into bodies by the max tree, every
// body one vector magnitude in <set>/flattened.iapx (flatten_set). A series' orders are s = b = P on each axis, P read
// from the pass's periods.tsv, and P = 0 leaves that axis unsmoothed. A series whose orders the residual refuses is
// reported and left out; the rest are one job.
//
//   knee_period flatten --daemon <tessera_daemon> --readings <periods.tsv> --set <dir> [--list <file>]
static int knee_flatten(int argc, char **argv)
{
    const char *readings = NULL;
    const char *set = NULL;
    const char *list = NULL;
    for (int at = 1; (at + 1) < argc; at += 2)
    {
        const char *const value = argv[at + 1];
        s_daemon = (strcmp(argv[at], "--daemon") == 0) ? value : s_daemon;
        readings = (strcmp(argv[at], "--readings") == 0) ? value : readings;
        set = (strcmp(argv[at], "--set") == 0) ? value : set;
        list = (strcmp(argv[at], "--list") == 0) ? value : list;
    }
    if ((s_daemon == NULL) || (readings == NULL) || (set == NULL))
    {
        fprintf(stderr, "usage: knee_period flatten --daemon <tessera_daemon> --readings <periods.tsv> --set <dir>"
                        " [--list <file>]\n");
        return 2;
    }
    unsigned long long period_count = 0ull;
    KneePeriod *const periods = knee_periods_read(readings, &period_count);
    if (periods == NULL)
    {
        fprintf(stderr, "  %s did not read\n", readings);
        return 1;
    }
    char **names = NULL;
    unsigned int count = 0u;
    if (list != NULL)
    {
        FILE *const named = fopen(list, "rb");
        char *line = NULL;
        while ((named != NULL) && ((line = knee_line(named)) != NULL))
        {
            char **const grown = (line[0] != '\0') ? (char **)realloc(names, ((size_t)count + 1u) * sizeof(char *))
                                                   : NULL;
            if (grown == NULL)
            {
                free(line);
                continue;
            }
            names = grown;
            names[count] = line;
            count += 1u;
        }
        if (named != NULL)
        {
            fclose(named);
        }
    }
    else
    {
        count = engine_set_samples(set, &names);
    }
    FlattenOrders *const orders = (FlattenOrders *)calloc((size_t)count + 1u, sizeof(FlattenOrders));
    char **const kept = (char **)calloc((size_t)count + 1u, sizeof(char *));
    unsigned long long *const lanes = (unsigned long long *)calloc((size_t)count + 1u, sizeof(unsigned long long));
    if ((count == 0u) || (orders == NULL) || (kept == NULL) || (lanes == NULL))
    {
        fprintf(stderr, "  %s: no series, or no room to hold them\n", set);
        return 1;
    }
    unsigned long long largest = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        KneePeriod key;
        key.name = names[at];
        const KneePeriod *const found =
            (const KneePeriod *)bsearch(&key, periods, (size_t)period_count, sizeof(KneePeriod), knee_period_order);
        unsigned long long extent[4] = {0ull, 0ull, 0ull, 0ull};
        EngineError error;
        memset(&error, 0, sizeof(error));
        if ((found == NULL) || (engine_iapx_head(set, names[at], extent, &error) != 0L))
        {
            fprintf(stderr, "  %s: %s\n", names[at], (found == NULL) ? "no reading names it" : "the head did not read");
            return 1;
        }
        for (unsigned int axis = 0u; axis < KNEE_RANK; axis += 1u)
        {
            orders[at].smooth[axis] = found->period[axis];
            orders[at].background[axis] = found->period[axis];
        }
        lanes[at] = extent[0] * extent[1] * extent[2] * extent[3];
        largest = (lanes[at] > largest) ? lanes[at] : largest;
    }
    const unsigned long long shape[KNEE_RANK] = {largest, 0ull, 0ull};
    KneeJob job;
    if (!knee_job_submit("knee flatten", shape,
                         knee_allocation_bytes(largest * sizeof(unsigned short)) + tower_reserve_bytes(largest) +
                             compression_reserve_bytes(largest),
                         &job))
    {
        return 1;
    }
    // each distinct set of orders is checked once, and every series that shares it takes its answer
    unsigned char *const fits = (unsigned char *)calloc((size_t)count + 1u, 1u);
    EngineError *const refusals = (EngineError *)calloc((size_t)count + 1u, sizeof(EngineError));
    if ((fits == NULL) || (refusals == NULL))
    {
        knee_job_release("knee flatten", &job);
        return 1;
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        unsigned int earlier = 0u;
        while ((earlier < at) && (memcmp(&orders[earlier], &orders[at], sizeof(FlattenOrders)) != 0))
        {
            earlier += 1u;
        }
        if (earlier < at)
        {
            fits[at] = fits[earlier];
            refusals[at] = refusals[earlier];
        }
        else
        {
            fits[at] = knee_orders_held(&orders[at], &refusals[at]) ? 1u : 0u;
        }
    }
    unsigned int held = 0u;
    unsigned int left = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (fits[at] != 0u)
        {
            orders[held] = orders[at];
            kept[held] = names[at];
            held += 1u;
        }
        else
        {
            left += 1u;
            printf("  left out  %s  P %u %u %u  (kind %u, module %u, site %u)\n", names[at], orders[at].smooth[0],
                   orders[at].smooth[1], orders[at].smooth[2], refusals[at].kind, refusals[at].module,
                   refusals[at].site);
        }
    }
    free(fits);
    free(refusals);
    EngineError error;
    memset(&error, 0, sizeof(error));
    FlattenSetRequest flatten;
    memset(&flatten, 0, sizeof(flatten));
    flatten.set = set;
    flatten.names = kept;
    flatten.count = held;
    flatten.orders = orders;
    flatten.error = &error;
    const unsigned long long started = knee_now_microseconds();
    const int flattened = (held != 0u) && (flatten_set(&flatten) != 0);
    const int released = knee_job_release("knee flatten", &job);
    printf("  the flatten is done: %u series named, %u flattened, %u left out, %llu us\n", count,
           flattened ? held : 0u, left, knee_now_microseconds() - started);
    if (!flattened)
    {
        fprintf(stderr, "  the flatten did not finish (kind %u, module %u, site %u)\n", error.kind, error.module,
                error.site);
    }
    return (flattened && released) ? 0 : 1;
}

// the elements a voxel's size is read from: Spacing Between Slices (0018,0088) holds z, and Pixel Spacing (0028,0030)
// holds y then x, the spacing between rows and then between columns
#define KNEE_SPACING_ELEMENTS 2u

static const unsigned int KNEE_SPACING_GROUP[KNEE_SPACING_ELEMENTS] = {0x0018u, 0x0028u};

static const unsigned int KNEE_SPACING_TAG[KNEE_SPACING_ELEMENTS] = {0x0088u, 0x0030u};

static const unsigned int KNEE_SPACING_WANT[KNEE_SPACING_ELEMENTS] = {1u, 2u};

static const unsigned int KNEE_SPACING_AXIS[KNEE_SPACING_ELEMENTS] = {0u, 1u};

static void knee_exact_of(unsigned long long value, AnchorExactInteger *out)
{
    anchor_exact_zero(out);
    out->limb[0] = (uint32_t)(value & 0xFFFFFFFFull);
    out->limb[1] = (uint32_t)(value >> 32u);
    out->sign = (value != 0ull) ? 1 : 0;
}

// two decimals carried to the lesser exponent of the two, where their mantissas add, subtract and compare. Returns 0
// where the one at the larger exponent cannot be carried
static int knee_decimal_align(const DicomDecimal *one, const DicomDecimal *other, DicomDecimal *left,
                              DicomDecimal *right)
{
    *left = *one;
    *right = *other;
    DicomDecimal *const wide = (left->exponent > right->exponent) ? left : right;
    const long long low = (left->exponent < right->exponent) ? left->exponent : right->exponent;
    const long long gap = wide->exponent - low;
    // the gap is at most ANCHOR_EXACT_DIGITS here, and fits a uint32_t exactly
    if ((gap > (long long)ANCHOR_EXACT_DIGITS) ||
        (anchor_exact_scale_by_ten(&wide->mantissa, (uint32_t)gap) != ANCHOR_EXACT_OK))
    {
        return 0;
    }
    wide->exponent = low;
    return 1;
}

// the order of two decimals by value, -1, 0 or 1, into `order`; returns 0 where they do not align
static int knee_decimal_order(const DicomDecimal *one, const DicomDecimal *other, int *order)
{
    DicomDecimal left;
    DicomDecimal right;
    if (!knee_decimal_align(one, other, &left, &right))
    {
        return 0;
    }
    *order = anchor_exact_compare(&left.mantissa, &right.mantissa);
    return 1;
}

// one minus the other, exactly
static int knee_decimal_difference(const DicomDecimal *one, const DicomDecimal *other, DicomDecimal *out)
{
    DicomDecimal left;
    DicomDecimal right;
    if (!knee_decimal_align(one, other, &left, &right))
    {
        return 0;
    }
    out->exponent = left.exponent;
    return anchor_exact_subtract(&left.mantissa, &right.mantissa, &out->mantissa) == ANCHOR_EXACT_OK;
}

// one plus the other times itself, exactly: the sum of squares a distance is read from
static int knee_decimal_square_into(DicomDecimal *sum, const DicomDecimal *value)
{
    DicomDecimal square;
    square.exponent = 2ll * value->exponent;
    if (anchor_exact_multiply(&value->mantissa, &value->mantissa, &square.mantissa) != ANCHOR_EXACT_OK)
    {
        return 0;
    }
    if (sum->mantissa.sign == 0)
    {
        *sum = square;
        return 1;
    }
    DicomDecimal left;
    DicomDecimal right;
    if (!knee_decimal_align(sum, &square, &left, &right))
    {
        return 0;
    }
    sum->exponent = left.exponent;
    return anchor_exact_add(&left.mantissa, &right.mantissa, &sum->mantissa) == ANCHOR_EXACT_OK;
}

// a positive decimal as its mantissa's digits, e, and its exponent: 0.3125 is 3125e-4. The digits come nine at a time,
// the remainders of exact division by 10^9
static int knee_decimal_add(KneeText *text, const DicomDecimal *value)
{
    if (value->mantissa.sign <= 0)
    {
        return 0;
    }
    AnchorExactInteger rest = value->mantissa;
    AnchorExactInteger billion;
    knee_exact_of(1000000000ull, &billion);
    unsigned int groups[(ANCHOR_EXACT_DIGITS / 9u) + 2u];
    unsigned int count = 0u;
    while ((rest.sign != 0) && (count < ((ANCHOR_EXACT_DIGITS / 9u) + 2u)))
    {
        AnchorExactInteger quotient;
        AnchorExactInteger remainder;
        if (anchor_exact_divide(&rest, &billion, &quotient, &remainder) != ANCHOR_EXACT_OK)
        {
            return 0;
        }
        groups[count] = remainder.limb[0];
        count += 1u;
        rest = quotient;
    }
    int good = (rest.sign == 0) && knee_text_add(text, "\t%u", groups[count - 1u]);
    for (unsigned int group = count - 1u; good && (group > 0u); group -= 1u)
    {
        good = knee_text_add(text, "%09u", groups[group - 1u]);
    }
    return good && knee_text_add(text, "e%lld", value->exponent);
}

static int knee_text_order(const void *left, const void *right)
{
    return strcmp(*(const char *const *)left, *(const char *const *)right);
}

// a spacing line starts with its series; the names already written are kept in `context` and not read again
static int knee_take_spacing(const char *line, void *context)
{
    KneeNames *const written = (KneeNames *)context;
    const char *const tab = strchr(line, '\t');
    if (tab == NULL)
    {
        return 0;
    }
    if (written->count == written->room)
    {
        written->room = (written->room != 0ull) ? (written->room * 2ull) : 1024ull;
        char **const grown = (char **)realloc(written->names, (size_t)written->room * sizeof(char *));
        if (grown == NULL)
        {
            return 0;
        }
        written->names = grown;
    }
    char *const name = (char *)malloc((size_t)(tab - line) + 1u);
    if (name == NULL)
    {
        return 0;
    }
    memcpy(name, line, (size_t)(tab - line));
    name[tab - line] = '\0';
    written->names[written->count] = name;
    written->count += 1ull;
    return 1;
}

// the spacing: each series' voxel in mm, exactly as its DICOM headers write it, read from every slice's side bytes in
// its crystal. A line holds the series; z, y and x, each the largest any slice writes ("-" where no slice holds the
// element); the slices; for each element the slices that do not write that largest value; z² from the slices'
// positions, the largest squared distance from one slice's position to the next ("-" where a slice holds no
// position); and the gaps whose square is not that largest. A stopped run resumes from its file.
//
//   knee_period spacing --daemon <tessera_daemon> --set <dir> --out <spacing.tsv>
static int knee_spacing(int argc, char **argv)
{
    const char *set = NULL;
    const char *out = NULL;
    for (int at = 1; (at + 1) < argc; at += 2)
    {
        const char *const value = argv[at + 1];
        s_daemon = (strcmp(argv[at], "--daemon") == 0) ? value : s_daemon;
        set = (strcmp(argv[at], "--set") == 0) ? value : set;
        out = (strcmp(argv[at], "--out") == 0) ? value : out;
    }
    if ((s_daemon == NULL) || (set == NULL) || (out == NULL))
    {
        fprintf(stderr, "usage: knee_period spacing --daemon <tessera_daemon> --set <dir> --out <spacing.tsv>\n");
        return 2;
    }
    KneeText header = {NULL, 0u, 0u};
    KneeNames written = {NULL, 0ull, 0ull};
    int existed = 0;
    if (!knee_text_add(&header, "# series\tz\ty\tx\tslices\tapart (0018,0088)\tapart (0028,0030)\tz² from positions"
                                "\tgaps apart\n") ||
        !knee_resume(out, &header, knee_take_spacing, &written, &existed))
    {
        return 1;
    }
    qsort(written.names, (size_t)written.count, sizeof(char *), knee_text_order);
    FILE *const file = fopen(out, "ab");
    if ((file == NULL) || (!existed && (fwrite(header.bytes, 1u, header.used, file) != header.used)))
    {
        fprintf(stderr, "  %s did not open for writing\n", out);
        return 1;
    }
    char **names = NULL;
    const unsigned int count = engine_set_samples(set, &names);
    unsigned int read = 0u;
    unsigned int failed = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if ((written.count != 0ull) &&
            (bsearch(&names[at], written.names, (size_t)written.count, sizeof(char *), knee_text_order) != NULL))
        {
            continue;
        }
        KneeText line = {NULL, 0u, 0u};
        KneeSeries series;
        memset(&series, 0, sizeof(series));
        series.set = set;
        series.name = names[at];
        EngineError error;
        memset(&error, 0, sizeof(error));
        EngineSignum root;
        EngineSideBytes side;
        memset(&side, 0, sizeof(side));
        unsigned short *volume = NULL;
        int good = (engine_iapx_head(set, names[at], series.extent, &error) == 0L) && (series.extent[0] == 1ull) &&
                   ((volume = knee_load(&series, &root, &side)) != NULL) && (side.leaves != 0ull);
        free(volume);
        const size_t leaves = good ? (size_t)side.leaves : 0u;
        DicomDecimal *const read_values = (DicomDecimal *)calloc((leaves * KNEE_RANK) + 1u, sizeof(DicomDecimal));
        unsigned int *const read_found = (unsigned int *)calloc(leaves + 1u, sizeof(unsigned int));
        good = good && (read_values != NULL) && (read_found != NULL);
        DicomDecimal largest[KNEE_RANK];
        memset(largest, 0, sizeof(largest));
        unsigned int held[KNEE_RANK] = {0u, 0u, 0u};
        unsigned long long apart[KNEE_SPACING_ELEMENTS] = {0ull, 0ull};
        for (unsigned int element = 0u; good && (element < KNEE_SPACING_ELEMENTS); element += 1u)
        {
            const unsigned int axis = KNEE_SPACING_AXIS[element];
            const unsigned int want = KNEE_SPACING_WANT[element];
            for (size_t leaf = 0u; good && (leaf < leaves); leaf += 1u)
            {
                DicomDecimal *const value = &read_values[leaf * KNEE_RANK];
                good = dicom_side_decimal_list(&side, leaf, KNEE_SPACING_GROUP[element], KNEE_SPACING_TAG[element],
                                               want, value, &read_found[leaf], &error) != 0;
                for (unsigned int place = 0u; good && (read_found[leaf] != 0u) && (place < want); place += 1u)
                {
                    int order = 1;
                    good = (held[axis + place] == 0u) || knee_decimal_order(&value[place], &largest[axis + place],
                                                                            &order);
                    largest[axis + place] = (good && (order > 0)) ? value[place] : largest[axis + place];
                    held[axis + place] = 1u;
                }
            }
            for (size_t leaf = 0u; good && (leaf < leaves); leaf += 1u)
            {
                int same = (read_found[leaf] != 0u);
                for (unsigned int place = 0u; good && same && (place < want); place += 1u)
                {
                    int order = 0;
                    good = knee_decimal_order(&read_values[(leaf * KNEE_RANK) + place], &largest[axis + place],
                                              &order);
                    same = (order == 0);
                }
                apart[element] += (good && !same) ? 1ull : 0ull;
            }
        }
        // the squared distance between each slice's position (0020,0032) and the next one's, in the slices' sorted
        // order; the largest of them, and the gaps that differ from it
        DicomDecimal widest;
        memset(&widest, 0, sizeof(widest));
        unsigned int positioned = (leaves >= 2u) ? 1u : 0u;
        for (size_t leaf = 0u; good && positioned && (leaf < leaves); leaf += 1u)
        {
            good = dicom_side_decimal_list(&side, leaf, 0x0020u, 0x0032u, KNEE_RANK, &read_values[leaf * KNEE_RANK],
                                           &read_found[leaf], &error) != 0;
            positioned = good && (read_found[leaf] != 0u);
        }
        DicomDecimal *const gaps = (good && positioned) ? (DicomDecimal *)calloc(leaves, sizeof(DicomDecimal)) : NULL;
        positioned = positioned && (gaps != NULL);
        for (size_t leaf = 0u; good && positioned && ((leaf + 1u) < leaves); leaf += 1u)
        {
            for (unsigned int place = 0u; good && (place < KNEE_RANK); place += 1u)
            {
                DicomDecimal step;
                good = knee_decimal_difference(&read_values[((leaf + 1u) * KNEE_RANK) + place],
                                               &read_values[(leaf * KNEE_RANK) + place], &step) &&
                       knee_decimal_square_into(&gaps[leaf], &step);
            }
            int order = 1;
            good = good && ((leaf == 0u) || knee_decimal_order(&gaps[leaf], &widest, &order));
            widest = (good && (order > 0)) ? gaps[leaf] : widest;
        }
        unsigned long long gaps_apart = 0ull;
        for (size_t leaf = 0u; good && positioned && ((leaf + 1u) < leaves); leaf += 1u)
        {
            int order = 0;
            good = knee_decimal_order(&gaps[leaf], &widest, &order);
            gaps_apart += (good && (order != 0)) ? 1ull : 0ull;
        }
        positioned = positioned && (widest.mantissa.sign > 0);
        good = good && knee_text_add(&line, "%s", names[at]);
        for (unsigned int axis = 0u; good && (axis < KNEE_RANK); axis += 1u)
        {
            good = (held[axis] != 0u) ? knee_decimal_add(&line, &largest[axis]) : knee_text_add(&line, "\t-");
        }
        good = good && knee_text_add(&line, "\t%llu\t%llu\t%llu", side.leaves, apart[0], apart[1]);
        good = good && (positioned ? knee_decimal_add(&line, &widest) : knee_text_add(&line, "\t-"));
        good = good && knee_text_add(&line, "\t%llu\n", gaps_apart);
        free(read_values);
        free(read_found);
        free(gaps);
        engine_side_release(&side);
        if (good)
        {
            fwrite(line.bytes, 1u, line.used, file);
            fflush(file);
            read += 1u;
            printf("  spacing %u/%u  %s", at + 1u, count, line.bytes);
        }
        else
        {
            failed += 1u;
            fprintf(stderr, "  %s: the spacing did not read (kind %u, module %u, site %u)\n", names[at], error.kind,
                    error.module, error.site);
        }
        free(line.bytes);
    }
    fclose(file);
    printf("  the spacing is done: %u series, %u read, %u did not read\n", count, read, failed);
    return (failed == 0u) ? 0 : 1;
}

// each series' voxel from a spacing.tsv, as the square of z, y and x: z² from Spacing Between Slices where a slice
// writes it and from the slices' positions where none does; `held` 0 where neither is in the line
typedef struct
{
    char *name;
    unsigned int held[KNEE_RANK];
    DicomDecimal square[KNEE_RANK];
} KneeSpacing;

static int knee_spacing_order(const void *left, const void *right)
{
    return strcmp(((const KneeSpacing *)left)->name, ((const KneeSpacing *)right)->name);
}

// a field as knee_spacing writes it, the mantissa's digits, e, then the exponent, into `value`; `held` 0 for "-"
static int knee_spacing_field(const char *field, DicomDecimal *value, unsigned int *held)
{
    *held = 0u;
    if (strcmp(field, "-") == 0)
    {
        return 1;
    }
    const char *const mark = strchr(field, 'e');
    if ((mark == NULL) || (mark == field))
    {
        return 0;
    }
    char *end = NULL;
    const long long exponent = strtoll(mark + 1, &end, 10);
    if ((end == (mark + 1)) || (*end != '\0') ||
        (anchor_exact_from_decimal(field, (size_t)(mark - field), 0u, &value->mantissa) != ANCHOR_EXACT_OK) ||
        (value->mantissa.sign <= 0))
    {
        return 0;
    }
    value->exponent = exponent;
    *held = 1u;
    return 1;
}

static KneeSpacing *knee_spacings_read(const char *path, unsigned long long *count)
{
    *count = 0ull;
    FILE *const file = fopen(path, "rb");
    KneeSpacing *spacings = NULL;
    unsigned long long room = 0ull;
    char *line = NULL;
    int ok = (file != NULL);
    while (ok && ((line = knee_line(file)) != NULL))
    {
        if ((line[0] == '#') || (line[0] == '\0'))
        {
            free(line);
            continue;
        }
        if (*count == room)
        {
            room = (room != 0ull) ? (room * 2ull) : 1024ull;
            KneeSpacing *const grown = (KneeSpacing *)realloc(spacings, (size_t)room * sizeof(KneeSpacing));
            ok = (grown != NULL);
            spacings = ok ? grown : spacings;
        }
        KneeSpacing *const spacing = ok ? &spacings[*count] : NULL;
        if (ok)
        {
            memset(spacing, 0, sizeof(*spacing));
        }
        // the fields read: the name, z, y and x at 1 to 3, and z² from the positions at 7
        DicomDecimal value[KNEE_RANK];
        DicomDecimal positions;
        unsigned int value_held[KNEE_RANK] = {0u, 0u, 0u};
        unsigned int positions_held = 0u;
        char *field = line;
        unsigned int at = 0u;
        while (ok && (field != NULL))
        {
            char *const tab = strchr(field, '\t');
            if (tab != NULL)
            {
                *tab = '\0';
            }
            ok = ((at == 0u) || (at > 7u) || ((at > 3u) && (at < 7u))) ||
                 ((at <= 3u) ? knee_spacing_field(field, &value[at - 1u], &value_held[at - 1u])
                             : knee_spacing_field(field, &positions, &positions_held));
            field = (tab != NULL) ? (tab + 1) : NULL;
            at += 1u;
        }
        ok = ok && (at >= 8u);
        for (unsigned int axis = 0u; ok && (axis < KNEE_RANK); axis += 1u)
        {
            if (value_held[axis] != 0u)
            {
                ok = knee_decimal_square_into(&spacing->square[axis], &value[axis]);
                spacing->held[axis] = 1u;
            }
            else if ((axis == 0u) && (positions_held != 0u))
            {
                spacing->square[axis] = positions;
                spacing->held[axis] = 1u;
            }
        }
        if (ok)
        {
            spacing->name = line;
            *count += 1ull;
        }
        else
        {
            fprintf(stderr, "  %s: a spacing line does not hold a name, z, y, x and z² from the positions\n", path);
            free(line);
        }
    }
    if (file != NULL)
    {
        fclose(file);
    }
    if (!ok)
    {
        for (unsigned long long at = 0ull; at < *count; at += 1ull)
        {
            free(spacings[at].name);
        }
        free(spacings);
        *count = 0ull;
        return NULL;
    }
    qsort(spacings, (size_t)*count, sizeof(KneeSpacing), knee_spacing_order);
    return spacings;
}

// a rung's orders: on each axis its order n_a, laid as spaced pairs, the smooth's from 0 to n_a and the background's
// from n_a to 2 n_a; and the comb P
typedef struct
{
    unsigned int order[KNEE_RANK];
    unsigned int comb[KNEE_RANK];
    unsigned int smooth_spaced[KNEE_RANK][ENGINE_SPACINGS];
    unsigned int background_spaced[KNEE_RANK][ENGINE_SPACINGS];
} KneeOrders;

// spaced pairs from the order `from` to the order `to`, both even. A pair at spacing s adds the order 2 s², the
// variance of a binomial of that order. Each pair takes the widest s, a power of two, whose 2 s² is at most the order
// already reached (2 at the start) and at most what is left: the gaps between its taps fall inside the smooth before
// it. The pairs from 0 to n are the start of the pairs from 0 to 2 n
static void knee_spaced_pairs(unsigned long long from, unsigned long long to, unsigned int pairs[ENGINE_SPACINGS])
{
    unsigned long long reached = from;
    while (reached < to)
    {
        const unsigned long long room = (reached > 2ull) ? reached : 2ull;
        const unsigned long long left = to - reached;
        unsigned int spacing = 0u;
        while (((spacing + 1u) < ENGINE_SPACINGS) && ((2ull << (2u * (spacing + 1u))) <= room) &&
               ((2ull << (2u * (spacing + 1u))) <= left))
        {
            spacing += 1u;
        }
        pairs[spacing] += 1u;
        reached += 2ull << (2u * spacing);
    }
}

// the residual request a rung's orders make: no unit orders, the spaced pairs and the comb
static void knee_residual_orders(const KneeOrders *orders, EngineResidualRequest *request)
{
    memset(request->smooth_orders, 0, sizeof(request->smooth_orders));
    memset(request->background_orders, 0, sizeof(request->background_orders));
    memcpy(request->comb, orders->comb, sizeof(request->comb));
    memcpy(request->smooth_spaced, orders->smooth_spaced, sizeof(request->smooth_spaced));
    memcpy(request->background_spaced, orders->background_spaced, sizeof(request->background_spaced));
}

// knee_orders_held for a rung's orders
static int knee_rung_held(const KneeOrders *orders, EngineError *error)
{
    unsigned short voxel = 0u;
    int offset_halves[ENGINE_AXES] = {0, 0, 0};
    EngineResidualRequest request;
    memset(&request, 0, sizeof(request));
    request.volume = &voxel;
    request.depth = 1u;
    request.height = 1u;
    request.width = 1u;
    knee_residual_orders(orders, &request);
    request.unit_sweep = ENGINE_RESIDUAL_BY_UNIT_SWEEP;
    request.offset_halves = offset_halves;
    request.error = error;
    const unsigned int *device_residual = NULL;
    return engine_residual(&request, &device_residual) == 0L;
}

// a rung's orders. The axis of the finest voxel takes n; every other axis the largest even n_a with n_a · d_a² ≤
// n · d², d the finest spacing: no axis smooths wider in mm than the finest does. Returns 0 where an axis's spacing
// is not held or does not carry exactly
static int knee_rung_orders(const KneeSpacing *spacing, unsigned int n, const unsigned int comb[KNEE_RANK],
                            KneeOrders *orders)
{
    memset(orders, 0, sizeof(*orders));
    for (unsigned int axis = 0u; axis < KNEE_RANK; axis += 1u)
    {
        if ((spacing->held[axis] == 0u) || (spacing->square[axis].mantissa.sign <= 0))
        {
            return 0;
        }
    }
    unsigned int finest = 0u;
    for (unsigned int axis = 1u; axis < KNEE_RANK; axis += 1u)
    {
        int order = 0;
        if (!knee_decimal_order(&spacing->square[axis], &spacing->square[finest], &order))
        {
            return 0;
        }
        finest = (order < 0) ? axis : finest;
    }
    AnchorExactInteger width;
    knee_exact_of(n, &width);
    for (unsigned int axis = 0u; axis < KNEE_RANK; axis += 1u)
    {
        DicomDecimal finest_square;
        DicomDecimal axis_square;
        AnchorExactInteger numerator;
        AnchorExactInteger quotient;
        AnchorExactInteger remainder;
        if (!knee_decimal_align(&spacing->square[finest], &spacing->square[axis], &finest_square, &axis_square) ||
            (anchor_exact_multiply(&finest_square.mantissa, &width, &numerator) != ANCHOR_EXACT_OK) ||
            (anchor_exact_divide(&numerator, &axis_square.mantissa, &quotient, &remainder) != ANCHOR_EXACT_OK))
        {
            return 0;
        }
        // the quotient is at most n, which fits one limb
        for (unsigned int limb = 1u; limb < ANCHOR_EXACT_LIMBS; limb += 1u)
        {
            if (quotient.limb[limb] != 0u)
            {
                return 0;
            }
        }
        const unsigned int order = (unsigned int)quotient.limb[0] & ~1u;
        orders->order[axis] = order;
        orders->comb[axis] = comb[axis];
        knee_spaced_pairs(0ull, order, orders->smooth_spaced[axis]);
        knee_spaced_pairs(order, 2ull * order, orders->background_spaced[axis]);
    }
    return 1;
}

// the low 64 bits of the field of `bits` at `offset` in a packed magnitude
static unsigned long long knee_field_low(const unsigned int *magnitude, unsigned int offset, unsigned int bits)
{
    unsigned long long value = 0ull;
    for (unsigned int bit = 0u; (bit < bits) && (bit < 64u); bit += 1u)
    {
        const unsigned int at = offset + bit;
        value |= (unsigned long long)((magnitude[at / 32u] >> (at % 32u)) & 1u) << bit;
    }
    return value;
}

// steps making 2^power: a constant below 2^32, times 2^32 as many times as 32 goes into power. Returns the step that
// holds it, and moves `at` past the steps it wrote
static unsigned int knee_power_steps(EngineRecordStep *steps, unsigned int *at, unsigned int power)
{
    const unsigned int made = *at;
    steps[made] = {ENGINE_RECORD_CONSTANT, 1u << (power % 32u), 0u, 0u};
    *at += 1u;
    unsigned int held = made;
    if (power >= 32u)
    {
        const unsigned int word = *at;
        steps[word] = {ENGINE_RECORD_CONSTANT, 0u, 1u, 0u};
        *at += 1u;
        for (unsigned int times = 0u; times < (power / 32u); times += 1u)
        {
            steps[*at] = {ENGINE_RECORD_PRODUCT, held, word, 0u};
            held = *at;
            *at += 1u;
        }
    }
    return held;
}

#define KNEE_LADDER_OWN 0u
#define KNEE_LADDER_BEFORE 1u
#define KNEE_LADDER_AFTER 2u
#define KNEE_LADDER_MEMBERS 3u

#define KNEE_LADDER_ROSE 0u
#define KNEE_LADDER_PEAKED 1u
#define KNEE_LADDER_OUTPUTS 2u

// the verdicts over a lane of three bodies, its own, its partner on the rung before and its partner on the rung after,
// each read by its level, the residual at its peak voxel. A rung's residual has the gain 2^G, G its background term's
// bits (knee_rung_gain), and a level over 2^G is the response normalized; `rise` is G here less G before, and `fall`
// G after less G here: the levels compare exactly with no division. A partner a lane does not have is a body of
// level 0.
//   rose: the response here is above the one before, c (c + 1) with c = COMPARE(level, before · 2^rise)
//   peaked: rose, and the response here is not below the one after, rose · (COMPARE(level · 2^fall, after) + 1)
static EngineRecordStep *knee_ladder_program(unsigned int rise, unsigned int fall, unsigned int *count,
                                             unsigned int outputs[KNEE_LADDER_OUTPUTS])
{
    EngineRecordStep *const steps =
        (EngineRecordStep *)calloc(24u + (2u * ((rise / 32u) + (fall / 32u))), sizeof(EngineRecordStep));
    if (steps == NULL)
    {
        return NULL;
    }
    steps[0] = {ENGINE_RECORD_FIELD, MAX_TREE_FIELD_LEVEL, 0u, KNEE_LADDER_OWN};
    steps[1] = {ENGINE_RECORD_FIELD, MAX_TREE_FIELD_LEVEL, 0u, KNEE_LADDER_BEFORE};
    steps[2] = {ENGINE_RECORD_FIELD, MAX_TREE_FIELD_LEVEL, 0u, KNEE_LADDER_AFTER};
    steps[3] = {ENGINE_RECORD_CONSTANT, 1u, 0u, 0u};
    unsigned int at = 4u;
    const unsigned int rise_power = knee_power_steps(steps, &at, rise);
    const unsigned int fall_power = knee_power_steps(steps, &at, fall);
    const unsigned int before = at;
    steps[at++] = {ENGINE_RECORD_PRODUCT, 1u, rise_power, 0u};
    const unsigned int own = at;
    steps[at++] = {ENGINE_RECORD_PRODUCT, 0u, fall_power, 0u};
    const unsigned int rise_order = at;
    steps[at++] = {ENGINE_RECORD_COMPARE, 0u, before, 0u};
    const unsigned int rise_up = at;
    steps[at++] = {ENGINE_RECORD_SUM, rise_order, 3u, 0u};
    const unsigned int rose = at;
    steps[at++] = {ENGINE_RECORD_PRODUCT, rise_order, rise_up, 0u};
    const unsigned int fall_order = at;
    steps[at++] = {ENGINE_RECORD_COMPARE, own, 2u, 0u};
    const unsigned int held = at;
    steps[at++] = {ENGINE_RECORD_SUM, fall_order, 3u, 0u};
    const unsigned int peaked = at;
    steps[at++] = {ENGINE_RECORD_PRODUCT, rose, held, 0u};
    outputs[KNEE_LADDER_ROSE] = rose;
    outputs[KNEE_LADDER_PEAKED] = peaked;
    *count = at;
    return steps;
}

// one rung of a series: its orders, its bodies' packed magnitudes with a body of level 0 after them, their peak voxels
// in ascending order, each body's partner on the rung before (`count` of that rung where it has none) and on the rung
// after, and the voxels' labels and positive words the next rung is linked against
typedef struct
{
    KneeOrders orders;
    unsigned long long gain;
    unsigned int count;
    unsigned int *magnitudes;
    unsigned int *peaks;
    unsigned int *before;
    unsigned int *after;
    unsigned int *labels;
    unsigned long long *positive;
} KneeRung;

static void knee_rung_release(KneeRung *rung)
{
    free(rung->magnitudes);
    free(rung->peaks);
    free(rung->before);
    free(rung->after);
    free(rung->labels);
    free(rung->positive);
    memset(rung, 0, sizeof(*rung));
}

static int knee_peak_order(const void *left, const void *right)
{
    const unsigned int one = *(const unsigned int *)left;
    const unsigned int other = *(const unsigned int *)right;
    return (one < other) ? -1 : ((one > other) ? 1 : 0);
}

// the body of `peak` on a rung, or the rung's count where no body has it
static unsigned int knee_body_of(const KneeRung *rung, unsigned int peak)
{
    const unsigned int *const found =
        (const unsigned int *)bsearch(&peak, rung->peaks, (size_t)rung->count, sizeof(unsigned int), knee_peak_order);
    return (found != NULL) ? (unsigned int)(found - rung->peaks) : rung->count;
}

// a rung cut into bodies: the residual on the device, the max tree's bodies and labels, and the bodies packed and
// brought to the host. Returns 1 when cut, -1 where the residual refuses the request before it runs (a line of the
// extent too long for the unit sweep at these orders), and 0 on any other failure
static int knee_rung_cut(const unsigned short *volume, const unsigned int extent[KNEE_RANK],
                         const MaxTreeLayout *layout, unsigned int frame, KneeRung *rung, EngineError *error)
{
    const size_t voxels = (size_t)extent[0] * extent[1] * extent[2];
    const size_t words = (voxels + 63u) / 64u;
    rung->labels = (unsigned int *)malloc(voxels * sizeof(unsigned int));
    rung->positive = (unsigned long long *)malloc(words * sizeof(unsigned long long));
    int offset_halves[ENGINE_AXES] = {0, 0, 0};
    EngineResidualRequest residual;
    memset(&residual, 0, sizeof(residual));
    residual.volume = volume;
    residual.depth = extent[0];
    residual.height = extent[1];
    residual.width = extent[2];
    knee_residual_orders(&rung->orders, &residual);
    residual.unit_sweep = ENGINE_RESIDUAL_BY_UNIT_SWEEP;
    residual.offset_halves = offset_halves;
    residual.error = error;
    const unsigned int *device_residual = NULL;
    if ((rung->labels == NULL) || (rung->positive == NULL))
    {
        return 0;
    }
    if (engine_residual(&residual, &device_residual) != 0L)
    {
        return (error->kind == ENGINE_ERROR_REQUEST) ? -1 : 0;
    }
    int ok = 1;
    unsigned int proof_count = 0u;
    unsigned int differ = 0u;
    MaxTreeObjectsRequest objects;
    memset(&objects, 0, sizeof(objects));
    objects.device_residual = device_residual;
    objects.depth = extent[0];
    objects.height = extent[1];
    objects.width = extent[2];
    objects.labels = rung->labels;
    objects.positive_words = rung->positive;
    objects.proof_count = &proof_count;
    objects.faces_differ = &differ;
    objects.error = error;
    const long found = max_tree_objects(&objects);
    if (ok && ((found < 0L) || (proof_count != 1u)))
    {
        fprintf(stderr, "  rung %u: the max tree found %ld bodies, %u proofs\n", frame, found, proof_count);
    }
    ok = (found >= 0L) && (proof_count == 1u);
    rung->count = ok ? (unsigned int)found : 0u;
    unsigned int *device_magnitudes = NULL;
    const size_t limbs = (size_t)layout->limbs;
    rung->magnitudes = ok ? (unsigned int *)calloc(((size_t)rung->count + 1u) * limbs, sizeof(unsigned int)) : NULL;
    rung->peaks = ok ? (unsigned int *)malloc(((size_t)rung->count + 1u) * sizeof(unsigned int)) : NULL;
    rung->before = ok ? (unsigned int *)malloc(((size_t)rung->count + 1u) * sizeof(unsigned int)) : NULL;
    rung->after = ok ? (unsigned int *)malloc(((size_t)rung->count + 1u) * sizeof(unsigned int)) : NULL;
    ok = ok && (rung->magnitudes != NULL) && (rung->peaks != NULL) && (rung->before != NULL) && (rung->after != NULL) &&
         (cudaMalloc((void **)&device_magnitudes, ((size_t)rung->count + 1u) * limbs * sizeof(unsigned int)) ==
          cudaSuccess);
    unsigned long long lost = 0ull;
    MaxTreePackRequest pack;
    memset(&pack, 0, sizeof(pack));
    pack.layout = layout;
    pack.frame = frame;
    pack.device_magnitudes = device_magnitudes;
    pack.mismatches = &lost;
    pack.error = error;
    ok = ok && (max_tree_pack(&pack) == found) && (lost == 0ull) &&
         ((rung->count == 0u) ||
          (cudaMemcpy(rung->magnitudes, device_magnitudes, (size_t)rung->count * limbs * sizeof(unsigned int),
                      cudaMemcpyDeviceToHost) == cudaSuccess));
    cudaFree(device_magnitudes);
    if ((found >= 0L) && !ok)
    {
        fprintf(stderr, "  rung %u: %u bodies did not pack (%llu bits lost)\n", frame, rung->count, lost);
    }
    for (unsigned int body = 0u; ok && (body < rung->count); body += 1u)
    {
        const unsigned int *const magnitude = &rung->magnitudes[(size_t)body * limbs];
        rung->peaks[body] = (unsigned int)knee_field_low(magnitude, layout->offset[MAX_TREE_FIELD_PEAK],
                                                         layout->bits[MAX_TREE_FIELD_PEAK]);
        // the bodies are packed in the order of their peak voxels, and a peak is found by bisection
        ok = (body == 0u) || (rung->peaks[body] > rung->peaks[body - 1u]);
        if (!ok)
        {
            fprintf(stderr, "  rung %u: body %u's peak %u is not past body %u's %u\n", frame, body, rung->peaks[body],
                    body - 1u, rung->peaks[body - 1u]);
        }
        rung->before[body] = 0u;
        rung->after[body] = 0u;
    }
    return ok;
}

// each body's partner on the rung after it: the voxels both rungs' bodies hold, counted for every pair of bodies
// (body_overlap), and the heaviest matching of those counts
static int knee_rung_link(KneeRung *earlier, KneeRung *later, const unsigned int extent[KNEE_RANK])
{
    for (unsigned int body = 0u; body < earlier->count; body += 1u)
    {
        earlier->after[body] = later->count;
    }
    for (unsigned int body = 0u; body < later->count; body += 1u)
    {
        later->before[body] = earlier->count;
    }
    if ((earlier->count == 0u) || (later->count == 0u))
    {
        return 1;
    }
    unsigned int capacity = earlier->count + later->count;
    unsigned int *peaks_before = NULL;
    unsigned int *peaks_after = NULL;
    unsigned int *counts = NULL;
    long total = -1L;
    for (;;)
    {
        free(peaks_before);
        free(peaks_after);
        free(counts);
        peaks_before = (unsigned int *)malloc((size_t)capacity * sizeof(unsigned int));
        peaks_after = (unsigned int *)malloc((size_t)capacity * sizeof(unsigned int));
        counts = (unsigned int *)malloc((size_t)capacity * sizeof(unsigned int));
        if ((peaks_before == NULL) || (peaks_after == NULL) || (counts == NULL))
        {
            total = -1L;
            break;
        }
        BodyOverlapRequest overlap;
        memset(&overlap, 0, sizeof(overlap));
        overlap.labels_before = earlier->labels;
        overlap.positive_before = earlier->positive;
        overlap.labels_after = later->labels;
        overlap.positive_after = later->positive;
        overlap.axes = KNEE_RANK;
        for (unsigned int axis = 0u; axis < KNEE_RANK; axis += 1u)
        {
            overlap.extents[axis] = extent[axis];
        }
        overlap.voxels = extent[0] * extent[1] * extent[2];
        overlap.capacity = capacity;
        overlap.peaks_before = peaks_before;
        overlap.peaks_after = peaks_after;
        overlap.counts = counts;
        total = body_overlap_run(&overlap);
        if ((total < 0L) || ((unsigned long)total <= (unsigned long)capacity))
        {
            break;
        }
        capacity = (unsigned int)total;
    }
    const unsigned int pairs = (total > 0L) ? (unsigned int)total : 0u;
    unsigned int *const before = (unsigned int *)malloc(((size_t)pairs + 1u) * sizeof(unsigned int));
    unsigned int *const after = (unsigned int *)malloc(((size_t)pairs + 1u) * sizeof(unsigned int));
    unsigned int *const weights = (unsigned int *)malloc(((size_t)pairs + 1u) * sizeof(unsigned int));
    unsigned char *const chosen = (unsigned char *)calloc((size_t)pairs + 1u, 1u);
    int ok = (total >= 0L) && (before != NULL) && (after != NULL) && (weights != NULL) && (chosen != NULL);
    unsigned int kept = 0u;
    for (unsigned int pair = 0u; ok && (pair < pairs); pair += 1u)
    {
        const unsigned int one = knee_body_of(earlier, peaks_before[pair]);
        const unsigned int other = knee_body_of(later, peaks_after[pair]);
        if ((one == earlier->count) || (other == later->count))
        {
            continue;
        }
        before[kept] = one;
        after[kept] = other;
        weights[kept] = counts[pair];
        kept += 1u;
    }
    HeaviestMatchingRequest matching;
    memset(&matching, 0, sizeof(matching));
    matching.before = before;
    matching.after = after;
    matching.counts = weights;
    matching.pairs = kept;
    matching.before_count = earlier->count;
    matching.after_count = later->count;
    matching.chosen = chosen;
    if (!ok)
    {
        fprintf(stderr, "  the overlap of %u and %u bodies did not run (%ld pairs)\n", earlier->count, later->count,
                total);
    }
    ok = ok && (heaviest_matching_run(&matching) != HEAVIEST_MATCHING_ERROR);
    for (unsigned int pair = 0u; ok && (pair < kept); pair += 1u)
    {
        if (chosen[pair] != 0u)
        {
            earlier->after[before[pair]] = after[pair];
            later->before[after[pair]] = before[pair];
        }
    }
    free(peaks_before);
    free(peaks_after);
    free(counts);
    free(before);
    free(after);
    free(weights);
    free(chosen);
    return ok;
}

// the verdicts over every body of `own`, its partners read from `before` and `after` (either may be `own` itself, its
// level-0 body standing in), run on the device and on the host, which must agree word for word. `counted` takes how
// many lanes are truthy for each verdict, and `truthy[output]`, where it is not NULL, 1 for each lane truthy on it
static int knee_rung_verdicts(const KneeRung *own, const KneeRung *before, const unsigned int *before_index,
                              const KneeRung *after, const unsigned int *after_index, unsigned long long rise,
                              unsigned long long fall, const MaxTreeLayout *layout,
                              unsigned long long counted[KNEE_LADDER_OUTPUTS],
                              unsigned char *const truthy[KNEE_LADDER_OUTPUTS], EngineError *error)
{
    counted[KNEE_LADDER_ROSE] = 0ull;
    counted[KNEE_LADDER_PEAKED] = 0ull;
    if (own->count == 0u)
    {
        return 1;
    }
    if ((rise > 0xFFFFFFFFull) || (fall > 0xFFFFFFFFull))
    {
        return 0;
    }
    unsigned int steps = 0u;
    unsigned int outputs[KNEE_LADDER_OUTPUTS];
    // a gain difference past 32 bits is refused above, and re-signs exactly
    EngineRecordStep *const program = knee_ladder_program((unsigned int)rise, (unsigned int)fall, &steps, outputs);
    unsigned int output_offset[KNEE_LADDER_OUTPUTS] = {0u, 0u};
    unsigned int output_bits[KNEE_LADDER_OUTPUTS] = {0u, 0u};
    EngineRecordRequest encode;
    memset(&encode, 0, sizeof(encode));
    encode.steps = program;
    encode.count = steps;
    encode.field_bits = layout->bits;
    encode.field_offset = layout->offset;
    encode.fields = MAX_TREE_FIELDS;
    for (unsigned int member = 0u; member < KNEE_LADDER_MEMBERS; member += 1u)
    {
        encode.in_limbs[member] = layout->limbs;
    }
    encode.members = KNEE_LADDER_MEMBERS;
    encode.outputs = outputs;
    encode.output_count = KNEE_LADDER_OUTPUTS;
    encode.output_offset = output_offset;
    encode.output_bits = output_bits;
    CycleRecord *record = NULL;
    int ok = (program != NULL) && (engine_record_encode(&encode, &record, error) != ENGINE_ERROR);
    const unsigned int out_limbs = ok ? cycle_record_out_limbs(record) : 0u;
    const size_t lanes = (size_t)own->count;
    unsigned int *const index = (unsigned int *)malloc(lanes * KNEE_LADDER_MEMBERS * sizeof(unsigned int));
    unsigned int *const device = (unsigned int *)malloc((lanes * out_limbs + 1u) * sizeof(unsigned int));
    unsigned int *const host = (unsigned int *)malloc((lanes * out_limbs + 1u) * sizeof(unsigned int));
    ok = ok && (index != NULL) && (device != NULL) && (host != NULL);
    for (size_t lane = 0u; ok && (lane < lanes); lane += 1u)
    {
        index[(lane * KNEE_LADDER_MEMBERS) + KNEE_LADDER_OWN] = (unsigned int)lane;
        index[(lane * KNEE_LADDER_MEMBERS) + KNEE_LADDER_BEFORE] = before_index[lane];
        index[(lane * KNEE_LADDER_MEMBERS) + KNEE_LADDER_AFTER] = after_index[lane];
    }
    unsigned long long device_us = 0ull;
    unsigned long long host_us = 0ull;
    EngineRecordSweep sweep = {record,
                               {own->magnitudes, before->magnitudes, after->magnitudes},
                               {(unsigned long long)own->count + 1ull, (unsigned long long)before->count + 1ull,
                                (unsigned long long)after->count + 1ull},
                               index,
                               (unsigned long long)lanes,
                               device,
                               &device_us,
                               error};
    if (!ok)
    {
        fprintf(stderr, "  the verdicts did not encode (%u steps, kind %u, module %u, site %u)\n", steps, error->kind,
                error->module, error->site);
    }
    const long swept = ok ? engine_record_sweep(&sweep) : -1L;
    sweep.records = host;
    sweep.sweep_microseconds = &host_us;
    const long proved = (swept == (long)lanes) ? engine_record_host(&encode, &sweep) : -1L;
    const int same = (proved == (long)lanes) && (memcmp(device, host, lanes * out_limbs * sizeof(unsigned int)) == 0);
    if (ok && !same)
    {
        fprintf(stderr, "  the verdicts over %zu lanes: device %ld, host %ld, %s (kind %u, module %u, site %u)\n", lanes,
                swept, proved, (proved == (long)lanes) ? "they differ" : "not both ran", error->kind, error->module,
                error->site);
    }
    ok = ok && same;
    for (size_t lane = 0u; ok && (lane < lanes); lane += 1u)
    {
        for (unsigned int output = 0u; output < KNEE_LADDER_OUTPUTS; output += 1u)
        {
            const unsigned int held =
                (knee_field_low(&device[lane * out_limbs], output_offset[output], output_bits[output]) != 0ull) ? 1u
                                                                                                                : 0u;
            counted[output] += held;
            if (truthy[output] != NULL)
            {
                truthy[output][lane] = (unsigned char)held;
            }
        }
    }
    if (record != NULL)
    {
        cycle_record_release(record);
    }
    free(program);
    free(index);
    free(device);
    free(host);
    return ok;
}

// a rung's gain exponent: 2 bits for each spaced pair, the smooth's and the background's
static unsigned long long knee_rung_gain(const KneeOrders *orders)
{
    unsigned long long gain = 0ull;
    for (unsigned int axis = 0u; axis < KNEE_RANK; axis += 1u)
    {
        for (unsigned int spacing = 0u; spacing < ENGINE_SPACINGS; spacing += 1u)
        {
            gain += 2ull * ((unsigned long long)orders->smooth_spaced[axis][spacing] +
                            orders->background_spaced[axis][spacing]);
        }
    }
    return gain;
}

// the background term's window on an axis: a pair at spacing s widens it by 2 s taps
static unsigned long long knee_rung_window(const KneeOrders *orders, unsigned int axis)
{
    unsigned long long taps = 1ull;
    for (unsigned int spacing = 0u; spacing < ENGINE_SPACINGS; spacing += 1u)
    {
        taps += (2ull << spacing) * ((unsigned long long)orders->smooth_spaced[axis][spacing] +
                                     orders->background_spaced[axis][spacing]);
    }
    return taps;
}

// the bodies a series keeps, each one packed magnitude of `limbs` words
typedef struct
{
    unsigned int *words;
    size_t count;
    size_t room;
    unsigned int limbs;
} KneeKept;

static int knee_kept_add(KneeKept *kept, const unsigned int *magnitude)
{
    if (kept->count == kept->room)
    {
        const size_t room = (kept->room != 0u) ? (kept->room * 2u) : 1024u;
        unsigned int *const grown = (unsigned int *)realloc(kept->words, room * kept->limbs * sizeof(unsigned int));
        if (grown == NULL)
        {
            return 0;
        }
        kept->words = grown;
        kept->room = room;
    }
    memcpy(&kept->words[kept->count * kept->limbs], magnitude, kept->limbs * sizeof(unsigned int));
    kept->count += 1u;
    return 1;
}

// a series' ladder file, <dir>/<series>.ladder, all of it 32-bit words: the fields (MAX_TREE_FIELDS), the limbs a body,
// the rungs, the peaked bodies and the open bodies; each field's bits, then each field's offset; for each rung its
// order on z, y and x and its gain's low and high words; then the peaked bodies' magnitudes, each in its rung's FRAME
// field, and after them the open bodies', the ones the last rung leaves rising. It is written whole and then renamed
static int knee_ladder_write(const char *dir, const char *name, const MaxTreeLayout *layout,
                             const unsigned int *rung_words, unsigned int rungs, const KneeKept *kept,
                             size_t peaked)
{
    KneeText path = {NULL, 0u, 0u};
    KneeText part = {NULL, 0u, 0u};
    int ok = knee_text_add(&path, "%s/%s.ladder", dir, name) && knee_text_add(&part, "%s.part", path.bytes);
    FILE *const file = ok ? fopen(part.bytes, "wb") : NULL;
    // the counts are a series' bodies and rungs, held far below 2^32 by the volume's own voxels
    const unsigned int head[5] = {MAX_TREE_FIELDS, layout->limbs, rungs, (unsigned int)peaked,
                                  (unsigned int)(kept->count - peaked)};
    ok = (file != NULL) && (fwrite(head, sizeof(unsigned int), 5u, file) == 5u) &&
         (fwrite(layout->bits, sizeof(unsigned int), MAX_TREE_FIELDS, file) == MAX_TREE_FIELDS) &&
         (fwrite(layout->offset, sizeof(unsigned int), MAX_TREE_FIELDS, file) == MAX_TREE_FIELDS) &&
         (fwrite(rung_words, sizeof(unsigned int), (size_t)rungs * 5u, file) == ((size_t)rungs * 5u)) &&
         (fwrite(kept->words, sizeof(unsigned int), kept->count * kept->limbs, file) == (kept->count * kept->limbs));
    ok = (file != NULL) && (fclose(file) == 0) && ok;
    remove(path.bytes);
    ok = ok && (rename(part.bytes, path.bytes) == 0);
    if (!ok)
    {
        fprintf(stderr, "  %s was not written\n", (path.bytes != NULL) ? path.bytes : name);
    }
    free(path.bytes);
    free(part.bytes);
    return ok;
}

// rung k smooths and takes the background at n = 2^(k + 1) on the finest axis (knee_rung_orders), a difference of
// smooths whose doubling keeps Lindeberg's t / Δt the same on every rung, each smooth laid as spaced pairs so its bits
// grow with the count of its pairs. A rung is cut into bodies, linked to the rung before, and its verdicts run: which
// bodies rose, and which bodies of the rung before peaked. The ladder ends where no body rose, where an axis's window
// is longer than the axis, or where the residual refuses the orders. Each rung is one line; the last line of a series
// says why it ended and how many bodies are still open. Where `bodies` is not NULL the peaked and open bodies are
// written there (knee_ladder_write)
#define KNEE_LADDER_RUNGS 32u

static int knee_ladder_series(const KneeSeries *series, const KneeSpacing *spacing,
                              const unsigned int comb[KNEE_RANK], const char *bodies, KneeText *text)
{
    const unsigned int extent[KNEE_RANK] = {(unsigned int)series->extent[1], (unsigned int)series->extent[2],
                                            (unsigned int)series->extent[3]};
    const unsigned long long voxels = (unsigned long long)extent[0] * extent[1] * extent[2];
    EngineSignum root;
    unsigned short *const volume = knee_load(series, &root, NULL);
    if (volume == NULL)
    {
        return 0;
    }
    MaxTreeLayout layout;
    max_tree_layout(extent[0], extent[1], extent[2], KNEE_LADDER_RUNGS, 1u, &layout);
    unsigned long long shape[KNEE_RANK];
    knee_shape(series, shape);
    KneeJob job;
    if (!knee_job_submit("knee ladder", shape,
                         knee_allocation_bytes(voxels * sizeof(unsigned short)) +
                             (2ull * knee_allocation_bytes(voxels * ENGINE_RESIDUAL_LIMBS * sizeof(unsigned int))),
                         &job))
    {
        free(volume);
        return 0;
    }
    KneeRung rungs[3];
    memset(rungs, 0, sizeof(rungs));
    EngineError error;
    memset(&error, 0, sizeof(error));
    int ok = 1;
    const char *ended = "no body rose";
    unsigned long long open = 0ull;
    unsigned int made = 0u;
    KneeKept kept = {NULL, 0u, 0u, layout.limbs};
    unsigned int rung_words[KNEE_LADDER_RUNGS * 5u];
    unsigned char *last_rose = NULL;
    const KneeRung *last = NULL;
    for (unsigned int step = 0u; ok && (step < KNEE_LADDER_RUNGS); step += 1u)
    {
        KneeRung *const here = &rungs[step % 3u];
        KneeRung *const before = &rungs[(step + 2u) % 3u];
        KneeRung *const earliest = &rungs[(step + 1u) % 3u];
        knee_rung_release(here);
        if (!knee_rung_orders(spacing, 2u << step, comb, &here->orders))
        {
            ended = "the spacing does not carry";
            ok = 0;
            break;
        }
        unsigned int longer = 0u;
        for (unsigned int axis = 0u; axis < KNEE_RANK; axis += 1u)
        {
            longer |= (unsigned int)(knee_rung_window(&here->orders, axis) > (unsigned long long)extent[axis]);
        }
        EngineError refusal;
        memset(&refusal, 0, sizeof(refusal));
        if (longer != 0u)
        {
            ended = "a window is longer than its axis";
            break;
        }
        if (!knee_rung_held(&here->orders, &refusal))
        {
            ended = "the residual refuses the orders";
            break;
        }
        const unsigned long long mark = knee_now_microseconds();
        here->gain = knee_rung_gain(&here->orders);
        const int cut = knee_rung_cut(volume, extent, &layout, step, here, &error);
        if (cut < 0)
        {
            ended = "the residual refuses the orders on this extent";
            memset(&error, 0, sizeof(error));
            engine_error_clear();
            break;
        }
        ok = (cut > 0) && ((step == 0u) || knee_rung_link(before, here, extent));
        for (unsigned int body = 0u; ok && (step == 0u) && (body < here->count); body += 1u)
        {
            here->before[body] = here->count;
        }
        for (unsigned int body = 0u; ok && (body < here->count); body += 1u)
        {
            here->after[body] = here->count;
        }
        // the rung's own: each body against its partner before, with no partner after
        unsigned long long own_counted[KNEE_LADDER_OUTPUTS] = {0ull, 0ull};
        unsigned char *const rose = ok ? (unsigned char *)calloc((size_t)here->count + 1u, 1u) : NULL;
        unsigned char *const own_truthy[KNEE_LADDER_OUTPUTS] = {rose, NULL};
        ok = ok && (rose != NULL) &&
             knee_rung_verdicts(here, (step == 0u) ? here : before, here->before, here, here->after,
                                (step == 0u) ? 0ull : (here->gain - before->gain), 0ull, &layout, own_counted,
                                own_truthy, &error);
        // the rung before: each body against both its partners, now that the rung after it is cut. Its peaked bodies
        // are kept
        unsigned long long before_counted[KNEE_LADDER_OUTPUTS] = {0ull, 0ull};
        unsigned char *const peaked =
            (ok && (step != 0u)) ? (unsigned char *)calloc((size_t)before->count + 1u, 1u) : NULL;
        unsigned char *const before_truthy[KNEE_LADDER_OUTPUTS] = {NULL, peaked};
        ok = ok && ((step == 0u) ||
                    ((peaked != NULL) &&
                     knee_rung_verdicts(before, (step == 1u) ? before : earliest, before->before, here, before->after,
                                        (step == 1u) ? 0ull : (before->gain - earliest->gain),
                                        here->gain - before->gain, &layout, before_counted, before_truthy, &error)));
        for (unsigned int body = 0u; ok && (step != 0u) && (body < before->count); body += 1u)
        {
            ok = (peaked[body] == 0u) || knee_kept_add(&kept, &before->magnitudes[(size_t)body * layout.limbs]);
        }
        free(peaked);
        if (!ok)
        {
            free(rose);
            ended = "a rung did not run";
            break;
        }
        free(last_rose);
        last_rose = rose;
        last = here;
        rung_words[(step * 5u) + 0u] = here->orders.order[0];
        rung_words[(step * 5u) + 1u] = here->orders.order[1];
        rung_words[(step * 5u) + 2u] = here->orders.order[2];
        rung_words[(step * 5u) + 3u] = (unsigned int)(here->gain & 0xFFFFFFFFull);
        rung_words[(step * 5u) + 4u] = (unsigned int)(here->gain >> 32u);
        made = step + 1u;
        open = own_counted[KNEE_LADDER_ROSE];
        ok = knee_text_add(text, "%s\t%u\t%u\t%u\t%u\t%u\t%llu\t%llu\t%llu\n", series->name, step,
                           here->orders.order[0], here->orders.order[1], here->orders.order[2], here->count,
                           own_counted[KNEE_LADDER_ROSE], before_counted[KNEE_LADDER_PEAKED],
                           knee_now_microseconds() - mark);
        if (open == 0ull)
        {
            break;
        }
    }
    const int released = knee_job_release("knee ladder", &job);
    // the bodies the last rung leaves rising are kept after the peaked ones
    const size_t peaked_count = kept.count;
    for (unsigned int body = 0u; ok && (last != NULL) && (body < last->count); body += 1u)
    {
        ok = (last_rose[body] == 0u) || knee_kept_add(&kept, &last->magnitudes[(size_t)body * layout.limbs]);
    }
    const int written =
        ok && ((bodies == NULL) || knee_ladder_write(bodies, series->name, &layout, rung_words, made, &kept,
                                                      peaked_count));
    for (unsigned int slot = 0u; slot < 3u; slot += 1u)
    {
        knee_rung_release(&rungs[slot]);
    }
    free(last_rose);
    free(kept.words);
    free(volume);
    if (!ok || !released || !written)
    {
        fprintf(stderr, "  %s: the ladder did not run: %s (kind %u, module %u, site %u)\n", series->name, ended,
                error.kind, error.module, error.site);
        return 0;
    }
    return knee_text_add(text, "%s\tend\t%u\t%s\t%llu\t%zu\n", series->name, made, ended, open, peaked_count);
}

// a ladder line starts with its series; a series is done where it has an end line
static int knee_take_ladder(const char *line, void *context)
{
    const char *const tab = strchr(line, '\t');
    if (tab == NULL)
    {
        return 0;
    }
    return (strncmp(tab + 1, "end\t", 4u) != 0) || knee_take_spacing(line, context);
}

// the ladder over a set: each series' P from the readings and its voxel from the spacing, one series' lines written
// together once its ladder ends, and its bodies' file in `--bodies` (knee_ladder_write), a directory that is there. A
// series with no reading or no spacing on an axis is reported and left out
//
//   knee_period ladder --daemon <tessera_daemon> --readings <periods.tsv> --spacing <spacing.tsv> --set <dir>
//                      --out <ladder.tsv> [--bodies <dir>] [--list <file>]
static int knee_ladder(int argc, char **argv)
{
    const char *readings = NULL;
    const char *spacing_path = NULL;
    const char *set = NULL;
    const char *out = NULL;
    const char *list = NULL;
    const char *bodies = NULL;
    for (int at = 1; (at + 1) < argc; at += 2)
    {
        const char *const value = argv[at + 1];
        s_daemon = (strcmp(argv[at], "--daemon") == 0) ? value : s_daemon;
        readings = (strcmp(argv[at], "--readings") == 0) ? value : readings;
        spacing_path = (strcmp(argv[at], "--spacing") == 0) ? value : spacing_path;
        set = (strcmp(argv[at], "--set") == 0) ? value : set;
        out = (strcmp(argv[at], "--out") == 0) ? value : out;
        list = (strcmp(argv[at], "--list") == 0) ? value : list;
        bodies = (strcmp(argv[at], "--bodies") == 0) ? value : bodies;
    }
    if ((s_daemon == NULL) || (readings == NULL) || (spacing_path == NULL) || (set == NULL) || (out == NULL))
    {
        fprintf(stderr, "usage: knee_period ladder --daemon <tessera_daemon> --readings <periods.tsv> --spacing "
                        "<spacing.tsv> --set <dir> --out <ladder.tsv> [--bodies <dir>] [--list <file>]\n");
        return 2;
    }
    unsigned long long period_count = 0ull;
    KneePeriod *const periods = knee_periods_read(readings, &period_count);
    unsigned long long spacing_count = 0ull;
    KneeSpacing *const spacings = knee_spacings_read(spacing_path, &spacing_count);
    if ((periods == NULL) || (spacings == NULL))
    {
        fprintf(stderr, "  %s or %s did not read\n", readings, spacing_path);
        return 1;
    }
    KneeText header = {NULL, 0u, 0u};
    KneeNames written = {NULL, 0ull, 0ull};
    int existed = 0;
    if (!knee_text_add(&header, "# series\trung\tz\ty\tx\tbodies\trose\tpeaked on the rung before\tmicroseconds\n") ||
        !knee_resume(out, &header, knee_take_ladder, &written, &existed))
    {
        return 1;
    }
    qsort(written.names, (size_t)written.count, sizeof(char *), knee_text_order);
    FILE *const file = fopen(out, "ab");
    if ((file == NULL) || (!existed && (fwrite(header.bytes, 1u, header.used, file) != header.used)))
    {
        fprintf(stderr, "  %s did not open for writing\n", out);
        return 1;
    }
    char **names = NULL;
    unsigned int count = 0u;
    if (list != NULL)
    {
        FILE *const named = fopen(list, "rb");
        char *line = NULL;
        while ((named != NULL) && ((line = knee_line(named)) != NULL))
        {
            char **const grown = (line[0] != '\0') ? (char **)realloc(names, ((size_t)count + 1u) * sizeof(char *))
                                                   : NULL;
            if (grown == NULL)
            {
                free(line);
                continue;
            }
            names = grown;
            names[count] = line;
            count += 1u;
        }
        if (named != NULL)
        {
            fclose(named);
        }
    }
    else
    {
        count = engine_set_samples(set, &names);
    }
    unsigned int ran = 0u;
    unsigned int left = 0u;
    unsigned int failed = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if ((written.count != 0ull) &&
            (bsearch(&names[at], written.names, (size_t)written.count, sizeof(char *), knee_text_order) != NULL))
        {
            continue;
        }
        KneePeriod period_key;
        period_key.name = names[at];
        const KneePeriod *const period =
            (const KneePeriod *)bsearch(&period_key, periods, (size_t)period_count, sizeof(KneePeriod),
                                        knee_period_order);
        KneeSpacing spacing_key;
        spacing_key.name = names[at];
        const KneeSpacing *const spacing =
            (const KneeSpacing *)bsearch(&spacing_key, spacings, (size_t)spacing_count, sizeof(KneeSpacing),
                                         knee_spacing_order);
        KneeSeries series;
        memset(&series, 0, sizeof(series));
        series.set = set;
        series.name = names[at];
        EngineError error;
        memset(&error, 0, sizeof(error));
        const int whole = (spacing != NULL) && (spacing->held[0] != 0u) && (spacing->held[1] != 0u) &&
                          (spacing->held[2] != 0u);
        if ((period == NULL) || !whole || (engine_iapx_head(set, names[at], series.extent, &error) != 0L) ||
            (series.extent[0] != 1ull))
        {
            left += 1u;
            printf("  left out  %s  (%s)\n", names[at],
                   (period == NULL) ? "no reading" : (!whole ? "a spacing not held" : "the head did not read"));
            continue;
        }
        KneeText text = {NULL, 0u, 0u};
        if (knee_ladder_series(&series, spacing, period->period, bodies, &text))
        {
            fwrite(text.bytes, 1u, text.used, file);
            fflush(file);
            ran += 1u;
            printf("%s", text.bytes);
        }
        else
        {
            failed += 1u;
        }
        free(text.bytes);
    }
    fclose(file);
    printf("  the ladder is done: %u series, %u run, %u left out, %u did not run\n", count, ran, left, failed);
    return (failed == 0u) ? 0 : 1;
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
    if ((argc > 1) && (strcmp(argv[1], "flatten") == 0))
    {
#if defined(_WIN32)
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
        return knee_flatten(argc - 1, argv + 1);
    }
    if ((argc > 1) && (strcmp(argv[1], "spacing") == 0))
    {
#if defined(_WIN32)
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
        return knee_spacing(argc - 1, argv + 1);
    }
    if ((argc > 1) && (strcmp(argv[1], "ladder") == 0))
    {
#if defined(_WIN32)
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
        return knee_ladder(argc - 1, argv + 1);
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
        unsigned short *const volume = knee_load(series, &root, NULL);
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
        unsigned short *const volume = knee_load(series, &root, NULL);
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
