// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// vector.c: the paths this machine runs, each loop sent down the path given or the widest, and the plain loops
#include "vector.h"

unsigned int cli_vector_paths(const CliVectorCfg *args)
{
    unsigned int *paths = args->paths;
    unsigned int count = 0u;
    paths[count] = CLI_VECTOR_PLAIN;
    count += 1u;
#if ENGINE_TARGET_X86_64
    paths[count] = CLI_VECTOR_SSE2;
    count += 1u;
    if (cli_vector_avx2_present())
    {
        paths[count] = CLI_VECTOR_AVX2;
        count += 1u;
    }
#endif
#if ENGINE_TARGET_AARCH64
    paths[count] = CLI_VECTOR_NEON;
    count += 1u;
#endif
    return count;
}

// the widest path, read once: every thread that reads it before it is stored reads the same answer and stores it
#if defined(_MSC_VER) && !defined(__clang__)
static volatile int s_best = -1;
#else
static int s_best = -1;
#endif

unsigned int cli_vector_best(void)
{
    if (s_best < 0)
    {
        unsigned int paths[4];
        const unsigned int count = EMBED_CALL(cli_vector.paths, CliVectorCfg, .paths = paths);
        // a path is one of four, which an int represents
        s_best = (int)paths[count - 1u];
    }
    // read back as the path it was stored as
    return (unsigned int)s_best;
}

unsigned short cli_vector_fold(const CliVectorUnitCfg *args)
{
    if ((args->unit >= 0x41u) && (args->unit <= 0x5au))
    {
        // a capital and its small letter differ in the one bit 0x20
        return (unsigned short)(args->unit | 0x20u);
    }
    return (args->unit == CLI_LONG_S)   ? (unsigned short)'s'
           : (args->unit == CLI_KELVIN) ? (unsigned short)'k'
                                        : args->unit;
}

int cli_vector_exists_at(const CliVectorTextNeedleCfg *args)
{
    for (unsigned long long step = 0ull; step < args->needle_length; step += 1ull)
    {
        const unsigned short here =
            args->folded ? EMBED_CALL(cli_vector_unit.fold, CliVectorUnitCfg, .unit = args->text[args->at + step])
                         : args->text[args->at + step];
        const unsigned short required =
            args->folded ? EMBED_CALL(cli_vector_unit.fold, CliVectorUnitCfg, .unit = args->needle[step])
                         : args->needle[step];
        if (here != required)
        {
            return 0;
        }
    }
    return 1;
}

unsigned long long cli_vector_plain_find(const CliVectorTextLengthCfg *args)
{
    if (args->needle_length > args->length)
    {
        return CLI_VECTOR_NONE;
    }
    for (unsigned long long at = args->start; at <= (args->length - args->needle_length); at += 1ull)
    {
        if (EMBED_CALL(cli_vector_text_needle.exists_at, CliVectorTextNeedleCfg, .text = args->text,
                       .needle = args->needle, .needle_length = args->needle_length, .at = at, .folded = args->folded))
        {
            return at;
        }
    }
    return CLI_VECTOR_NONE;
}

unsigned long long cli_vector_common_on(const CliVectorLengthCfg *args)
{
#if ENGINE_TARGET_X86_64
    if (args->path == CLI_VECTOR_SSE2)
    {
        return cli_vector_length_x86.common_sse2(args);
    }
    if ((args->path == CLI_VECTOR_AVX2) && cli_vector_avx2_present())
    {
        return cli_vector_length_x86.common_avx2(args);
    }
#endif
#if ENGINE_TARGET_AARCH64
    if (args->path == CLI_VECTOR_NEON)
    {
        return cli_vector_length_neon.common_neon(args);
    }
#endif
    unsigned long long at = 0ull;
    while ((at < args->length) && (args->a_u64s[at] == args->b_u64s[at]))
    {
        at += 1ull;
    }
    return at;
}

unsigned long long cli_vector_find_unit_on(const CliVectorUnitCfg *args)
{
#if ENGINE_TARGET_X86_64
    if (args->path == CLI_VECTOR_SSE2)
    {
        return cli_vector_unit_x86.find_unit_sse2(args);
    }
    if ((args->path == CLI_VECTOR_AVX2) && cli_vector_avx2_present())
    {
        return cli_vector_unit_x86.find_unit_avx2(args);
    }
#endif
#if ENGINE_TARGET_AARCH64
    if (args->path == CLI_VECTOR_NEON)
    {
        return cli_vector_unit_neon.find_unit_neon(args);
    }
#endif
    for (unsigned long long at = 0ull; at < args->length; at += 1ull)
    {
        if (args->text[at] == args->unit)
        {
            return at;
        }
    }
    return CLI_VECTOR_NONE;
}

