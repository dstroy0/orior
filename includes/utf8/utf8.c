// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// utf8.c: a text of UTF-8 read letter by letter and walked by a set of bytes
#include "utf8.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

typedef struct
{
    const Utf8Set *filled;
    const char *text;
    unsigned long long at;
    unsigned long long size;
    int in;
} Utf8Ctx;

unsigned int utf8_letter(const Utf8AtCfg *args)
{
    unsigned long long *length = args->length;
    const unsigned char lead = (unsigned char)args->text[args->at];
    unsigned long long want;
    if (lead < 0xc0u)
    {
        want = 1u;
    }
    else if (lead < 0xe0u)
    {
        want = 2u;
    }
    else if (lead < 0xf0u)
    {
        want = 3u;
    }
    else
    {
        want = 4u;
    }
    if ((want == 1u) || (args->at + want > args->size))
    {
        *length = 1u;
        return lead;
    }
    const unsigned int long_lead_bits = (want == 3u) ? 0x0fu : 0x07u;
    unsigned int letter = lead & ((want == 2u) ? 0x1fu : long_lead_bits);
    for (unsigned long long next = 1ULL; next < want; next += 1u)
    {
        const unsigned char more = (unsigned char)args->text[args->at + next];
        if ((more & 0xc0u) != 0x80u)
        {
            *length = 1u;
            return lead;
        }
        letter = (letter << 6u) | (more & 0x3fu);
    }
    *length = want;
    return letter;
}

unsigned int utf8_letter_before(const Utf8EndCfg *args)
{
    unsigned long long *length = args->length;
    unsigned long long start = args->end - 1u;
    while ((start > 0u) && (args->end - start < 4u) && (((unsigned char)args->text[start] & 0xc0u) == 0x80u))
    {
        start -= 1u;
    }
    unsigned long long read = 0ULL;
    const unsigned int letter =
        EMBED_CALL(utf8_at.letter, Utf8AtCfg, .text = args->text, .size = args->end, .at = start, .length = &read);
    if (start + read != args->end)
    {
        *length = 1u;
        return (unsigned char)args->text[args->end - 1u];
    }
    *length = read;
    return letter;
}

void utf8_set_fill(const Utf8SetFillCfg *args)
{
    const embed_word count = cellul.len(&(CatenaFinitaCfg){.src = args->set, .cap = UTF8_SET_MAX});
    // a set is at most UTF8_SET_MAX bytes, which an unsigned int stores
    args->filled_out->count = (unsigned int)count;
    for (unsigned int at = 0u; at < args->filled_out->count; at += 1u)
    {
        // Explicit cast reads the byte as unsigned before it is repeated into every lane
        const embed_u8 byte = (embed_u8)args->set[at];
        args->filled_out->word_text[at] = MMGR_SWAR_ONES * (embed_word)byte;
        args->filled_out->lane_at[at] = embed_lane_fill_byte(byte);
    }
}

embed_word utf8_word_in(const Utf8WordCfg *args)
{
    embed_word hit = 0u;
    for (unsigned int at = 0u; at < args->filled->count; at += 1u)
    {
        hit |= lane.has_zero(&(ScrutLaneCfg){.word = args->loaded ^ args->filled->word_text[at]});
    }
    return hit;
}

