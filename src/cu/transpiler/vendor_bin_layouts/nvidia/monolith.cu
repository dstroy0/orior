// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// monolith.cu: one program that contains every base primitive (precepts.h), each in its own block between two tags.
// NVIDIA's compiler builds it once, and its listing is the answer key. The instructions between tag n and tag n + 1
// are the NVIDIA form of precept n. The program compares the SASS of our compiler for the same precept against them.
// The plan builds a chain to a larger operator from these blocks.
//
// A tag is a performance-monitor event whose mask is the number of the precept plus one, written in the listing as
// PMTRIG. It changes no word, and the compiler leaves it in place. Every block loads its operands and stores its answer
// with volatile accesses, and a tag clobbers memory. Thus the compiler moves nothing that a block does across a tag.
// The answer of a precept follows precept_applied (precept_value.h): a shift takes its count from the low five bits of
// the right word.
//
//     `in[0]`   the left word.
//     `in[1]`   the right word.
//     `in[2]`   a flag, 0 on every run. JCC branches on it and ERR raises on it, and neither occurs where it is 0.
//     `out[n]`  the answer of precept n, PRECEPT_COUNT words.
//     `out[PRECEPT_COUNT]`, `out[PRECEPT_COUNT + 1]`  the clock of the part across the program, low word then high.
//
// The second entry, monolith_cost, reads what each precept costs. It runs a precept as a chain, in which every step
// takes the answer of the step before, MONOLITH_BLOCK steps to a turn and `turns` turns. It reads the clock of the part
// before and after. The longer the run, the nearer its mean is to what the part does.
//
// Then it runs every pair of precepts interleaved, as two chains apart from each other, one step of each in turn. A
// pair that costs about the dearer of the two alone is one that the part runs side by side. One that costs about both
// together is one that it runs one after the other. The words come in as the own parameters of the entry, and nothing
// between the two clock reads touches memory.
//
// A step is one PTX instruction under a predicate read from `flag`, which is 1 on every run. The compiler cannot know
// the predicate, and it folds no chain of the same instruction into fewer. A chain written plainly, even with an empty
// fence after each step, comes out of the assembler as one instruction to a turn. A predicated instruction costs what
// the bare one does.
//
// A step answers what precept_applied answers, and the runner checks the last word of every chain against the word of
// the host. NOP is no instruction, and MOV is a copy of a word onto itself, which the assembler drops. Both read what
// the turns cost with nothing in them.
//
//     `left`, `right`  the two words. `turns`  the turns that a section runs. `flag`  1, the predicate of every step.
//     `cycles[s]`  the clock count of section s, singles first in MONOLITH_COSTED order, then each pair i <= j.
//     `sink[s]`    the last word of section s, kept so that no chain is dead.
#include "monolith.h"

static_assert((PRECEPT_NOP == 0) && (PRECEPT_ERR == 1) && (PRECEPT_NOT == 2) &&
                  (PRECEPT_MUL == (int)MONOLITH_COSTED_COUNT),
              "monolith: the costed run is NOP, then NOT through MUL, with ERR alone left out");

// the tags are in the program where MONOLITH_TAGGED is 1, its default, and out of it where it is 0. The program
// compares the two builds for what the tags cost: cycles, and the instructions that the compiler puts apart across a
// tag
#ifndef MONOLITH_TAGGED
#define MONOLITH_TAGGED 1
#endif

// the tag before precept `precept_`
#if MONOLITH_TAGGED
#define MONOLITH_TAG(precept_) asm volatile("pmevent.mask %0;" ::"n"((precept_) + 1) : "memory")
#else
#define MONOLITH_TAG(precept_) ((void)0)
#endif

// the tag after the last precept
#define MONOLITH_END MONOLITH_TAG(PRECEPT_COUNT)

static __device__ __forceinline__ unsigned int monolith_read(const unsigned int *in, unsigned int place)
{
    return *(volatile const unsigned int *)&in[place];
}

static __device__ __forceinline__ void monolith_write(unsigned int *out, unsigned int place, unsigned int word)
{
    *(volatile unsigned int *)&out[place] = word;
}

// the clock of the part, read where it is. It clobbers memory, and the compiler moves no access across the read
static __device__ __forceinline__ unsigned long long monolith_clock(void)
{
    unsigned long long counted = 0ULL;
    asm volatile("mov.u64 %0, %%clock64;" : "=l"(counted)::"memory");
    return counted;
}

