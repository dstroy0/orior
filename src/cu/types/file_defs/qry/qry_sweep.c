// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_sweep.c: the host's own widths, alignments and steps for the run's buffer (qry_buffer.h), found by timing each
// candidate on the host with its own clock, and the ones found fastest written to qry_host.h beside qry_buffer.h, which
// every build of the buffer then takes (qry_sweep.sh).
//
//     qry_sweep time <scratch folder>    each candidate timed QRY_SWEEP_ROUNDS times, a line each to the standard
//                                        output: what was timed, the candidate, and the nanoseconds it took a MiB
//     qry_sweep pick <run .qry>...       the blobs `sweep` of every run given, each candidate's median taken over them,
//                                        and qry_host.h written to the standard output from the fastest, once at least
//                                        QRY_SWEEP_LEAST runs hold a sweep
//
// Each kind's first candidate is timed once and let go before any timing is kept, its pages and caches warmed, and
// each round of a kind starts one candidate further along than the round before, so that no candidate is always the
// first timed.
//
// What is timed:
//   align  blobs of a run's sizes copied into a buffer one after another, each starting on a boundary of the candidate;
//          the smallest within QRY_SWEEP_NEAR of the fastest is taken, a wider one only costing padding
//   word   an array summed in words of the candidate's bytes; the fastest is the host's word
//   commit memory a buffer reserves given to it a candidate's bytes a step, each page then touched; the fastest
//   write  a scratch file written whole, a candidate's bytes a write, and flushed to the disk; the fastest is each
//          write's most, and the smallest within QRY_SWEEP_SLICE_NEAR of it the slice a panic waits behind at most
//
// Exit 0 where it was done, 1 where it was not, 2 where the words are none of the above, 3 where a pick is given fewer
// sweeps than QRY_SWEEP_LEAST
#if !defined(_WIN32)
// the clocks, the anonymous maps and the file flush are POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#endif
#include "qry_buffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#endif

// how far past the fastest a candidate may be and still be taken where a smaller one is the better, in parts a thousand
#define QRY_SWEEP_NEAR 20u
#define QRY_SWEEP_SLICE_NEAR 50u

// the times each candidate is timed a sweep, and the sweeps a pick takes at least before it writes qry_host.h: one
// sweep's timings are the host as it was that minute
#define QRY_SWEEP_ROUNDS 3u
#define QRY_SWEEP_LEAST 3u

// the bytes each candidate is timed over
#define QRY_SWEEP_COPY_BYTES (512ull << 20u)
#define QRY_SWEEP_WORD_BYTES (256ull << 20u)
#define QRY_SWEEP_COMMIT_BYTES (1ull << 30u)
#define QRY_SWEEP_WRITE_BYTES (256ull << 20u)

// the candidates of each
static const unsigned long long s_aligns[] = {8ull, 16ull, 32ull, 64ull, 128ull, 256ull};
static const unsigned long long s_words[] = {4ull, 8ull};
static const unsigned long long s_commits[] = {1ull << 20u, 4ull << 20u, 16ull << 20u, 64ull << 20u, 256ull << 20u};
static const unsigned long long s_writes[] = {64ull << 10u, 256ull << 10u, 1ull << 20u,
                                              4ull << 20u,  16ull << 20u,  64ull << 20u};

// a run's blob sizes, taken round as the copies go
static const unsigned long long s_sizes[] = {48ull, 200ull, 1500ull, 9000ull, 64000ull, 333ull, 4096ull};

static unsigned long long qry_sweep_now(void)
{
#if defined(_WIN32)
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    // a performance counter and its frequency are positive
    const unsigned long long ticks = (unsigned long long)counter.QuadPart;
    // a performance counter and its frequency are positive
    const unsigned long long rate = (unsigned long long)frequency.QuadPart;
    return ((ticks / rate) * 1000000000ull) + (((ticks % rate) * 1000000000ull) / rate);
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    // a monotonic clock's seconds and nanoseconds are not negative
    return ((unsigned long long)now.tv_sec * 1000000000ull) + (unsigned long long)now.tv_nsec;
#endif
}

// the nanoseconds a MiB of `bytes` took from `began`
static unsigned long long qry_sweep_rate(unsigned long long began, unsigned long long bytes)
{
    const unsigned long long took = qry_sweep_now() - began;
    return (took * (1ull << 20u)) / ((bytes != 0ull) ? bytes : 1ull);
}

// what every timing works in: the scratch folder a file is written in, QRY_SWEEP_COPY_BYTES to copy from, and a
// quarter of that and 64 KiB to copy into
typedef struct
{
    const char *folder;
    const unsigned char *from;
    unsigned char *into;
} QrySweepScratch;