int cli_vector_same_on(const CliVectorACfg *args)
{
    if (args->a_length != args->b_length)
    {
        return 0;
    }
#if ENGINE_TARGET_X86_64
    if (args->path == CLI_VECTOR_SSE2)
    {
        return EMBED_CALL(cli_vector_length_a_x86.same_sse2, CliVectorLengthACfg, .a = args->a, .b = args->b,
                          .length = args->a_length);
    }
    if ((args->path == CLI_VECTOR_AVX2) && cli_vector_avx2_present())
    {
        return EMBED_CALL(cli_vector_length_a_x86.same_avx2, CliVectorLengthACfg, .a = args->a, .b = args->b,
                          .length = args->a_length);
    }
#endif
#if ENGINE_TARGET_AARCH64
    if (args->path == CLI_VECTOR_NEON)
    {
        return EMBED_CALL(cli_vector_length_a_neon.same_neon, CliVectorLengthACfg, .a = args->a, .b = args->b,
                          .length = args->a_length);
    }
#endif
    return memor.cmp(&(MemoriaCfg){.src = args->a, .other = args->b, .bytes = args->a_length * sizeof(args->a[0])}) ==
           0;
}

// Each place both the first and the last unit sought stand where they would, found a block at a time, then checked
// whole; folded where `folded` is 1
unsigned long long cli_vector_find_on(const CliVectorTextCfg *args)
{
    if (args->needle_length == 0ull)
    {
        return (args->from <= args->length) ? args->from : CLI_VECTOR_NONE;
    }
    if ((args->from >= args->length) || ((args->length - args->from) < args->needle_length))
    {
        return CLI_VECTOR_NONE;
    }
    const unsigned short *const rest = args->text + args->from;
    const unsigned long long remainder_length = args->length - args->from;
    if ((args->needle_length == 1ull) && !args->folded)
    {
        const unsigned long long at = EMBED_CALL(cli_vector_unit.find_unit_on, CliVectorUnitCfg, .path = args->path,
                                                 .text = rest, .length = remainder_length, .unit = args->needle[0]);
        return (at == CLI_VECTOR_NONE) ? at : (at + args->from);
    }
    unsigned long long found = CLI_VECTOR_NONE;
#if ENGINE_TARGET_X86_64
    if (args->path == CLI_VECTOR_SSE2)
    {
        found = EMBED_CALL(cli_vector_text_length_x86.find_sse2, CliVectorTextLengthCfg, .text = rest,
                           .length = remainder_length, .needle = args->needle, .needle_length = args->needle_length,
                           .folded = args->folded);
    }
    else if ((args->path == CLI_VECTOR_AVX2) && cli_vector_avx2_present())
    {
        found = EMBED_CALL(cli_vector_text_length_x86.find_avx2, CliVectorTextLengthCfg, .text = rest,
                           .length = remainder_length, .needle = args->needle, .needle_length = args->needle_length,
                           .folded = args->folded);
    }
    else
#endif
#if ENGINE_TARGET_AARCH64
        if (args->path == CLI_VECTOR_NEON)
    {
        found = EMBED_CALL(cli_vector_text_length_neon.find_neon, CliVectorTextLengthCfg, .text = rest,
                           .length = remainder_length, .needle = args->needle, .needle_length = args->needle_length,
                           .folded = args->folded);
    }
    else
#endif
    {
        found = EMBED_CALL(cli_vector_text_length.plain_find, CliVectorTextLengthCfg, .text = rest,
                           .length = remainder_length, .needle = args->needle, .needle_length = args->needle_length,
                           .folded = args->folded, .start = 0ull);
    }
    return (found == CLI_VECTOR_NONE) ? found : (found + args->from);
}

unsigned long long cli_vector_find_unit(const CliVectorUnitCfg *args)
{
    return EMBED_CALL(cli_vector_unit.find_unit_on, CliVectorUnitCfg, .path = cli_vector_best(), .text = args->text,
                      .length = args->length, .unit = args->unit);
}

unsigned long long cli_vector_find(const CliVectorTextCfg *args)
{
    return EMBED_CALL(cli_vector_text.find_on, CliVectorTextCfg, .path = cli_vector_best(), .text = args->text,
                      .length = args->length, .needle = args->needle, .needle_length = args->needle_length,
                      .from = args->from, .folded = 0);
}

unsigned long long cli_vector_find_fold(const CliVectorTextCfg *args)
{
    return EMBED_CALL(cli_vector_text.find_on, CliVectorTextCfg, .path = cli_vector_best(), .text = args->text,
                      .length = args->length, .needle = args->needle, .needle_length = args->needle_length,
                      .from = args->from, .folded = 1);
}

int cli_vector_same(const CliVectorACfg *args)
{
    return EMBED_CALL(cli_vector_a.same_on, CliVectorACfg, .path = cli_vector_best(), .a = args->a,
                      .a_length = args->a_length, .b = args->b, .b_length = args->b_length);
}

unsigned long long cli_vector_common(const CliVectorALengthCfg *args)
{
    return EMBED_CALL(cli_vector_length.common_on, CliVectorLengthCfg, .path = cli_vector_best(),
                      .a_u64s = args->a_u64s, .b_u64s = args->b_u64s,
                      .length = (args->a_length < args->b_length) ? args->a_length : args->b_length);
}
