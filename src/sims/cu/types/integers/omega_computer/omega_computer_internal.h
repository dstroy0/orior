// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// What the omega_computer_*.cu pieces share: its includes, types and the functions one piece calls in another
#ifndef OMEGA_COMPUTER_INTERNAL_H
#define OMEGA_COMPUTER_INTERNAL_H

// The omega computer: Omega inside Omega inside Omega.
//
// The machine is Tromp's binary lambda calculus, run on the empty input. It reads a closed term M, self-delimited in
// de Bruijn form. In that form 00 M is lambda M, 01 M N is M applied to N, and 1^k 0 is the variable bound k lambdas
// out. The machine applies M to the empty list, and it halts where M nil has a normal form. Omega_nil is the sum of
// 2^-|M| over the M that halt. The sim brackets it between two exact dyadics, as chaitin_omega brackets its Omega.
//
// Tromp's universal machine U is itself such a term, of 190 bits. The source is Tromp, the AIT repository,
// ait/uni.lam, with the improvements by 50_ft_lock and Sean Palmer. The bits are those that the size optimizer of
// Tromp's `blc blc` gives. The file uni.lam gives the text of uni and its size, 190 bits, and not its bits.
//
// A translation of that text that only inlines its lets, or puts them out as redexes, comes to 209 bits or more. The
// optimizer also reduces. Tromp's test below checks the bits here, and no published string does.
//
// Given the bits of a closed M and then the rest of its input, U reduces to M applied to the rest. A machine made of
// the machine is thus U that reads code(M). The nest d deep is U that reads d - 1 copies of its own code, then
// code(M).
//
//   depth 0: M nil,  depth d: U (code(U)^(d-1) code(M) nil)
//
// Every depth is beta-equal to M nil. By Church and Rosser each has the normal form of M nil or none: one Omega, run
// d machines deep.
//
// The nested runs go to a lazy machine. It calls by need, and it reads back under every lambda and into every
// argument of a variable. It reaches a normal form wherever one exists. The direct run goes to it and to the rewrite
// of chaitin_omega, and the two must agree.
//
// 1. U is 190 bits and closed, and Tromp's own test passes at every depth. U reads delimit (326 bits,
//    ait/delimit.lam) and then 1111000111001, and it gives the list 11010.
// 2. The direct run: every closed M through L bits, run on nil by the rewrite in normal order. Brent's watcher and
//    the growth proof of chaitin_omega decide the runs that never halt. The lazy machine gives the normal form of the
//    rewrite wherever both halt. No run of either halts where the other proves it never does.
// 3. At every depth, every M whose nested run halts has the normal form of the direct run, token for token. None
//    halts where a run at any depth proves it never does.
// 4. The bracket of each depth contains the one outside it. The outer machine finished every M that the inner machine
//    finishes. By Church and Rosser, a non-halt proven at any depth stays valid at every depth.
//
// The sim measures the bracket of each depth and the bits of Omega_nil that it proves. It also measures the steps of
// each depth against the one outside it.

#include "sim.h"

// the longest program read when the request names none, and the max it may name
#define OMEGA_COMPUTER_LENGTH_DEFAULT 20u

#define OMEGA_COMPUTER_LENGTH_MAX 40u

// the machines deep when the request names none, and the max it may name
#define OMEGA_COMPUTER_DEPTH_DEFAULT 3u

#define OMEGA_COMPUTER_DEPTH_MAX 8u

// the rewriting's budgets a run, chaitin_omega's defaults
#define OMEGA_COMPUTER_REWRITE_STEPS 2048u

#define OMEGA_COMPUTER_REWRITE_TOKENS 2048u

// the lazy machine's beta steps a run when the request names none
#define OMEGA_COMPUTER_STEPS_DEFAULT (1ULL << 25u)

// The lazy machine's beta steps a run for a program that the rewrite proves never halts. By Church and Rosser it halts
// at no depth, and the run only checks that it does not
#define OMEGA_COMPUTER_NEVER_STEPS (1ULL << 16u)

// The lazy machine's cells a run: thunks, environment cells and spine cells together. Each arena is one cell of the
// pool at this count, and a run that needs more outgrows the machine
#define OMEGA_COMPUTER_CELLS_MAX (1ULL << 25u)

// the cells that a run can take past its check of the cells left: the root, and the three cells of one readback
#define OMEGA_COMPUTER_CELLS_SPARE 4ULL

// the max tokens of a normal form that the lazy machine reads back
#define OMEGA_COMPUTER_TOKENS_MAX 65536u

// the room of a normal form that the lazy machine reads back: the max, and one spine past it
#define OMEGA_COMPUTER_NORMAL_MAX (2u * OMEGA_COMPUTER_TOKENS_MAX)

// the tokens of a term that the rewrite holds at once, at max. A step past it ends the run with the reason
#define OMEGA_COMPUTER_TERM_MAX (1u << 22u)

// the tokens of a program, its input or U that the sim builds, at max
#define OMEGA_COMPUTER_PROGRAM_MAX 16384u

// the bits that one run of the lazy machine reads, at max: U for each depth, the program's code and its input
#define OMEGA_COMPUTER_BITS_MAX 8192u

// the lazy machine's term nodes at max: the fixed nodes, the program, and three nodes for each bit of the input
#define OMEGA_COMPUTER_NODES_MAX (3u * OMEGA_COMPUTER_BITS_MAX + OMEGA_COMPUTER_PROGRAM_MAX)

// tokens of a term in the order of its code: a lambda, an application, or a variable's index from 1
#define OMEGA_COMPUTER_LAMBDA 0