// the copies of a run's blobs, each starting on a boundary of `align`, into the scratch's room from its bytes
static unsigned long long qry_sweep_align(const QrySweepScratch *scratch, unsigned long long align)
{
    unsigned char *const into = scratch->into;
    const unsigned char *const from = scratch->from;
    const unsigned long long room = QRY_SWEEP_COPY_BYTES / 4ull;
    unsigned long long copied = 0ull;
    unsigned long long at = 0ull;
    const unsigned long long began = qry_sweep_now();
    for (size_t turn = 0u; copied < QRY_SWEEP_COPY_BYTES; turn += 1u)
    {
        const unsigned long long size = s_sizes[turn % (sizeof(s_sizes) / sizeof(s_sizes[0]))];
        if ((at + size) > room)
        {
            at = 0ull;
        }
        // a size of the run's is far below what a size_t counts
        memcpy(into + at, from, (size_t)size);
        copied += size;
        at = (at + size + align - 1ull) & ~(align - 1ull);
    }
    return qry_sweep_rate(began, copied);
}

// the scratch's bytes summed in words of `word` bytes; the sum is kept so the work is not taken away
static volatile unsigned long long s_kept;

static unsigned long long qry_sweep_word(const QrySweepScratch *scratch, unsigned long long word)
{
    const unsigned char *const array = scratch->from;
    const unsigned long long began = qry_sweep_now();
    unsigned long long sum = 0ull;
    if (word == 4ull)
    {
        const unsigned int *const words = (const unsigned int *)(const void *)array;
        for (unsigned long long at = 0ull; at < (QRY_SWEEP_WORD_BYTES / 4ull); at += 1ull)
        {
            sum += words[at];
        }
    }
    else
    {
        const unsigned long long *const words = (const unsigned long long *)(const void *)array;
        for (unsigned long long at = 0ull; at < (QRY_SWEEP_WORD_BYTES / 8ull); at += 1ull)
        {
            sum += words[at];
        }
    }
    s_kept = sum;
    return qry_sweep_rate(began, QRY_SWEEP_WORD_BYTES);
}

// memory reserved and given `step` bytes at a time, each page touched; 0 where the system has no such step
static unsigned long long qry_sweep_commit(const QrySweepScratch *scratch, unsigned long long step)
{
    (void)scratch;
#if defined(_WIN32)
    // the bytes timed are a whole count of steps, which a size_t counts
    unsigned char *const reserved =
        (unsigned char *)VirtualAlloc(NULL, (size_t)QRY_SWEEP_COMMIT_BYTES, MEM_RESERVE, PAGE_READWRITE);
    if (reserved == NULL)
    {
        return 0ull;
    }
    const unsigned long long began = qry_sweep_now();
    int whole = 1;
    for (unsigned long long at = 0ull; whole && (at < QRY_SWEEP_COMMIT_BYTES); at += step)
    {
        // a step is at most 256 MiB, which a size_t counts
        whole = (VirtualAlloc(reserved + at, (size_t)step, MEM_COMMIT, PAGE_READWRITE) != NULL);
        for (unsigned long long page = 0ull; whole && (page < step); page += 4096ull)
        {
            reserved[at + page] = 1u;
        }
    }
    const unsigned long long rate = qry_sweep_rate(began, QRY_SWEEP_COMMIT_BYTES);
    VirtualFree(reserved, 0u, MEM_RELEASE);
    return whole ? rate : 0ull;
#else
    (void)step;
    return 0ull;
#endif
}