extern "C" __global__ void monolith(const unsigned int *in, unsigned int *out)
{
    const unsigned long long began = monolith_clock();

    MONOLITH_TAG(PRECEPT_NOP);
    monolith_write(out, PRECEPT_NOP, monolith_read(in, 0u));

    MONOLITH_TAG(PRECEPT_ERR);
    // raised only where the flag is not zero. The trap is in the program, and no run takes it
    if (monolith_read(in, 2u) != 0u)
    {
        asm volatile("trap;" ::: "memory");
    }
    monolith_write(out, PRECEPT_ERR, 0u);

    MONOLITH_TAG(PRECEPT_NOT);
    monolith_write(out, PRECEPT_NOT, ~monolith_read(in, 0u));

    MONOLITH_TAG(PRECEPT_BITAND);
    monolith_write(out, PRECEPT_BITAND, monolith_read(in, 0u) & monolith_read(in, 1u));

    MONOLITH_TAG(PRECEPT_BITOR);
    monolith_write(out, PRECEPT_BITOR, monolith_read(in, 0u) | monolith_read(in, 1u));

    MONOLITH_TAG(PRECEPT_BITXOR);
    monolith_write(out, PRECEPT_BITXOR, monolith_read(in, 0u) ^ monolith_read(in, 1u));

    MONOLITH_TAG(PRECEPT_NAND);
    monolith_write(out, PRECEPT_NAND, ~(monolith_read(in, 0u) & monolith_read(in, 1u)));

    MONOLITH_TAG(PRECEPT_NOR);
    monolith_write(out, PRECEPT_NOR, ~(monolith_read(in, 0u) | monolith_read(in, 1u)));

    MONOLITH_TAG(PRECEPT_MOV);
    monolith_write(out, PRECEPT_MOV, monolith_read(in, 0u));

    MONOLITH_TAG(PRECEPT_SHL);
    {
        const unsigned int left = monolith_read(in, 0u);
        const unsigned int count = monolith_read(in, 1u) & 31u;
        monolith_write(out, PRECEPT_SHL, (count == 0u) ? left : (left << count));
    }

    MONOLITH_TAG(PRECEPT_SHR);
    {
        const unsigned int left = monolith_read(in, 0u);
        const unsigned int count = monolith_read(in, 1u) & 31u;
        monolith_write(out, PRECEPT_SHR, (count == 0u) ? left : (left >> count));
    }

    MONOLITH_TAG(PRECEPT_ASR);
    {
        const unsigned int left = monolith_read(in, 0u);
        const unsigned int count = monolith_read(in, 1u) & 31u;
        // ASR: the word, read as signed, carries its sign in from the top. The bits do not change
        monolith_write(out, PRECEPT_ASR, (count == 0u) ? left : (unsigned int)((int)left >> count));
    }

    MONOLITH_TAG(PRECEPT_ROL);
    {
        const unsigned int left = monolith_read(in, 0u);
        const unsigned int count = monolith_read(in, 1u) & 31u;
        monolith_write(out, PRECEPT_ROL, (count == 0u) ? left : ((left << count) | (left >> (32u - count))));
    }

    MONOLITH_TAG(PRECEPT_ROR);
    {
        const unsigned int left = monolith_read(in, 0u);
        const unsigned int count = monolith_read(in, 1u) & 31u;
        monolith_write(out, PRECEPT_ROR, (count == 0u) ? left : ((left >> count) | (left << (32u - count))));
    }

    MONOLITH_TAG(PRECEPT_ADD);
    monolith_write(out, PRECEPT_ADD, monolith_read(in, 0u) + monolith_read(in, 1u));

    MONOLITH_TAG(PRECEPT_SUB);
    monolith_write(out, PRECEPT_SUB, monolith_read(in, 0u) - monolith_read(in, 1u));

    MONOLITH_TAG(PRECEPT_MUL);
    monolith_write(out, PRECEPT_MUL, monolith_read(in, 0u) * monolith_read(in, 1u));

    MONOLITH_TAG(PRECEPT_BRA);
    // An unconditional jump stays in the program only where a branch jumps around it. The arm that the flag takes ends
    // in a jump over the other. Each arm holds four stores, more than the compiler writes as predicated stores in place
    // of a branch. With the flag 0, the second arm runs, and its last store is the answer, 0
    if (monolith_read(in, 2u) != 0u)
    {
        monolith_write(out, PRECEPT_BRA, 1u);
        monolith_write(out, PRECEPT_BRA, 2u);
        monolith_write(out, PRECEPT_BRA, 3u);
        monolith_write(out, PRECEPT_BRA, 4u);
    }
    else
    {
        monolith_write(out, PRECEPT_BRA, 5u);
        monolith_write(out, PRECEPT_BRA, 6u);
        monolith_write(out, PRECEPT_BRA, 7u);
        monolith_write(out, PRECEPT_BRA, 0u);
    }

    MONOLITH_TAG(PRECEPT_JCC);
    // a jump taken on the flag over four stores, more than the compiler writes as predicated stores in place of a
    // branch. With the flag 0 the stores run and the last is the answer, the left word
    if (monolith_read(in, 2u) == 0u)
    {
        monolith_write(out, PRECEPT_JCC, 1u);
        monolith_write(out, PRECEPT_JCC, 2u);
        monolith_write(out, PRECEPT_JCC, 3u);
        monolith_write(out, PRECEPT_JCC, monolith_read(in, 0u));
    }

    MONOLITH_END;

    const unsigned long long spent = monolith_clock() - began;
    // the count split into its two words, the low word first
    monolith_write(out, PRECEPT_COUNT, (unsigned int)(spent & 0xffffffffULL));
    monolith_write(out, PRECEPT_COUNT + 1u, (unsigned int)(spent >> 32u));
}

