// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sift.h: where a needle of bytes stands in a text, the engine's sift (sift.c, sift_x86.c): the text read a window at
// a time, the bytes of each window counted, and the needle's anchors placed where the window shows they refute most.
// The candidate offsets are ranked by the census, the rarest byte first, and the anchors are then placed one level at
// a time, each at the offset that leaves fewest of the alignments the anchors above it left standing, a level ending
// the descent where it leaves standing every one of them. The first two anchors are tested a block of alignments at a
// time in the processor's vector instructions, the rest one alignment at a time, and every alignment all of them agree
// with is compared whole.
//
// The census and the plan steer and never decide: an alignment stands only where every anchor agrees and the whole
// needle is equal there, whatever placed the anchors. A census taken on a sample and a plan scored on a sample cost
// speed and never a match. The checks cover every path to the plain loop.
//
// A needle searched without case is searched in ASCII case: each letter of the text and of the needle to its lower case
#ifndef SIFT_H
#define SIFT_H
typedef struct CliSiftWindowCfg CliSiftWindowCfg;
typedef struct CliSiftWindowLengthCfg CliSiftWindowLengthCfg;
typedef struct CliSiftLengthCfg CliSiftLengthCfg;
typedef struct CliSiftLengthFoldCfg CliSiftLengthFoldCfg;
typedef struct CliSiftWindowFoldCfg CliSiftWindowFoldCfg;
typedef struct CliSiftWindowLengthFoldCfg CliSiftWindowLengthFoldCfg;
typedef struct CliSiftWindowLengthFoldCensusCfg CliSiftWindowLengthFoldCensusCfg;
typedef struct CliSiftLengthCensusCfg CliSiftLengthCensusCfg;
typedef struct CliSiftCfg CliSiftCfg;
typedef struct CliSiftFoldCfg CliSiftFoldCfg;

#include "../command_line/callback_cfg.h"
#include "embed_dispatch_layout.h"
#include "vector.h"

// the most anchors a plan places, and the bytes a window contains, the last needle's length less one of them the next
// window's first too
#define CLI_SIFT_ANCHORS 4u

#define CLI_SIFT_WINDOW (1u << 20u)

// How often each byte stands in a window, read on every so many of its bytes
typedef struct
{
    unsigned long long occurrences[256];
    unsigned long long total;
} CliCensus;

// the census of a window, of every so many of its bytes as bounds the count
void cli_sift_census(const CliSiftWindowLengthFoldCensusCfg *args);
// a census stored with a text: every byte of `bytes` counted into it as they enter the text, and counted out of it as
// they leave, so the text's census stands without a count over the text; and the census as a search without case
// reads it, each capital's count joined to its small letter's
void cli_sift_census_count(const CliSiftLengthCensusCfg *args);
void cli_sift_census_uncounted(const CliSiftLengthCensusCfg *args);
void cli_sift_census_fold(const CliSiftCfg *args);
// the anchors placed on a needle for a window into `anchors`, sized for CLI_SIFT_ANCHORS: offsets into the needle,
// the first refuting most; their count
unsigned int cli_sift_plan(const CliSiftWindowLengthFoldCfg *args);
// every offset `needle` stands at in `text`, read a window at a time, each handed to `found` in order, on the widest
// path or the path given: their count
unsigned long long cli_sift_find_all(const CliSiftLengthCfg *args);
// every offset `needle` stands at in a text within one window, found by the needle's own shifts, Horspool's, with no
// plan made: their count; what cli_sift_find_all takes for such a text
unsigned long long cli_sift_horspool(const CliSiftLengthCfg *args);
unsigned long long cli_sift_find_all_on(const CliSiftLengthCfg *args);
// the same, each window's plan ranked by `census`, the text's stored census as the search reads it, folded where the
// search folds, in place of a count over the window
unsigned int cli_sift_plan_with(const CliSiftWindowLengthFoldCfg *args);
unsigned long long cli_sift_find_all_with(const CliSiftLengthFoldCfg *args);

