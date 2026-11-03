// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// vector.h: the inner loops of the page's slow modules, over text stored in UTF-16 units as the page's own strings
// store it, each written in the processor's vector instructions: SSE2, and AVX2 where the machine has it, on x86-64
// (vector_x86.c); NEON on aarch64 (vector_neon.c); and a plain loop where neither is there (vector.c). Each path
// answers as the plain loop does, which the checks check each one against on every path the machine runs.
//
// A search without case folds as the page's own does with the `i` and `u` flags: each unit of the text and of what is
// sought to its simple case fold. Only text sought in ASCII is searched here; two units outside ASCII fold into it,
// `ſ` to `s` and the Kelvin sign to `k`, and are folded too
#ifndef VECTOR_H
#define VECTOR_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliVectorTextCfg CliVectorTextCfg;
typedef struct CliVectorTextLengthCfg CliVectorTextLengthCfg;
typedef struct CliVectorACfg CliVectorACfg;
typedef struct CliVectorTextNeedleCfg CliVectorTextNeedleCfg;
typedef struct CliVectorALengthCfg CliVectorALengthCfg;
typedef struct CliVectorLengthCfg CliVectorLengthCfg;
typedef struct CliVectorUnitCfg CliVectorUnitCfg;
typedef struct CliVectorLengthACfg CliVectorLengthACfg;
typedef struct CliVectorCfg CliVectorCfg;

// none found
#define CLI_VECTOR_NONE 0xffffffffffffffffull

// the two units outside ASCII that fold into it
#define CLI_LONG_S 0x017fu

#define CLI_KELVIN 0x212au

// the ways the loops run: the plain loop, or one of the processor's sets of vector instructions
enum
{
    CLI_VECTOR_PLAIN,
    CLI_VECTOR_SSE2,
    CLI_VECTOR_AVX2,
    CLI_VECTOR_NEON
};

// the paths this machine runs into `paths`, sized for 4, the plain loop first and the widest last: their count
unsigned int cli_vector_paths(const CliVectorCfg *args);
// the widest path this machine runs
unsigned int cli_vector_best(void);
// a unit folded as the page's search without case folds it, for units that fold into ASCII
unsigned short cli_vector_fold(const CliVectorUnitCfg *args);
// where `unit` first stands in `text`; where `needle` first stands in `text` from `from` on, unit for unit, and
// without case where `needle` is ASCII; whether two runs of units are the same; how many of the numbers at the starts
// of `a` and `b` are the same, one for one. Each on the widest path, or on the path given
unsigned long long cli_vector_find_unit(const CliVectorUnitCfg *args);
unsigned long long cli_vector_find(const CliVectorTextCfg *args);
unsigned long long cli_vector_find_fold(const CliVectorTextCfg *args);
int cli_vector_same(const CliVectorACfg *args);
unsigned long long cli_vector_common(const CliVectorALengthCfg *args);
unsigned long long cli_vector_find_unit_on(const CliVectorUnitCfg *args);
unsigned long long cli_vector_find_on(const CliVectorTextCfg *args);
int cli_vector_same_on(const CliVectorACfg *args);
unsigned long long cli_vector_common_on(const CliVectorLengthCfg *args);

// what vector.c and the paths share: whether `needle` stands in `text` at `at`, each unit folded where `folded` is 1;
// the plain loop of a find from `start` on; and each path's loops, where the machine has them
int cli_vector_exists_at(const CliVectorTextNeedleCfg *args);
unsigned long long cli_vector_plain_find(const CliVectorTextLengthCfg *args);
#if ENGINE_TARGET_X86_64
int cli_vector_avx2_present(void);
unsigned long long cli_vector_find_unit_sse2(const CliVectorUnitCfg *args);
unsigned long long cli_vector_find_unit_avx2(const CliVectorUnitCfg *args);
unsigned long long cli_vector_common_sse2(const CliVectorLengthCfg *args);
unsigned long long cli_vector_common_avx2(const CliVectorLengthCfg *args);
int cli_vector_same_sse2(const CliVectorLengthACfg *args);
int cli_vector_same_avx2(const CliVectorLengthACfg *args);
unsigned long long cli_vector_find_sse2(const CliVectorTextLengthCfg *args);
unsigned long long cli_vector_find_avx2(const CliVectorTextLengthCfg *args);
#endif
#if ENGINE_TARGET_AARCH64
unsigned long long cli_vector_find_unit_neon(const CliVectorUnitCfg *args);
unsigned long long cli_vector_common_neon(const CliVectorLengthCfg *args);
int cli_vector_same_neon(const CliVectorLengthACfg *args);
unsigned long long cli_vector_find_neon(const CliVectorTextLengthCfg *args);
#endif