// the instruction of a step, under the predicate that the flag names. `%0` is the word, `%1` the right word, `%2` the
// flag, and `%3` the shift count, the low five bits of the right word
#define MONOLITH_PREDICATED(instruction_)                                                                              \
    asm volatile("{\n\t.reg .pred taken;\n\tsetp.ne.u32 taken, %2, 0;\n\t" instruction_ "\n\t}"                        \
                 : "+r"(word)                                                                                          \
                 : "r"(right), "r"(flag), "r"(count))

// one step of precept `precept_` on `word`, with its answer left in `word`. `right` is the right word, and `count` its
// low five bits. The step masks the count of a shift before it puts it, as precept_applied masks it. A funnel shift of
// a word with itself is a rotate
template <unsigned int precept_>
static __device__ __forceinline__ void monolith_step(unsigned int &word, unsigned int right, unsigned int count,
                                                     unsigned int flag)
{
    switch (precept_)
    {
    case PRECEPT_NOT:
        MONOLITH_PREDICATED("@taken not.b32 %0, %0;");
        break;
    case PRECEPT_BITAND:
        MONOLITH_PREDICATED("@taken and.b32 %0, %0, %1;");
        break;
    case PRECEPT_BITOR:
        MONOLITH_PREDICATED("@taken or.b32 %0, %0, %1;");
        break;
    case PRECEPT_BITXOR:
        MONOLITH_PREDICATED("@taken xor.b32 %0, %0, %1;");
        break;
    case PRECEPT_NAND:
        MONOLITH_PREDICATED("@taken and.b32 %0, %0, %1;\n\t@taken not.b32 %0, %0;");
        break;
    case PRECEPT_NOR:
        MONOLITH_PREDICATED("@taken or.b32 %0, %0, %1;\n\t@taken not.b32 %0, %0;");
        break;
    case PRECEPT_MOV:
        MONOLITH_PREDICATED("@taken mov.b32 %0, %0;");
        break;
    case PRECEPT_SHL:
        MONOLITH_PREDICATED("@taken shl.b32 %0, %0, %3;");
        break;
    case PRECEPT_SHR:
        MONOLITH_PREDICATED("@taken shr.u32 %0, %0, %3;");
        break;
    case PRECEPT_ASR:
        MONOLITH_PREDICATED("@taken shr.s32 %0, %0, %3;");
        break;
    case PRECEPT_ROL:
        MONOLITH_PREDICATED("@taken shf.l.wrap.b32 %0, %0, %0, %3;");
        break;
    case PRECEPT_ROR:
        MONOLITH_PREDICATED("@taken shf.r.wrap.b32 %0, %0, %0, %3;");
        break;
    case PRECEPT_ADD:
        MONOLITH_PREDICATED("@taken add.u32 %0, %0, %1;");
        break;
    case PRECEPT_SUB:
        MONOLITH_PREDICATED("@taken sub.u32 %0, %0, %1;");
        break;
    case PRECEPT_MUL:
        MONOLITH_PREDICATED("@taken mul.lo.u32 %0, %0, %1;");
        break;
    default:
        // NOP is no instruction
        break;
    }
}