// what sift.c and sift_x86.c share: a byte as a search without case reads it; whether the anchors from `from` on
// agree at `at` and the whole needle stands there; the plain loop of a window from the alignment `start`; and the
// window's loops, sixteen or thirty-two alignments at a time. Each hands every alignment found to `found` from `base`
// on, and gives their count
unsigned char cli_sift_on_fold(const CliSiftFoldCfg *args);
int cli_sift_matches(const CliSiftWindowFoldCfg *args);
unsigned long long cli_sift_plain_window(const CliSiftWindowCfg *args);
unsigned long long cli_sift_window_sse2(const CliSiftWindowLengthCfg *args);
unsigned long long cli_sift_window_avx2(const CliSiftWindowLengthCfg *args);

struct CliSiftWindowCfg
{
    const unsigned char *window;
    unsigned long long length;
    int fold;
    const unsigned char *needle;
    unsigned long long needle_length;
    void (*found)(const CliEachAtCfg *args);
    void *context;
    const unsigned long long *anchors;
    unsigned int anchor_count;
    unsigned long long start;
    unsigned long long base;
};

struct CliSiftWindowLengthCfg
{
    const unsigned char *window;
    unsigned long long length;
    int fold;
    const unsigned char *needle;
    unsigned long long needle_length;
    void (*found)(const CliEachAtCfg *args);
    void *context;
    const unsigned long long *anchors;
    unsigned int anchor_count;
    unsigned long long base;
};

struct CliSiftLengthCfg
{
    unsigned long long length;
    int fold;
    const unsigned char *needle;
    unsigned long long needle_length;
    const unsigned char *text;
    void (*found)(const CliEachAtCfg *args);
    void *context;
    unsigned int path;
};

struct CliSiftLengthFoldCfg
{
    unsigned long long length;
    int fold;
    const unsigned char *needle;
    unsigned long long needle_length;
    const unsigned char *text;
    void (*found)(const CliEachAtCfg *args);
    void *context;
    const CliCensus *census_read;
};

struct CliSiftWindowFoldCfg
{
    const unsigned char *window;
    int fold;
    const unsigned char *needle;
    unsigned long long needle_length;
    const unsigned long long *anchors;
    unsigned int anchor_count;
    unsigned int from;
    unsigned long long at;
};

struct CliSiftWindowLengthFoldCfg
{
    const unsigned char *window;
    unsigned long long length;
    int fold;
    const unsigned char *needle;
    unsigned long long needle_length;
    unsigned long long *anchors_out;
    const CliCensus *census_read;
};

struct CliSiftWindowLengthFoldCensusCfg
{
    const unsigned char *window;
    unsigned long long length;
    int fold;
    CliCensus *census;
};

struct CliSiftLengthCensusCfg
{
    unsigned long long length;
    CliCensus *census;
    const unsigned char *bytes;
};

struct CliSiftCfg
{
    const CliCensus *exact;
    CliCensus *folded;
};

struct CliSiftFoldCfg
{
    int fold;
    unsigned char byte;
};

typedef struct
{
    void (*census)(const CliSiftWindowLengthFoldCensusCfg *args);
} CliSiftWindowLengthFoldCensusNs;
EMBED_TABLE_LAYOUT(CliSiftWindowLengthFoldCensusNs, census);
EMBED_TABLE_STORAGE CliSiftWindowLengthFoldCensusNs cli_sift_window_length_fold_census EMBED_UNUSED = {
    .census = cli_sift_census,
};

typedef struct
{
    void (*census_count)(const CliSiftLengthCensusCfg *args);
    void (*census_uncounted)(const CliSiftLengthCensusCfg *args);
} CliSiftLengthCensusNs;
EMBED_TABLE_LAYOUT(CliSiftLengthCensusNs, census_count, census_uncounted);
EMBED_TABLE_STORAGE CliSiftLengthCensusNs cli_sift_length_census EMBED_UNUSED = {
    .census_count = cli_sift_census_count,
    .census_uncounted = cli_sift_census_uncounted,
};