struct CliVectorTextCfg
{
    const unsigned short *text;
    unsigned long long length;
    const unsigned short *needle;
    unsigned long long needle_length;
    unsigned long long from;
    unsigned int path;
    int folded;
};

struct CliVectorTextLengthCfg
{
    const unsigned short *text;
    unsigned long long length;
    const unsigned short *needle;
    unsigned long long needle_length;
    int folded;
    unsigned long long start;
};

struct CliVectorACfg
{
    const unsigned short *a;
    unsigned long long a_length;
    const unsigned short *b;
    unsigned long long b_length;
    unsigned int path;
};

struct CliVectorTextNeedleCfg
{
    const unsigned short *text;
    const unsigned short *needle;
    unsigned long long needle_length;
    int folded;
    unsigned long long at;
};

struct CliVectorALengthCfg
{
    unsigned long long a_length;
    unsigned long long b_length;
    const unsigned long long *a_u64s;
    const unsigned long long *b_u64s;
};

struct CliVectorLengthCfg
{
    unsigned long long length;
    const unsigned long long *a_u64s;
    const unsigned long long *b_u64s;
    unsigned int path;
};

struct CliVectorUnitCfg
{
    unsigned short unit;
    const unsigned short *text;
    unsigned long long length;
    unsigned int path;
};

struct CliVectorLengthACfg
{
    unsigned long long length;
    const unsigned short *a;
    const unsigned short *b;
};

struct CliVectorCfg
{
    unsigned int *paths;
};

typedef struct
{
    unsigned int (*paths)(const CliVectorCfg *args);
} CliVectorNs;
EMBED_TABLE_LAYOUT(CliVectorNs, paths);
EMBED_TABLE_STORAGE CliVectorNs cli_vector EMBED_UNUSED = {
    .paths = cli_vector_paths,
};

typedef struct
{
    unsigned short (*fold)(const CliVectorUnitCfg *args);
    unsigned long long (*find_unit)(const CliVectorUnitCfg *args);
    unsigned long long (*find_unit_on)(const CliVectorUnitCfg *args);
} CliVectorUnitNs;
EMBED_TABLE_LAYOUT(CliVectorUnitNs, fold, find_unit, find_unit_on);
EMBED_TABLE_STORAGE CliVectorUnitNs cli_vector_unit EMBED_UNUSED = {
    .fold = cli_vector_fold,
    .find_unit = cli_vector_find_unit,
    .find_unit_on = cli_vector_find_unit_on,
};

typedef struct
{
    unsigned long long (*find)(const CliVectorTextCfg *args);
    unsigned long long (*find_fold)(const CliVectorTextCfg *args);
    unsigned long long (*find_on)(const CliVectorTextCfg *args);
} CliVectorTextNs;
EMBED_TABLE_LAYOUT(CliVectorTextNs, find, find_fold, find_on);
EMBED_TABLE_STORAGE CliVectorTextNs cli_vector_text EMBED_UNUSED = {
    .find = cli_vector_find,
    .find_fold = cli_vector_find_fold,
    .find_on = cli_vector_find_on,
};

typedef struct
{
    int (*same)(const CliVectorACfg *args);
    int (*same_on)(const CliVectorACfg *args);
} CliVectorANs;
EMBED_TABLE_LAYOUT(CliVectorANs, same, same_on);
EMBED_TABLE_STORAGE CliVectorANs cli_vector_a EMBED_UNUSED = {
    .same = cli_vector_same,
    .same_on = cli_vector_same_on,
};

typedef struct
{
    unsigned long long (*common)(const CliVectorALengthCfg *args);
} CliVectorALengthNs;
EMBED_TABLE_LAYOUT(CliVectorALengthNs, common);
EMBED_TABLE_STORAGE CliVectorALengthNs cli_vector_a_length EMBED_UNUSED = {
    .common = cli_vector_common,
};

typedef struct
{
    unsigned long long (*common_on)(const CliVectorLengthCfg *args);
} CliVectorLengthNs;
EMBED_TABLE_LAYOUT(CliVectorLengthNs, common_on);
EMBED_TABLE_STORAGE CliVectorLengthNs cli_vector_length EMBED_UNUSED = {
    .common_on = cli_vector_common_on,
};

typedef struct
{
    int (*exists_at)(const CliVectorTextNeedleCfg *args);
} CliVectorTextNeedleNs;
EMBED_TABLE_LAYOUT(CliVectorTextNeedleNs, exists_at);
EMBED_TABLE_STORAGE CliVectorTextNeedleNs cli_vector_text_needle EMBED_UNUSED = {
    .exists_at = cli_vector_exists_at,
};