#define OMEGA_COMPUTER_APPLY (-1)

// the lazy machine's term nodes
#define OMEGA_COMPUTER_NODE_VARIABLE 0

#define OMEGA_COMPUTER_NODE_LAMBDA 1

#define OMEGA_COMPUTER_NODE_APPLY 2

// a thunk's states: its term not yet evaluated, entered and under evaluation, or with its value stored
#define OMEGA_COMPUTER_UNEVALUATED 0

#define OMEGA_COMPUTER_ENTERED 1

#define OMEGA_COMPUTER_EVALUATED 2

typedef enum
{
    OMEGA_COMPUTER_HALTS = 0,
    OMEGA_COMPUTER_LOOPS = 1,
    OMEGA_COMPUTER_OPEN = 2,
    OMEGA_COMPUTER_GREW = 3,
    OMEGA_COMPUTER_DIVERGES = 4
} OmegaComputerFate;

// A term: `count` tokens at `token`, in a table of `room` tokens. A token past the room ends the run with the reason
typedef struct
{
    int *token;
    size_t count;
    size_t room;
} OmegaComputerTerm;

// A text of bits, '0' and '1': `count` bits at `bit`, in a table of `room` bytes. A bit past the room ends the run
// with the reason
typedef struct
{
    char *bit;
    size_t count;
    size_t room;
} OmegaComputerBits;

typedef struct
{
    int type;
    // the body, the function, or the variable's index from 1
    int left;
    // the argument
    int right;
} OmegaComputerNode;

typedef struct
{
    int term;
    // the environment's first cell, -1 where empty
    int environment;
    int state;
    // 1 where the value is a variable applied to a spine, 0 where it is a lambda
    int neutral;
    // the lambda's node, or the variable's level
    int value;
    // the lambda's environment, or the spine's last cell
    int value_cells;
} OmegaComputerThunk;

// a cell of an environment or a spine: a thunk and the next cell, -1 at the end
typedef struct
{
    int thunk;
    int next;
} OmegaComputerCell;

typedef struct
{
    int thunk;
    int depth;
} OmegaComputerWork;

// The lazy machine. Its nodes are a table at a fixed bound. Its arenas, its stack, its work and its arguments are
// each one cell of the persistent tier at the cap of the cells. The one machine runs every program of the sim
typedef struct
{
    OmegaComputerNode nodes[OMEGA_COMPUTER_NODES_MAX];
    size_t node_count;
    size_t fixed_nodes;
    int variable_node;
    int true_node;
    int false_node;
    int universal_node;
    OmegaComputerThunk *thunks;
    size_t thunk_count;
    OmegaComputerCell *environments;
    size_t environment_count;
    OmegaComputerCell *spines;
    size_t spine_count;
    int *stack;
    size_t stack_count;
    OmegaComputerWork *work;
    size_t work_count;
    int *arguments;
    size_t argument_count;
    unsigned long long steps;
    unsigned long long step_budget;
} OmegaComputerMachine;

typedef struct
{
    unsigned long long count[OMEGA_COMPUTER_LENGTH_MAX + 1u][OMEGA_COMPUTER_LENGTH_MAX + 2u];
} OmegaComputerCounts;

void omega_computer_refused(const char *why);

void *omega_computer_taken(unsigned long long count, unsigned long long size);

void omega_computer_given(void *cell);

void omega_computer_put(OmegaComputerTerm *term, int token);

int omega_computer_same(const OmegaComputerTerm *left, const OmegaComputerTerm *right);

void omega_computer_appended(OmegaComputerTerm *term, const int *from, size_t count);

void omega_computer_bits_put(OmegaComputerBits *bits, const char *text, size_t count);

unsigned long long omega_computer_count(const OmegaComputerCounts *counts, unsigned int length, unsigned int depth);

void omega_computer_count_all(OmegaComputerCounts *counts, unsigned int max);

void omega_computer_unrank(const OmegaComputerCounts *counts, unsigned int length, unsigned int depth,
                           unsigned long long index, OmegaComputerTerm *term);

int omega_computer_parse(const char *code, size_t length, size_t *at, int depth, OmegaComputerTerm *term);

void omega_computer_code(const OmegaComputerTerm *term, OmegaComputerBits *code);

void omega_computer_list_tokens(const char *bits, size_t count, OmegaComputerTerm *term);

int omega_computer_step(const OmegaComputerTerm *term, OmegaComputerTerm *next);

unsigned int omega_computer_spine(const OmegaComputerTerm *term, size_t *ends);

#define OMEGA_COMPUTER_NOT_HEAD 0xFFFFFFFFu

unsigned int omega_computer_head_redex(const OmegaComputerTerm *term);

int omega_computer_grows_forever(const OmegaComputerTerm *stored, const size_t *stored_ends, unsigned int stored_range,
                                 const OmegaComputerTerm *term, unsigned int least, size_t *ends);

extern size_t g_omega_computer_peak;

void omega_computer_rewrite_open(void);

void omega_computer_rewrite_close(void);

OmegaComputerFate omega_computer_rewrite(const OmegaComputerTerm *start, unsigned int steps, unsigned int tokens,
                                         unsigned long long *taken, OmegaComputerTerm *result);

void omega_computer_machine_open(OmegaComputerMachine *machine, const OmegaComputerTerm *universal);

void omega_computer_machine_close(OmegaComputerMachine *machine);

OmegaComputerFate omega_computer_run(OmegaComputerMachine *machine, const OmegaComputerTerm *program, const char *input,
                                     size_t input_count, unsigned int depth, const char *universal_code,
                                     size_t universal_count, OmegaComputerTerm *normal);

#endif
