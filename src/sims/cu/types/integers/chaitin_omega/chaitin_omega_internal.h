// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// What the chaitin_omega_*.cu pieces share: its includes, types and the functions one piece calls in another
#ifndef CHAITIN_OMEGA_INTERNAL_H
#define CHAITIN_OMEGA_INTERNAL_H

#include "chaitin_omega_declarations.h"

// the engine's index is 32 bits. Every token a round reads must sit below this in the pool
#define OMEGA_ENGINE_INDEX_MAX 0xFFFFFFFFULL

#define OMEGA_ENGINE_NONE (~0ULL)

// the sim checks the terms settled on the engine against omega_run through this length
#define OMEGA_ENGINE_CROSS_MAX 24u

typedef long long OmegaToken;

// One live term in the device's pool: where its tokens and its watcher's copy sit, and this round's survey.
typedef struct
{
    unsigned long long index;
    unsigned long long steps;
    unsigned long long power;
    unsigned long long since;
    unsigned long long least;
    unsigned long long base;
    unsigned long long size;
    unsigned long long stored_base;
    unsigned long long stored_size;
    unsigned long long stored_range;
    unsigned long long ends_base;
    // this round: the head redex's spine position, the leftmost redex, its argument and what follows it
    unsigned long long head;
    unsigned long long redex;
    unsigned long long argument;
    unsigned long long after;
    unsigned long long out_size;
    unsigned long long out_base;
    // settled: a halt's normal form in bits
    unsigned long long bits;
    unsigned int length;
    unsigned int job;
    int fate;
    // the watcher takes the reduct as its new copy
    unsigned int stored_again;
} OmegaTerm;

// what a round's threads find across the pool
typedef struct
{
    unsigned long long max_token;
    unsigned long long max_depth;
    unsigned long long max_bound;
    unsigned long long max_tokens;
    unsigned long long settled;
    unsigned int unread;
} OmegaRoundTotals;

// the round's widths, from which keymath encodes the reduct program and the scheduler arranges the records
typedef struct
{
    unsigned int token_bits;
    unsigned int depth_bits;
    unsigned int bound_bits;
    unsigned int token_limbs;
    unsigned int plan_limbs;
} OmegaRoundWidths;

typedef struct
{
    unsigned int length;
    unsigned int job;
    unsigned long long index;
    int fate;
    unsigned long long steps;
    unsigned long long bits;
} OmegaSettled;

// The terms that the engine settled through OMEGA_ENGINE_CROSS_MAX bits, for the check against omega_run. They are one
// cell of the persistent tier at the count of the closed terms through that length, with their count. `owed` counts
// the terms through that length in the jobs that the run takes on. A job from the ledger adds none
typedef struct
{
    OmegaSettled *settled;
    size_t count;
    size_t room;
    unsigned long long owed;
} OmegaCrossed;

#include "chaitin_omega_pool_terms.h"

int omega_program_load(OmegaProgram *program, EngineError *error);

void omega_program_free(OmegaProgram *program);

__global__ void omega_pool_survey(OmegaTerm *terms, unsigned long long live, const OmegaToken *tokens,
                                  unsigned char *frames, unsigned long long *out_sizes, OmegaRoundTotals *totals);

__global__ void omega_pool_pack(const OmegaToken *tokens, unsigned long long count, OmegaRoundWidths widths,
                                unsigned int *packed);

__global__ void omega_pool_place(const OmegaTerm *terms, unsigned long long live, const OmegaToken *tokens,
                                 const unsigned long long *out_bases, unsigned char *frames,
                                 unsigned char *inner_frames, OmegaRoundWidths widths, unsigned int *plans,
                                 unsigned int *index);

__global__ void omega_pool_unpack(const unsigned int *out, unsigned long long lanes, unsigned int out_limbs,
                                  unsigned int offset, unsigned int bits, OmegaToken *reducts,
                                  OmegaRoundTotals *totals);

__global__ void omega_pool_watch(OmegaTerm *terms, unsigned long long live, const OmegaToken *reducts,
                                 const unsigned long long *out_bases, const OmegaToken *stored,
                                 const unsigned long long *stored_ends, unsigned long long *ends);

__global__ void omega_pool_settled(const OmegaTerm *terms, unsigned long long live, OmegaSettled *settled,
                                   OmegaRoundTotals *totals);

__global__ void omega_pool_retained_sizes(const OmegaTerm *terms, unsigned long long live, unsigned long long *places,
                                          unsigned long long *token_sizes, unsigned long long *stored_sizes,
                                          unsigned long long *end_sizes);

__global__ void omega_pool_retain(const OmegaTerm *terms, unsigned long long live, const OmegaToken *reducts,
                                  const OmegaToken *stored, const unsigned long long *stored_ends,
                                  const unsigned long long *places, const unsigned long long *token_bases,
                                  const unsigned long long *stored_bases, const unsigned long long *end_bases,
                                  OmegaTerm *retained, OmegaToken *retained_tokens, OmegaToken *retained_stored,
                                  unsigned long long *retained_ends);