// what the program runs every costed section with. That is the two words, the turns, the flag of the step predicates,
// and where each section writes its count and last word
typedef struct
{
    unsigned int left;
    unsigned int right;
    unsigned int turns;
    unsigned int flag;
    unsigned long long *cycles;
    unsigned int *sink;
} MonolithRun;

static __device__ __forceinline__ void monolith_count(const MonolithRun &run, unsigned int section,
                                                      unsigned long long spent, unsigned int word)
{
    *(volatile unsigned long long *)&run.cycles[section] = spent;
    monolith_write(run.sink, section, word);
}

// costed place `place_` run alone: one chain, `turns` turns of MONOLITH_BLOCK steps
template <unsigned int place_> static __device__ __forceinline__ void monolith_single(const MonolithRun &run)
{
    const unsigned int right = run.right;
    const unsigned int count = right & 31u;
    unsigned int word = run.left;
    const unsigned long long began = monolith_clock();
    for (unsigned int turn = 0u; turn < run.turns; turn += 1u)
    {
#pragma unroll
        for (unsigned int step = 0u; step < MONOLITH_BLOCK; step += 1u)
        {
            monolith_step<MONOLITH_COSTED(place_)>(word, right, count, run.flag);
        }
    }
    const unsigned long long spent = monolith_clock() - began;
    monolith_count(run, place_, spent, word);
}

// costed places `first_` and `second_` interleaved: two chains apart from each other, a step of each in turn. The
// first chain begins at the left word and takes the right word, the second the other way about
template <unsigned int first_, unsigned int second_>
static __device__ __forceinline__ void monolith_pair(const MonolithRun &run)
{
    const unsigned int left = run.left;
    const unsigned int right = run.right;
    const unsigned int left_count = left & 31u;
    const unsigned int right_count = right & 31u;
    unsigned int one = left;
    unsigned int other = right;
    const unsigned long long began = monolith_clock();
    for (unsigned int turn = 0u; turn < run.turns; turn += 1u)
    {
#pragma unroll
        for (unsigned int step = 0u; step < MONOLITH_BLOCK; step += 1u)
        {
            monolith_step<MONOLITH_COSTED(first_)>(one, right, right_count, run.flag);
            monolith_step<MONOLITH_COSTED(second_)>(other, left, left_count, run.flag);
        }
    }
    const unsigned long long spent = monolith_clock() - began;
    monolith_count(run, MONOLITH_PAIR_SECTION(first_, second_), spent, one ^ other);
}

// every single from costed place `place_` on
template <unsigned int place_> struct MonolithSingles
{
    static __device__ __forceinline__ void run(const MonolithRun &run)
    {
        monolith_single<place_>(run);
        MonolithSingles<place_ + 1u>::run(run);
    }
};

template <> struct MonolithSingles<MONOLITH_COSTED_COUNT>
{
    static __device__ __forceinline__ void run(const MonolithRun &)
    {
    }
};

// every pair from (`first_`, `second_`) on, row by row, each i <= j once
template <unsigned int first_, unsigned int second_> struct MonolithPairs
{
    static __device__ __forceinline__ void run(const MonolithRun &run)
    {
        monolith_pair<first_, second_>(run);
        MonolithPairs<((second_ + 1u) < MONOLITH_COSTED_COUNT) ? first_ : (first_ + 1u),
                      ((second_ + 1u) < MONOLITH_COSTED_COUNT) ? (second_ + 1u) : (first_ + 1u)>::run(run);
    }
};

template <unsigned int second_> struct MonolithPairs<MONOLITH_COSTED_COUNT, second_>
{
    static __device__ __forceinline__ void run(const MonolithRun &)
    {
    }
};

extern "C" __global__ void monolith_cost(unsigned int left, unsigned int right, unsigned int turns, unsigned int flag,
                                         unsigned long long *cycles, unsigned int *sink)
{
    const MonolithRun run = {left, right, turns, flag, cycles, sink};
    MonolithSingles<0u>::run(run);
    MonolithPairs<0u, 0u>::run(run);
}