typedef struct
{
    void (*census_fold)(const CliSiftCfg *args);
} CliSiftNs;
EMBED_TABLE_LAYOUT(CliSiftNs, census_fold);
EMBED_TABLE_STORAGE CliSiftNs cli_sift EMBED_UNUSED = {
    .census_fold = cli_sift_census_fold,
};

typedef struct
{
    unsigned int (*plan)(const CliSiftWindowLengthFoldCfg *args);
    unsigned int (*plan_with)(const CliSiftWindowLengthFoldCfg *args);
} CliSiftWindowLengthFoldNs;
EMBED_TABLE_LAYOUT(CliSiftWindowLengthFoldNs, plan, plan_with);
EMBED_TABLE_STORAGE CliSiftWindowLengthFoldNs cli_sift_window_length_fold EMBED_UNUSED = {
    .plan = cli_sift_plan,
    .plan_with = cli_sift_plan_with,
};

typedef struct
{
    unsigned long long (*find_all)(const CliSiftLengthCfg *args);
    unsigned long long (*horspool)(const CliSiftLengthCfg *args);
    unsigned long long (*find_all_on)(const CliSiftLengthCfg *args);
} CliSiftLengthNs;
EMBED_TABLE_LAYOUT(CliSiftLengthNs, find_all, horspool, find_all_on);
EMBED_TABLE_STORAGE CliSiftLengthNs cli_sift_length EMBED_UNUSED = {
    .find_all = cli_sift_find_all,
    .horspool = cli_sift_horspool,
    .find_all_on = cli_sift_find_all_on,
};

typedef struct
{
    unsigned long long (*find_all_with)(const CliSiftLengthFoldCfg *args);
} CliSiftLengthFoldNs;
EMBED_TABLE_LAYOUT(CliSiftLengthFoldNs, find_all_with);
EMBED_TABLE_STORAGE CliSiftLengthFoldNs cli_sift_length_fold EMBED_UNUSED = {
    .find_all_with = cli_sift_find_all_with,
};

typedef struct
{
    unsigned char (*fold)(const CliSiftFoldCfg *args);
} CliSiftFoldNs;
EMBED_TABLE_LAYOUT(CliSiftFoldNs, fold);
EMBED_TABLE_STORAGE CliSiftFoldNs cli_sift_fold EMBED_UNUSED = {
    .fold = cli_sift_on_fold,
};

typedef struct
{
    int (*matches)(const CliSiftWindowFoldCfg *args);
} CliSiftWindowFoldNs;
EMBED_TABLE_LAYOUT(CliSiftWindowFoldNs, matches);
EMBED_TABLE_STORAGE CliSiftWindowFoldNs cli_sift_window_fold EMBED_UNUSED = {
    .matches = cli_sift_matches,
};

typedef struct
{
    unsigned long long (*plain_window)(const CliSiftWindowCfg *args);
} CliSiftWindowNs;
EMBED_TABLE_LAYOUT(CliSiftWindowNs, plain_window);
EMBED_TABLE_STORAGE CliSiftWindowNs cli_sift_window EMBED_UNUSED = {
    .plain_window = cli_sift_plain_window,
};

typedef struct
{
    unsigned long long (*window_sse2)(const CliSiftWindowLengthCfg *args);
    unsigned long long (*window_avx2)(const CliSiftWindowLengthCfg *args);
} CliSiftWindowLengthNs;
EMBED_TABLE_LAYOUT(CliSiftWindowLengthNs, window_sse2, window_avx2);
EMBED_TABLE_STORAGE CliSiftWindowLengthNs cli_sift_window_length EMBED_UNUSED = {
    .window_sse2 = cli_sift_window_sse2,
    .window_avx2 = cli_sift_window_avx2,
};

#endif