__global__ void omega_pool_insert_sizes(const unsigned long long *counts, unsigned int length, unsigned long long from,
                                        unsigned long long count, unsigned long long *sizes);

__global__ void omega_pool_insert(const unsigned long long *counts, unsigned int length, unsigned int job,
                                  unsigned long long from, unsigned long long count, const unsigned long long *offsets,
                                  unsigned long long live, unsigned long long token_end, unsigned long long stored_end,
                                  unsigned long long ends_end, OmegaTerm *terms, OmegaToken *tokens, OmegaToken *stored,
                                  unsigned long long *stored_ends);

typedef struct
{
    unsigned int length;
    unsigned long long from;
    unsigned long long count;
    unsigned long long settled;
    OmegaResults results;
} OmegaEngineJob;

size_t omega_engine_ledger_read(const char *path, OmegaEngineJob **jobs);

int omega_engine_ledger_write(const char *path, const OmegaEngineJob *job);

// the engine's run of every closed term through L bits
typedef struct
{
    unsigned long long rounds;
    unsigned long long sweeps;
    unsigned long long records;
    unsigned long long max_steps;
    size_t max_tokens;
    unsigned long long jobs_run;
    unsigned long long jobs_retained;
    unsigned long long parked;
} OmegaEngineReport;

void omega_engine_settle(const OmegaCounts *counts, OmegaEngineJob *job, const OmegaSettled *settled,
                         OmegaRewrite *rewrite, OmegaCrossed *crossed);

// A device buffer that only grows. 0 where the device cannot allocate `bytes`. The call then clears the failed
// allocation's error and leaves the buffer as it was. -1 where a copy into the larger buffer fails. `preserve` carries
// the buffer's bytes over.
typedef struct
{
    void *data;
    size_t bytes;
} OmegaBuffer;

int omega_buffer_reserve(OmegaBuffer *buffer, size_t bytes, int preserve);

void omega_buffer_release(OmegaBuffer *buffer);

void omega_buffer_swap(OmegaBuffer *one, OmegaBuffer *other);

int omega_buffer_scan(SimResults *results, OmegaBuffer *scratch, const unsigned long long *values,
                      unsigned long long *sums, unsigned long long count, unsigned long long *total);

// the pool's buffers; each round moves its retained terms from the one of a pair into the other
typedef enum
{
    OMEGA_BUFFER_TERMS = 0,
    OMEGA_BUFFER_RETAINED_TERMS,
    OMEGA_BUFFER_TOKENS,
    OMEGA_BUFFER_RETAINED_TOKENS,
    OMEGA_BUFFER_STORED,
    OMEGA_BUFFER_RETAINED_STORED,
    OMEGA_BUFFER_ENDS,
    OMEGA_BUFFER_RETAINED_ENDS,
    OMEGA_BUFFER_FRAMES,
    OMEGA_BUFFER_INNER_FRAMES,
    OMEGA_BUFFER_OUT_SIZES,
    OMEGA_BUFFER_OUT_BASES,
    OMEGA_BUFFER_PLACES,
    OMEGA_BUFFER_PLACE_BASES,
    OMEGA_BUFFER_TOKEN_SIZES,
    OMEGA_BUFFER_TOKEN_BASES,
    OMEGA_BUFFER_STORED_SIZES,
    OMEGA_BUFFER_STORED_BASES,
    OMEGA_BUFFER_END_SIZES,
    OMEGA_BUFFER_END_BASES,
    OMEGA_BUFFER_PACKED,
    OMEGA_BUFFER_PLANS,
    OMEGA_BUFFER_INDEX,
    OMEGA_BUFFER_OUT,
    OMEGA_BUFFER_REDUCTS,
    OMEGA_BUFFER_REDUCT_ENDS,
    OMEGA_BUFFER_SETTLED,
    OMEGA_BUFFER_SCAN,
    OMEGA_BUFFER_TOTALS,
    OMEGA_BUFFER_COUNTS,
    OMEGA_BUFFERS
} OmegaBufferName;

#define OMEGA_BUFFER(buffers_, name_, type_) ((type_ *)(buffers_)[(name_)].data)

// how much of the pool's buffers the live terms fill
typedef struct
{
    unsigned long long live;
    unsigned long long tokens;
    unsigned long long stored;
    unsigned long long ends;
} OmegaPoolExtent;

#define OMEGA_POOL_BLOCK 256u

#define OMEGA_POOL_GRID_MAX 65536ULL

unsigned int omega_pool_grid(unsigned long long count);

int omega_pool_insert_job(SimResults *results, OmegaBuffer *buffers, const OmegaEngineJob *job, unsigned int job_at,
                          OmegaPoolExtent *extent);

unsigned long long omega_engine_declared(const OmegaCounts *counts);

int omega_engine(SimResults *results, const OmegaCounts *counts, const char *ledger, OmegaEngineReport *report,
                 OmegaCrossed *crossed, OmegaResults *fates);

#endif