// the first place from `at` below `size` whose byte the set holds where `in`, or does not hold where not, else `size`,
// the widest lane first, then a word at a time, and the last bytes loaded into a word of their own
static unsigned long long utf8_walk(const Utf8Ctx *args)
{
    unsigned long long at = args->at;
    if (at >= args->size)
    {
        return at;
    }
    // Explicit cast reads the text's bytes as unsigned
    const embed_u8 *const bytes = (const embed_u8 *)args->text;
    const Utf8Set *const filled = args->filled;
    unsigned long long place = at;
    while ((args->size - place) >= EMBED_LANE_BYTES)
    {
        const embed_lane loaded = embed_lane_load(bytes + place);
        embed_lane_mask in_lanes = 0u;
        for (unsigned int one = 0u; one < filled->count; one += 1u)
        {
            in_lanes |= embed_lane_bytes_equal(loaded, filled->lane_at[one]);
        }
        const embed_lane_mask hit = args->in ? in_lanes : (embed_lane_mask)(EMBED_LANE_MASK_ALL & ~in_lanes);
        if (hit != 0u)
        {
            return place + embed_lane_mask_first(hit);
        }
        place += EMBED_LANE_BYTES;
    }
    const embed_word every = mask.lanes_below(&(ScrutMaskCfg){.bytes = MMGR_SWAR_BYTES});
    while ((args->size - place) >= MMGR_SWAR_BYTES)
    {
        const embed_word found = EMBED_CALL(utf8_word.in, Utf8WordCfg,
                                            .loaded = word.load(&(ScrutWordCfg){.at = bytes + place}), .filled = filled);
        const embed_word hit = args->in ? found : (every & ~found);
        if (hit != 0u)
        {
            return place + lane.first(&(ScrutLaneCfg){.mask = hit});
        }
        place += MMGR_SWAR_BYTES;
    }
    if (place < args->size)
    {
        embed_word loaded = 0u;
        memor.cpy(&(MemoriaCfg){.dst = &loaded, .src = bytes + place, .bytes = (embed_word)(args->size - place)});
        const embed_word found = EMBED_CALL(utf8_word.in, Utf8WordCfg, .loaded = loaded, .filled = filled);
        const embed_word hit = (args->in ? found : (every & ~found)) &
                               mask.lanes_below(&(ScrutMaskCfg){.bytes = (embed_word)(args->size - place)});
        if (hit != 0u)
        {
            return place + lane.first(&(ScrutLaneCfg){.mask = hit});
        }
    }
    return args->size;
}

// the place past the last byte before `at` whose byte the set holds where `in`, or does not hold where not, else 0, a
// word at a time from the end, the first bytes loaded into a word of their own
static unsigned long long utf8_walked_back(const Utf8Ctx *args)
{
    unsigned long long at = args->at;
    // Explicit cast reads the text's bytes as unsigned
    const embed_u8 *const bytes = (const embed_u8 *)args->text;
    const Utf8Set *const filled = args->filled;
    const embed_word every = mask.lanes_below(&(ScrutMaskCfg){.bytes = MMGR_SWAR_BYTES});
    unsigned long long place = at;
    while (place >= MMGR_SWAR_BYTES)
    {
        const embed_word found =
            EMBED_CALL(utf8_word.in, Utf8WordCfg,
                       .loaded = word.load(&(ScrutWordCfg){.at = bytes + (place - MMGR_SWAR_BYTES)}), .filled = filled);
        const embed_word hit = args->in ? found : (every & ~found);
        if (hit != 0u)
        {
            return (place - MMGR_SWAR_BYTES) + lane.last(&(ScrutLaneCfg){.mask = hit}) + 1ULL;
        }
        place -= MMGR_SWAR_BYTES;
    }
    if (place != 0ULL)
    {
        embed_word loaded = 0u;
        memor.cpy(&(MemoriaCfg){.dst = &loaded, .src = bytes, .bytes = (embed_word)place});
        const embed_word found = EMBED_CALL(utf8_word.in, Utf8WordCfg, .loaded = loaded, .filled = filled);
        const embed_word hit =
            (args->in ? found : (every & ~found)) & mask.lanes_below(&(ScrutMaskCfg){.bytes = (embed_word)place});
        if (hit != 0u)
        {
            return lane.last(&(ScrutLaneCfg){.mask = hit}) + 1ULL;
        }
    }
    return 0ULL;
}

// the set filled and the walk run forward with `in`, or back
EMBED_INLINE unsigned long long utf8_set_walked(const Utf8SetCfg *args, int in, int back)
{
    Utf8Set filled;
    EMBED_CALL(utf8_set_fill_table.fill, Utf8SetFillCfg, .filled_out = &filled, .set = args->set);
    const Utf8Ctx walk = {&filled, args->text, args->at, args->size, in};
    return back ? utf8_walked_back(&walk) : utf8_walk(&walk);
}

unsigned long long utf8_to_any(const Utf8SetCfg *args)
{
    return utf8_set_walked(args, 1, 0);
}

unsigned long long utf8_end_any(const Utf8SetCfg *args)
{
    return utf8_set_walked(args, 0, 0);
}

unsigned long long utf8_back_any(const Utf8SetCfg *args)
{
    return utf8_set_walked(args, 1, 1);
}

unsigned long long utf8_back_end(const Utf8SetCfg *args)
{
    return utf8_set_walked(args, 0, 1);
}
