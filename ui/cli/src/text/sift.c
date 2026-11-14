// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sift.c: every offset a needle stands at in a text: each start the library's byte find gives for the needle's first
// byte, proven by the library's bounded compare, so a zero byte in the text is a byte like any other
#include "sift.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

// one byte the search anchors on, and the next start that holds it, kept until the walk passes it
typedef struct
{
    unsigned char val;
    const unsigned char *next;
    int none;
} SiftAnchor;

typedef struct
{
    const CliSiftLengthCfg *cfg;
    const unsigned char *text;
    unsigned long long starts;
    SiftAnchor *anchor;
    unsigned long long from;
} SiftCtx;

// the next start at or after `from` that holds the anchor's byte, NULL where none is left before the last start
EMBED_INLINE const unsigned char *sift_anchor_next(const SiftCtx *args)
{
    SiftAnchor *const anchor = args->anchor;
    if (anchor->none)
    {
        return NULL;
    }
    if ((anchor->next != NULL) && (anchor->next >= args->text + args->from))
    {
        return anchor->next;
    }
    anchor->next = (const unsigned char *)memor.chr(
        &(MemoriaCfg){.src = args->text + args->from, .val = anchor->val, .bytes = args->starts - args->from});
    anchor->none = (anchor->next == NULL);
    return anchor->next;
}

// the first byte's other case where the needle's first byte is an ASCII letter and the search folds case, else the
// byte itself
EMBED_INLINE unsigned char sift_first_other_case(const CliSiftLengthCfg *args)
{
    const unsigned char first = args->needle[0];
    const unsigned char lower = (unsigned char)(first | 0x20u);
    if (!args->fold || (lower < 'a') || (lower > 'z'))
    {
        return first;
    }
    return (unsigned char)(first ^ 0x20u);
}

unsigned long long cli_sift_find_all(const CliSiftLengthCfg *args)
{
    const unsigned char *const text = args->text;
    const unsigned long long needle_length = args->needle_length;
    if ((needle_length == 0ULL) || (needle_length > args->length))
    {
        return 0ULL;
    }
    // a start is any offset the whole needle fits at; the anchors look no further than the last one
    const unsigned long long starts = args->length - needle_length + 1ULL;
    SiftAnchor one = {.val = args->needle[0]};
    SiftAnchor two = {.val = sift_first_other_case(args)};
    const int cased = two.val != one.val;
    unsigned long long count = 0ULL;
    unsigned long long at = 0ULL;
    while (at < starts)
    {
        const unsigned char *const first =
            EMBED_CALL(sift_anchor_next, SiftCtx, .text = text, .starts = starts, .anchor = &one, .from = at);
        const unsigned char *const other =
            cased ? EMBED_CALL(sift_anchor_next, SiftCtx, .text = text, .starts = starts, .anchor = &two, .from = at)
                  : NULL;
        const unsigned char *const hit =
            (first == NULL) ? other : (((other == NULL) || (first < other)) ? first : other);
        if (hit == NULL)
        {
            break;
        }
        // a hit stands inside the text, so its distance from the start is not negative
        at = (unsigned long long)(hit - text);
        if (cellul.diff(&(CatenaFinitaCfg){.src = (const char *)text + at,
                                           .other = (const char *)args->needle,
                                           .cap = needle_length,
                                           .ci = args->fold ? EMBED_TRUE : EMBED_FALSE}) == needle_length)
        {
            EMBED_CALL(args->found, CliEachAtCfg, .context = args->context, .at = at);
            count += 1ULL;
        }
        at += 1ULL;
    }
    return count;
}
