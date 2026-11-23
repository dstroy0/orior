// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// chaitin_omega_declarations.h: the first half of what chaitin_omega_*.cu's pieces share (chaitin_omega_internal.h
// includes it)
#ifndef CHAITIN_OMEGA_DECLARATIONS_H
#define CHAITIN_OMEGA_DECLARATIONS_H
//
// Chaitin's halting probability for Tromp's binary lambda calculus, bracketed exactly.
//
// The machine reads a closed lambda term from its input, self-delimited in de Bruijn form. In that form 00 M is
// lambda M, 01 M N is M applied to N, and 1^k 0 is the variable bound k lambdas out. The machine halts where the term
// has a normal form. The codes are prefix free, and Omega = sum of 2^-|t| over the halting terms is a probability.
// Its first n bits settle the halting of every program of n bits or fewer. No machine thus computes them all: a run
// comes to Omega from below and never reaches it.
//
// The sim runs every closed term of at max L bits by normal order reduction, which reaches a normal form whenever one
// exists. A normal form reached proves the halt. A watcher stores a copy of the term at each power of two steps
// (Brent) and sees every bit of it. A term that comes back to a state it had never halts. In a bounded space a term
// can only halt or repeat, as the space contains finitely many terms.
//
// The exits of the runs are thus: halted, looped, out of steps (the space not yet searched), and outgrew the space.
// No bounded search closes that last exit. The sim closes a grower where it proves that the grower
// grows forever: a part of it returns at its own head (omega_grows_forever).
//
// The runs go to the device, one term to a thread, and the host runs the few that pass the device's small budgets
// (omega_device). The host's own run of every term through 30 bits must give the same fates and busy beavers.
// Arguments: L, steps, tokens, and "cpu" to run every term on the host instead.
//
// The sim counts, and does not run, to bound the mass of the closed terms longer than L. The mass a(n) of every term
// code of n bits and the mass c(n, k) of those closed under k lambdas follow recurrences. The sums are in fixed point,
// rounded toward the bound that each side needs. Every code parses to its end with probability 1. The parse is a
// subcritical branching process, and 1 is the smaller root of T^2 - 3T + 2 = 0. What the counted lengths leave of 1
// bounds every longer length at once.
//
//   Omega >= the halted mass,
//   Omega <= the halted mass + the open mass + the closed mass past L,
//
// both exact dyadics, and each bit the two share is a bit of Omega.
#include "../../../../../cu/engine/analysis/cycle/cycle.h"
#include "../../../../../cu/engine/analysis/key_schedule/key_schedule.h"
#include "../../../../../cu/engine/analysis/keymath/keymath.h"
#include "sim.h"

#include <cub/cub.cuh>

#define OMEGA_LENGTH_MAX 60u

#define OMEGA_LENGTH_DEFAULT 28u

#define OMEGA_STEPS_DEFAULT 2048u

#define OMEGA_TOKENS_DEFAULT 2048u

// the longest code whose mass the counting recurrences carry; the parse's tail bounds every longer code
#define OMEGA_COUNTED 1200u

// the brute-force parse checks the enumeration through this length
#define OMEGA_PARSED_MAX 22u

#define OMEGA_CHUNK 4096ULL

// fixed point: 62 fraction bits. A mass of 1 and its sums fit a word
#define OMEGA_POINT 62u

#define OMEGA_ONE (1ULL << OMEGA_POINT)

// tokens of a term in the order of its code: a lambda, an application, or a variable's index from 1
#define OMEGA_LAMBDA 0

#define OMEGA_APPLY (-1)

typedef enum
{
    OMEGA_HALTS = 0,
    OMEGA_LOOPS = 1,
    OMEGA_OPEN = 2,
    OMEGA_GREW = 3,
    OMEGA_DIVERGES = 4,
    OMEGA_TYPED = 5
} OmegaFate;

typedef struct
{
    unsigned int length;
    unsigned int steps;
    unsigned int tokens;
    unsigned long long count[OMEGA_LENGTH_MAX + 1u][OMEGA_LENGTH_MAX + 2u];
} OmegaCounts;

// Per length, the busy beavers among the halting terms. They are the max normal order steps to a normal form, and the
// largest normal form in bits (Tromp's BB lambda). Each comes with the first term in rank order that reaches it
typedef struct
{
    unsigned long long fate[6][OMEGA_LENGTH_MAX + 1u];
    unsigned long long contradictions;
    unsigned long long max_steps[OMEGA_LENGTH_MAX + 1u];
    unsigned long long steps_champion[OMEGA_LENGTH_MAX + 1u];
    unsigned long long max_bits[OMEGA_LENGTH_MAX + 1u];
    unsigned long long bits_champion[OMEGA_LENGTH_MAX + 1u];
} OmegaResults;