typedef struct
{
    unsigned long long (*plain_find)(const CliVectorTextLengthCfg *args);
} CliVectorTextLengthNs;
EMBED_TABLE_LAYOUT(CliVectorTextLengthNs, plain_find);
EMBED_TABLE_STORAGE CliVectorTextLengthNs cli_vector_text_length EMBED_UNUSED = {
    .plain_find = cli_vector_plain_find,
};

// each path's loops, on the architecture that has them
#if ENGINE_TARGET_X86_64
typedef struct
{
    unsigned long long (*find_unit_sse2)(const CliVectorUnitCfg *args);
    unsigned long long (*find_unit_avx2)(const CliVectorUnitCfg *args);
} CliVectorUnitX86Ns;
EMBED_TABLE_LAYOUT(CliVectorUnitX86Ns, find_unit_sse2, find_unit_avx2);
EMBED_TABLE_STORAGE CliVectorUnitX86Ns cli_vector_unit_x86 EMBED_UNUSED = {
    .find_unit_sse2 = cli_vector_find_unit_sse2,
    .find_unit_avx2 = cli_vector_find_unit_avx2,
};

typedef struct
{
    unsigned long long (*common_sse2)(const CliVectorLengthCfg *args);
    unsigned long long (*common_avx2)(const CliVectorLengthCfg *args);
} CliVectorLengthX86Ns;
EMBED_TABLE_LAYOUT(CliVectorLengthX86Ns, common_sse2, common_avx2);
EMBED_TABLE_STORAGE CliVectorLengthX86Ns cli_vector_length_x86 EMBED_UNUSED = {
    .common_sse2 = cli_vector_common_sse2,
    .common_avx2 = cli_vector_common_avx2,
};

typedef struct
{
    int (*same_sse2)(const CliVectorLengthACfg *args);
    int (*same_avx2)(const CliVectorLengthACfg *args);
} CliVectorLengthAX86Ns;
EMBED_TABLE_LAYOUT(CliVectorLengthAX86Ns, same_sse2, same_avx2);
EMBED_TABLE_STORAGE CliVectorLengthAX86Ns cli_vector_length_a_x86 EMBED_UNUSED = {
    .same_sse2 = cli_vector_same_sse2,
    .same_avx2 = cli_vector_same_avx2,
};

typedef struct
{
    unsigned long long (*find_sse2)(const CliVectorTextLengthCfg *args);
    unsigned long long (*find_avx2)(const CliVectorTextLengthCfg *args);
} CliVectorTextLengthX86Ns;
EMBED_TABLE_LAYOUT(CliVectorTextLengthX86Ns, find_sse2, find_avx2);
EMBED_TABLE_STORAGE CliVectorTextLengthX86Ns cli_vector_text_length_x86 EMBED_UNUSED = {
    .find_sse2 = cli_vector_find_sse2,
    .find_avx2 = cli_vector_find_avx2,
};
#endif

#if ENGINE_TARGET_AARCH64
typedef struct
{
    unsigned long long (*find_unit_neon)(const CliVectorUnitCfg *args);
} CliVectorUnitNeonNs;
EMBED_TABLE_LAYOUT(CliVectorUnitNeonNs, find_unit_neon);
EMBED_TABLE_STORAGE CliVectorUnitNeonNs cli_vector_unit_neon EMBED_UNUSED = {
    .find_unit_neon = cli_vector_find_unit_neon,
};

typedef struct
{
    unsigned long long (*common_neon)(const CliVectorLengthCfg *args);
} CliVectorLengthNeonNs;
EMBED_TABLE_LAYOUT(CliVectorLengthNeonNs, common_neon);
EMBED_TABLE_STORAGE CliVectorLengthNeonNs cli_vector_length_neon EMBED_UNUSED = {
    .common_neon = cli_vector_common_neon,
};

typedef struct
{
    int (*same_neon)(const CliVectorLengthACfg *args);
} CliVectorLengthANeonNs;
EMBED_TABLE_LAYOUT(CliVectorLengthANeonNs, same_neon);
EMBED_TABLE_STORAGE CliVectorLengthANeonNs cli_vector_length_a_neon EMBED_UNUSED = {
    .same_neon = cli_vector_same_neon,
};

typedef struct
{
    unsigned long long (*find_neon)(const CliVectorTextLengthCfg *args);
} CliVectorTextLengthNeonNs;
EMBED_TABLE_LAYOUT(CliVectorTextLengthNeonNs, find_neon);
EMBED_TABLE_STORAGE CliVectorTextLengthNeonNs cli_vector_text_length_neon EMBED_UNUSED = {
    .find_neon = cli_vector_find_neon,
};
#endif

#endif