// a scratch file in the scratch's folder written whole from its bytes, `chunk` bytes a write, and flushed to the disk,
// then let go; 0 where it was not written
static unsigned long long qry_sweep_write(const QrySweepScratch *scratch, unsigned long long chunk)
{
    const unsigned char *const bytes = scratch->from;
    char path[1100];
    snprintf(path, sizeof(path), "%s/qry_sweep_scratch.bin", scratch->folder);
    const unsigned long long began = qry_sweep_now();
    int whole = 1;
#if defined(_WIN32)
    HANDLE file = CreateFileA(path, GENERIC_WRITE, 0u, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    whole = (file != INVALID_HANDLE_VALUE);
    for (unsigned long long at = 0ull; whole && (at < QRY_SWEEP_WRITE_BYTES); at += chunk)
    {
        DWORD wrote = 0u;
        // a chunk is at most 64 MiB, which a DWORD counts
        whole = WriteFile(file, bytes + (at % QRY_SWEEP_COPY_BYTES), (DWORD)chunk, &wrote, NULL) && (wrote == chunk);
    }
    whole = whole && (FlushFileBuffers(file) != 0);
    if (file != INVALID_HANDLE_VALUE)
    {
        CloseHandle(file);
    }
#else
    const int file = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    whole = (file >= 0);
    for (unsigned long long at = 0ull; whole && (at < QRY_SWEEP_WRITE_BYTES); at += chunk)
    {
        // a chunk is at most 64 MiB, which a size_t and an ssize_t count
        whole = (write(file, bytes + (at % QRY_SWEEP_COPY_BYTES), (size_t)chunk) == (ssize_t)chunk);
    }
    whole = whole && (fsync(file) == 0);
    if (file >= 0)
    {
        close(file);
    }
#endif
    const unsigned long long rate = qry_sweep_rate(began, QRY_SWEEP_WRITE_BYTES);
    remove(path);
    return whole ? rate : 0ull;
}

// each kind timed: its word in a sweep's lines, its candidates, and its timing of one, 0 where the host has none
typedef struct
{
    const char *what;
    const unsigned long long *candidates;
    size_t count;
    unsigned long long (*timed)(const QrySweepScratch *scratch, unsigned long long candidate);
} QrySweepKind;

static const QrySweepKind s_kinds[] = {
    {"align", s_aligns, sizeof(s_aligns) / sizeof(s_aligns[0]), qry_sweep_align},
    {"word", s_words, sizeof(s_words) / sizeof(s_words[0]), qry_sweep_word},
    {"commit", s_commits, sizeof(s_commits) / sizeof(s_commits[0]), qry_sweep_commit},
    {"write", s_writes, sizeof(s_writes) / sizeof(s_writes[0]), qry_sweep_write},
};

static int qry_sweep_timed(const char *folder)
{
    unsigned char *const from = (unsigned char *)malloc((size_t)QRY_SWEEP_COPY_BYTES);
    unsigned char *const into = (unsigned char *)malloc((size_t)(QRY_SWEEP_COPY_BYTES / 4ull) + 65536u);
    if ((from == NULL) || (into == NULL))
    {
        free(from);
        free(into);
        fprintf(stderr, "qry_sweep: no memory to time with\n");
        return 0;
    }
    for (unsigned long long at = 0ull; at < QRY_SWEEP_COPY_BYTES; at += 1ull)
    {
        from[at] = (unsigned char)((at * 2654435761ull) >> 13u);
    }
    const QrySweepScratch scratch = {folder, from, into};
    for (size_t kind = 0u; kind < (sizeof(s_kinds) / sizeof(s_kinds[0])); kind += 1u)
    {
        const QrySweepKind *const each = &s_kinds[kind];
        (void)each->timed(&scratch, each->candidates[0]);
        for (size_t round = 0u; round < QRY_SWEEP_ROUNDS; round += 1u)
        {
            for (size_t step = 0u; step < each->count; step += 1u)
            {
                const unsigned long long candidate = each->candidates[(round + step) % each->count];
                const unsigned long long rate = each->timed(&scratch, candidate);
                if (rate != 0ull)
                {
                    printf("%s %llu %llu\n", each->what, candidate, rate);
                }
            }
        }
    }
    free(from);
    free(into);
    return 1;
}

// every timing of one candidate the sweeps gave, and how many
#define QRY_SWEEP_RUNS 256u
typedef struct
{
    char what[16];
    unsigned long long candidate;
    unsigned long long rate[QRY_SWEEP_RUNS];
    unsigned int count;
} QrySweepCandidate;

static QrySweepCandidate s_candidates[64];
static unsigned int s_candidate_count;

static void qry_sweep_kept(const char *what, unsigned long long candidate, unsigned long long rate)
{
    QrySweepCandidate *found = NULL;
    for (unsigned int at = 0u; (found == NULL) && (at < s_candidate_count); at += 1u)
    {
        found = ((strcmp(s_candidates[at].what, what) == 0) && (s_candidates[at].candidate == candidate))
                    ? &s_candidates[at]
                    : NULL;
    }
    if ((found == NULL) && (s_candidate_count < (sizeof(s_candidates) / sizeof(s_candidates[0]))))
    {
        found = &s_candidates[s_candidate_count];
        snprintf(found->what, sizeof(found->what), "%s", what);
        found->candidate = candidate;
        s_candidate_count += 1u;
    }
    if ((found != NULL) && (found->count < QRY_SWEEP_RUNS))
    {
        found->rate[found->count] = rate;
        found->count += 1u;
    }
}

static int qry_sweep_order(const void *left, const void *right)
{
    const unsigned long long one = *(const unsigned long long *)left;
    const unsigned long long two = *(const unsigned long long *)right;
    return (one < two) ? -1 : ((one > two) ? 1 : 0);
}

static unsigned long long qry_sweep_median(QrySweepCandidate *candidate)
{
    qsort(candidate->rate, candidate->count, sizeof(candidate->rate[0]), qry_sweep_order);
    return candidate->rate[candidate->count / 2u];
}

// the candidate of `what` with the least median, or where `within` is not 0, the smallest candidate within `within`
// parts a thousand of that least; 0 where no sweep timed it
static unsigned long long qry_sweep_taken(const char *what, unsigned int within)
{
    unsigned long long least = 0ull;
    unsigned long long fastest = 0ull;
    for (unsigned int at = 0u; at < s_candidate_count; at += 1u)
    {
        if ((strcmp(s_candidates[at].what, what) == 0) && (s_candidates[at].count != 0u))
        {
            const unsigned long long median = qry_sweep_median(&s_candidates[at]);
            if ((fastest == 0ull) || (median < least))
            {
                least = median;
                fastest = s_candidates[at].candidate;
            }
        }
    }
    if ((within == 0u) || (fastest == 0ull))
    {
        return fastest;
    }
    unsigned long long taken = fastest;
    for (unsigned int at = 0u; at < s_candidate_count; at += 1u)
    {
        if ((strcmp(s_candidates[at].what, what) == 0) && (s_candidates[at].count != 0u) &&
            (s_candidates[at].candidate < taken) &&
            ((s_candidates[at].rate[s_candidates[at].count / 2u] * 1000ull) <= (least * (1000ull + within))))
        {
            taken = s_candidates[at].candidate;
        }
    }
    return taken;
}

// qry_host.h written from the sweeps of the runs `runs`: the exit the program makes, 0 where it was written, 3 where
// too few runs hold a sweep
static int qry_sweep_picked(int count, char **runs)
{
    unsigned int read = 0u;
    for (int at = 0; at < count; at += 1)
    {
        unsigned char *bytes = NULL;
        unsigned long long size = 0ull;
        char error[512] = "";
        if (!qry_file_joined(runs[at], "sweep", &bytes, &size, error, sizeof(error)) || (size == 0ull))
        {
            free(bytes);
            continue;
        }
        read += 1u;
        // each line `<what> <candidate> <nanoseconds a MiB>`
        char line[128];
        unsigned long long next = 0ull;
        while (next < size)
        {
            const unsigned char *const end = (const unsigned char *)memchr(bytes + next, '\n', (size_t)(size - next));
            const size_t length = (end != NULL) ? (size_t)(end - (bytes + next)) : (size_t)(size - next);
            snprintf(line, sizeof(line), "%.*s", (int)length, (const char *)(bytes + next));
            next += (unsigned long long)length + 1ull;
            char what[16];
            unsigned long long candidate = 0ull;
            unsigned long long rate = 0ull;
            if (sscanf(line, "%15s %llu %llu", what, &candidate, &rate) == 3)
            {
                qry_sweep_kept(what, candidate, rate);
            }
        }
        free(bytes);
    }
    if (read < QRY_SWEEP_LEAST)
    {
        fprintf(stderr, "qry_sweep: %u of the runs given hold a sweep, and qry_host.h is written from %u\n", read,
                QRY_SWEEP_LEAST);
        return 3;
    }
    const unsigned long long align = qry_sweep_taken("align", QRY_SWEEP_NEAR);
    const unsigned long long word = qry_sweep_taken("word", 0u);
    const unsigned long long commit = qry_sweep_taken("commit", 0u);
    const unsigned long long write_call = qry_sweep_taken("write", 0u);
    const unsigned long long slice = qry_sweep_taken("write", QRY_SWEEP_SLICE_NEAR);
    printf("// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational\n");
    printf("// qry_host.h: this host's widths, alignments and steps for the run's buffer, the fastest of each "
           "candidate's\n");
    printf("// median over the host's sweeps (qry_sweep.c), written by qry_sweep.sh and read by qry_buffer.h\n");
    printf("#ifndef QRY_HOST_H\n#define QRY_HOST_H\n\n");
    if (align != 0ull)
    {
        printf("#define QRY_FRAME_ALIGN %lluull\n", align);
    }
    if (word != 0ull)
    {
        printf("#define QRY_HOST_WORD_BYTES %lluu\n", word);
    }
    if (commit != 0ull)
    {
        printf("#define QRY_COMMIT_BYTES %lluull\n", commit);
    }
    if (write_call != 0ull)
    {
        printf("#define QRY_WRITE_CALL_BYTES %lluull\n", write_call);
        printf("#define QRY_SLICE_BYTES %lluull\n", slice);
    }
    printf("\n#endif\n");
    return 0;
}

int main(int count, char **words)
{
    if ((count == 3) && (strcmp(words[1], "time") == 0))
    {
        return qry_sweep_timed(words[2]) ? 0 : 1;
    }
    if ((count >= 3) && (strcmp(words[1], "pick") == 0))
    {
        return qry_sweep_picked(count - 2, words + 2);
    }
    fprintf(stderr, "qry_sweep time <scratch folder> | pick <run .qry>...\n");
    return 2;
}