// A term: `count` tokens at `token`, in a table of `room` tokens. A token past the room ends the run with the reason
typedef struct
{
    int *token;
    size_t count;
    size_t room;
} OmegaTokens;

// the tokens of a term that the rewrite of a run with no token budget holds at once, at max
#define OMEGA_TERM_MAX (1u << 21u)

// The most nodes of the types of a program. A lambda and an application each make two, and a program of
// OMEGA_LENGTH_MAX bits holds at max half as many tokens
#define OMEGA_TYPES_MAX OMEGA_LENGTH_MAX

// Simple types by unification (Hindley): a type is a variable or an arrow, stored in a union-find. Every simply
// typable term normalizes strongly (Tait 1967). A type found is a certificate that the term halts, with
// no step run and no normal form written. It settles terms whose normal forms outgrow any space. The nodes and the
// types of the bound variables are tables at a fixed bound with their counts
typedef struct
{
    unsigned int parent[OMEGA_TYPES_MAX];
    unsigned int from[OMEGA_TYPES_MAX];
    unsigned int to[OMEGA_TYPES_MAX];
    unsigned char arrow[OMEGA_TYPES_MAX];
    unsigned int count;
    unsigned int bound[OMEGA_TYPES_MAX];
    unsigned int bound_count;
} OmegaTypes;

// The tables of one worker's rewrite: the term, its next step and the copy that Brent's watcher stores. Also the ends
// of the two spines, and the types. The tables of the terms are cells at the room of the run. Each worker holds its
// own from the start of its thread to its end
typedef struct
{
    OmegaTokens term;
    OmegaTokens next;
    OmegaTokens stored;
    size_t *ends;
    size_t *stored_ends;
    OmegaTypes types;
} OmegaRewrite;

// one row of the masses, a mass for each length counted
typedef struct
{
    unsigned long long value[OMEGA_COUNTED + 1u];
} OmegaMass;

void omega_refused(const char *why);

void *omega_taken(unsigned long long count, unsigned long long size, int ephemeral);

void omega_given(void *cell);

void omega_put(OmegaTokens *term, int token);

size_t omega_rewrite_room(unsigned int tokens);

void omega_rewrite_taken(OmegaRewrite *rewrite, size_t room, int ephemeral);

void omega_rewrite_given(OmegaRewrite *rewrite);

char *omega_text_read(const char *path, size_t *length);

size_t omega_text_lines(const char *text, size_t length);

int omega_text_line(const char *text, size_t length, size_t *at, char *line, size_t room);

unsigned long long omega_count(const OmegaCounts *counts, unsigned int length, unsigned int depth);

void omega_count_all(OmegaCounts *counts);

void omega_unrank(const OmegaCounts *counts, unsigned int length, unsigned int depth, unsigned long long index,
                  OmegaTokens *term);

size_t omega_end(const int *term, size_t at);

unsigned long long omega_code_bits(const OmegaTokens *term);

void omega_code_text(VerbaLinea *line, const OmegaTokens *term);

// the spine position of the head redex, where the term is an application spine over a lambda. Otherwise the
// step normal order takes is not a head step, and the answer is OMEGA_NOT_HEAD
#define OMEGA_NOT_HEAD 0xFFFFFFFFu

OmegaFate omega_run(OmegaRewrite *rewrite, unsigned int steps, unsigned int tokens, unsigned int *taken);

int omega_parse(unsigned long long code, unsigned int length, unsigned int *at, unsigned int depth);

void omega_all_mass(OmegaMass *mass, int up);

void omega_closed_mass(const OmegaMass *all_up, OmegaMass *closed);

void omega_normal_mass(OmegaMass *normal_closed);

void omega_results_open(OmegaResults *results);

void omega_champion(unsigned long long *max, unsigned long long *champion, unsigned long long value,
                    unsigned long long index);

void omega_settle(const OmegaCounts *counts, unsigned int length, unsigned long long index, OmegaFate fate,
                  unsigned int taken, OmegaRewrite *rewrite, OmegaResults *results);

void omega_merge(OmegaResults *into, const OmegaResults *one);

// the most workers the host runs at once
#define OMEGA_WORKERS_MAX 256u

unsigned int omega_workers(void);

void omega_host(const OmegaCounts *counts, unsigned int max, unsigned int workers, OmegaResults *fates);

// The same run on the device, one term to a thread, steered. The device takes the bulk under small budgets, and the
// host the few that need its large ones. Each thread unranks its term from the counts and reduces it in its own
// buffers. It takes every decision that the host's omega_run takes, in the same order, and the recursive walks become
// scans with an explicit stack. A run is deterministic and its budgets only stop it.
//
// A halt, a loop or a growth proof inside the small budgets comes at the same step inside the large ones. The device
// tallies a halt. The device passes every other term back by rank. A loop or growth proof comes back with its fate,
// for the type certificate. A run that hit a small budget or outgrew the device's room comes back for the host to
// run itself under the full budgets. No fate rests on the device's limits.
#define OMEGA_BLOCK 128u

// threads resident on a multiprocessor: each thread reads its buffers through the cache, and past this many they
// crowd one another out of it
#define OMEGA_THREADS_PER_SM 384u

// the device's budgets, where the full ones are larger
#define OMEGA_DEVICE_STEPS 256u

#define OMEGA_DEVICE_TOKENS 256u

// a passed back term's code for a run the host must make itself
#define OMEGA_HOST_RUNS 7u

// a stack frame of the scans: the children still to start in the low two bits, and whether it is a lambda
#define OMEGA_FRAME_LAMBDA 4u

#define OMEGA_STEP_NORMAL 0u

#define OMEGA_STEP_TAKEN 1u

#define OMEGA_STEP_OVERFLOW 2u

// the max terms one launch takes. An offset fits the 32 bits of a busy beaver key and 56 of a passed code
#define OMEGA_BATCH_MAX (1ULL << 24u)

// the sim checks the device's fates against the host's through this length
#define OMEGA_CROSS_MAX 30u

typedef struct
{
    unsigned long long next;
    unsigned long long halts;
    unsigned long long passed;
    // max steps and largest normal form, each the value over (2^32 - 1 - offset). The largest key is the
    // largest value at the lowest rank, and 0 is none
    unsigned long long steps_key;
    unsigned long long bits_key;
} OmegaBatch;

typedef struct
{
    short *term;
    short *next;
    short *stored;
    unsigned short *ends;
    unsigned short *stored_ends;
    unsigned char *stack;
} OmegaBuffers;

#include "chaitin_omega_device_terms.h"

typedef struct
{
    unsigned int steps;
    unsigned int tokens;
    unsigned int length;
    unsigned long long from;
    unsigned long long count;
    OmegaResults results;
} OmegaLedgerJob;

int omega_device(SimResults *results, const OmegaCounts *counts, unsigned int workers, const char *ledger,
                 unsigned int *threads_used, unsigned long long *host_runs, unsigned long long *jobs_run,
                 unsigned long long *jobs_retained, OmegaResults *fates);

// The run by the engine. Every normal order step of every live term is one sweep of the engine's record machine, and
// nothing bounds it but the machine. A term is as many records as it has tokens. It grows as it adds records, and no
// register or field contains a whole term. The live terms stay on the device from admission to their fate.
//
// Each round, a thread a term finds its leftmost redex and arranges where every token of the reduct comes from. The
// prefix and the suffix are copies, and the body's tokens come with the depth they sit at. A copy of the argument
// comes for each variable that the redex's lambda binds. It comes with the depth it goes under and the lambdas above
// each of its tokens.
//
// The record machine gathers each source token through its index list and computes the reduct's token. A body
// variable bound past the redex moves in by one, and an argument's variable bound outside it moves out by the depth.
// The rest are copies. Keymath encodes the record program for the widths that the round's values need, and no code
// sets a width.
//
// A thread a term then takes the same decisions as omega_run, in the same order. A normal form halts, Brent's watcher
// proves a loop, and omega_grows_forever makes its proof on the new term. Only a settled term leaves the device, as
// one record. There is no step budget and no token budget: a term runs until one of those settles it.
//
// The sim parks a term that the device's memory cannot fit for its next round as outgrown, the only exit left open.
// A stop leaves every live term open. The run is one tessera job.

// a job: one length's terms from `from`, at max this many, admitted to the live pool as the pool drains
#define OMEGA_ENGINE_JOB (1ULL << 22u)

// the pool takes the next job once fewer terms than this are live
#define OMEGA_ENGINE_REFILL (1ULL << 21u)

// a sweep of the record machine is at max this many lanes; the lane's plan index restarts at each sweep
#define OMEGA_ENGINE_SWEEP (1ULL << 26u)

#endif
