// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// text.c: a text of UTF-8 read letter by letter. A byte that starts no letter, or a letter cut short, is read as one
// letter of its own byte
#include "text.h"
#include "json_tree.h"
#include "regexp.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include "config/mmgr_vector.h"
#include "memoria_operor/memoria_operor.h"

// the most bytes of a set a walk tests for
#define CLI_TEXT_SET_MAX 16u
// A set's bytes each filled across a word, and across a lane where the build has one
typedef struct
{
    unsigned int count;
    embed_word word_text[CLI_TEXT_SET_MAX];
#if MMGR_VECTOR_BYTES != 0u
    MmgrVector lane_at[CLI_TEXT_SET_MAX];
#endif
} CliTextSet;
unsigned int cli_text_letter(const CliTextAtCfg *args)
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

unsigned int cli_text_letter_before(const CliTextLengthCfg *args)
{
    unsigned long long *length = args->length;
    unsigned long long start = args->end_value - 1u;
    while ((start > 0u) && (args->end_value - start < 4u) && (((unsigned char)args->text[start] & 0xc0u) == 0x80u))
    {
        start -= 1u;
    }
    unsigned long long read = 0ULL;
    const unsigned int letter = EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = args->text,
                                           .size = args->end_value, .at = start, .length = &read);
    if (start + read != args->end_value)
    {
        *length = 1u;
        return (unsigned char)args->text[args->end_value - 1u];
    }
    *length = read;
    return letter;
}

int cli_text_white(const CliTextCfg *args)
{
    return (args->letter == ' ') || ((args->letter >= '\t') && (args->letter <= '\r')) || (args->letter == 0x85u) ||
           (args->letter == 0xA0u) || (args->letter == 0x1680u) ||
           ((args->letter >= 0x2000u) && (args->letter <= 0x200Au)) || (args->letter == 0x2028u) ||
           (args->letter == 0x2029u) || (args->letter == 0x202Fu) || (args->letter == 0x205Fu) ||
           (args->letter == 0x3000u);
}

int cli_text_alphabetic(const CliTextCfg *args)
{
    unsigned int letter = args->letter;
    if (letter < 0x80u)
    {
        return ((letter >= 'a') && (letter <= 'z')) || ((letter >= 'A') && (letter <= 'Z'));
    }
    return EMBED_CALL(cli_regexp_letter.property, CliRegexpLetterCfg, .property = CLI_REGEXP_LETTER, .letter = letter);
}

int cli_text_alphanumeric(const CliTextCfg *args)
{
    unsigned int letter = args->letter;
    if (letter < 0x80u)
    {
        return EMBED_CALL(cli_text_table.alphabetic, CliTextCfg, .letter = letter) ||
               ((letter >= '0') && (letter <= '9'));
    }
    return EMBED_CALL(cli_regexp_letter.property, CliRegexpLetterCfg, .property = CLI_REGEXP_LETTER,
                      .letter = letter) ||
           EMBED_CALL(cli_regexp_letter.property, CliRegexpLetterCfg, .property = CLI_REGEXP_NUMBER, .letter = letter);
}

typedef struct
{
    CliTextSet *filled_out;
    const char *set;
    embed_word loaded;
    const CliTextSet *filled;
    const char *text;
    unsigned long long at;
    unsigned long long size;
    int in;
    unsigned int classes;
    char byte;
    int in_set;
} TextCtx;
EMBED_INLINE void text_set_fill(const TextCtx *args)
{
    const size_t count = cellul.len(&(CatenaFinitaCfg){.src = args->set, .cap = CLI_TEXT_SET_MAX});
    // a set is at most CLI_TEXT_SET_MAX bytes, which an unsigned int stores
    args->filled_out->count = (unsigned int)count;
    for (unsigned int at = 0u; at < args->filled_out->count; at += 1u)
    {
        // Explicit cast reads the byte as unsigned before it is repeated into every lane
        const uint8_t byte = (uint8_t)args->set[at];
        args->filled_out->word_text[at] = MMGR_SWAR_ONES * (embed_word)byte;
#if MMGR_VECTOR_BYTES != 0u
        args->filled_out->lane_at[at] = mmgr_vector_fill(byte);
#endif
    }
}

// the lanes of a word that the set holds
EMBED_INLINE embed_word text_word_in(const TextCtx *args)
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
static unsigned long long text_walk(const TextCtx *args)
{
    unsigned long long at = args->at;
    if (at >= args->size)
    {
        return at;
    }
    // Explicit cast reads the text's bytes as unsigned
    const uint8_t *const bytes = (const uint8_t *)args->text;
    CliTextSet filled;
    EMBED_CALL(text_set_fill, TextCtx, .filled_out = &filled, .set = args->set);
    unsigned long long place = at;
#if MMGR_VECTOR_BYTES != 0u
    while ((args->size - place) >= MMGR_VECTOR_BYTES)
    {
        const MmgrVector loaded = mmgr_vector_load(bytes + place);
        MmgrVectorMask in_lanes = 0u;
        for (unsigned int one = 0u; one < filled.count; one += 1u)
        {
            in_lanes |= mmgr_vector_equal_lanes(loaded, filled.lane_at[one]);
        }
        const MmgrVectorMask hit = args->in ? in_lanes : (MmgrVectorMask)(EMBED_LANE_MASK_ALL & ~in_lanes);
        if (hit != 0u)
        {
            return place + mmgr_vector_mask_first(hit);
        }
        place += MMGR_VECTOR_BYTES;
    }
#endif
    const embed_word every = mask.lanes_below(&(ScrutMaskCfg){.bytes = MMGR_SWAR_BYTES});
    while ((args->size - place) >= MMGR_SWAR_BYTES)
    {
        const embed_word found = EMBED_CALL(
            text_word_in, TextCtx, .loaded = word.load(&(ScrutWordCfg){.at = bytes + place}), .filled = &filled);
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
        memor.cpy(&(MemoriaCfg){.dst = &loaded, .src = bytes + place, .bytes = (size_t)(args->size - place)});
        const embed_word found = EMBED_CALL(text_word_in, TextCtx, .loaded = loaded, .filled = &filled);
        const embed_word hit = (args->in ? found : (every & ~found)) &
                               mask.lanes_below(&(ScrutMaskCfg){.bytes = (size_t)(args->size - place)});
        if (hit != 0u)
        {
            return place + lane.first(&(ScrutLaneCfg){.mask = hit});
        }
    }
    return args->size;
}

// the place past the last byte before `at` whose byte the set holds where `in`, or does not hold where not, else 0, a
// word at a time from the end, the first bytes loaded into a word of their own
EMBED_INLINE unsigned long long text_walked_back(const TextCtx *args)
{
    unsigned long long at = args->at;
    // Explicit cast reads the text's bytes as unsigned
    const uint8_t *const bytes = (const uint8_t *)args->text;
    CliTextSet filled;
    EMBED_CALL(text_set_fill, TextCtx, .filled_out = &filled, .set = args->set);
    const embed_word every = mask.lanes_below(&(ScrutMaskCfg){.bytes = MMGR_SWAR_BYTES});
    unsigned long long place = at;
    while (place >= MMGR_SWAR_BYTES)
    {
        const embed_word found = EMBED_CALL(
            text_word_in, TextCtx, .loaded = word.load(&(ScrutWordCfg){.at = bytes + (place - MMGR_SWAR_BYTES)}),
            .filled = &filled);
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
        memor.cpy(&(MemoriaCfg){.dst = &loaded, .src = bytes, .bytes = (size_t)place});
        const embed_word found = EMBED_CALL(text_word_in, TextCtx, .loaded = loaded, .filled = &filled);
        const embed_word hit =
            (args->in ? found : (every & ~found)) & mask.lanes_below(&(ScrutMaskCfg){.bytes = (size_t)place});
        if (hit != 0u)
        {
            return lane.last(&(ScrutLaneCfg){.mask = hit}) + 1ULL;
        }
    }
    return 0ULL;
}

unsigned long long cli_text_to_any(const CliTextAtSizeClassesCfg *args)
{
    return EMBED_CALL(text_walk, TextCtx, .text = args->text, .at = args->at, .size = args->size, .set = args->set,
                      .in = 1);
}

unsigned long long cli_text_end_any(const CliTextAtSizeClassesCfg *args)
{
    return EMBED_CALL(text_walk, TextCtx, .text = args->text, .at = args->at, .size = args->size, .set = args->set,
                      .in = 0);
}

unsigned long long cli_text_back_any(const CliTextAtSizeClassesCfg *args)
{
    return EMBED_CALL(text_walked_back, TextCtx, .text = args->text, .at = args->at, .set = args->set, .in = 1);
}

unsigned long long cli_text_back_end(const CliTextAtSizeClassesCfg *args)
{
    return EMBED_CALL(text_walked_back, TextCtx, .text = args->text, .at = args->at, .set = args->set, .in = 0);
}

// the lanes of a word that hold a byte of `classes` or of the set
EMBED_INLINE embed_word text_word_of_class(const TextCtx *args)
{
    embed_word hit = text_word_in(args);
    if ((args->classes & CLI_TEXT_LETTERS) != 0u)
    {
        hit |= lane.alpha(&(ScrutLaneCfg){.word = args->loaded});
    }
    if ((args->classes & CLI_TEXT_DIGITS) != 0u)
    {
        hit |= lane.ge(&(ScrutLaneCfg){.word = args->loaded, .byte = '0'}) &
               lane.le(&(ScrutLaneCfg){.word = args->loaded, .byte = '9'});
    }
    if ((args->classes & CLI_TEXT_END_ASCII) != 0u)
    {
        hit |= args->loaded & MMGR_VERBUM_SCRUTOR_HIGH;
    }
    if ((args->classes & CLI_TEXT_UPPER) != 0u)
    {
        hit |= lane.ge(&(ScrutLaneCfg){.word = args->loaded, .byte = 'A'}) &
               lane.le(&(ScrutLaneCfg){.word = args->loaded, .byte = 'Z'});
    }
    if ((args->classes & CLI_TEXT_LOWER) != 0u)
    {
        hit |= lane.ge(&(ScrutLaneCfg){.word = args->loaded, .byte = 'a'}) &
               lane.le(&(ScrutLaneCfg){.word = args->loaded, .byte = 'z'});
    }
    return hit;
}

unsigned long long cli_text_to_class(const CliTextAtSizeClassesCfg *args)
{
    CliTextSet filled;
    EMBED_CALL(text_set_fill, TextCtx, .filled_out = &filled, .set = args->set);
    unsigned long long place = args->at;
    while (place < args->size)
    {
        const size_t bytes = ((args->size - place) < MMGR_SWAR_BYTES) ? (size_t)(args->size - place) : MMGR_SWAR_BYTES;
        embed_word loaded = 0u;
        memor.cpy(&(MemoriaCfg){.dst = &loaded, .src = args->text + place, .bytes = bytes});
        const embed_word hit =
            EMBED_CALL(text_word_of_class, TextCtx, .loaded = loaded, .classes = args->classes, .filled = &filled) &
            mask.lanes_below(&(ScrutMaskCfg){.bytes = bytes});
        if (hit != 0u)
        {
            return place + lane.first(&(ScrutLaneCfg){.mask = hit});
        }
        place += bytes;
    }
    return (args->at < args->size) ? args->size : args->at;
}

unsigned long long cli_text_end_class(const CliTextAtSizeClassesCfg *args)
{
    CliTextSet filled;
    EMBED_CALL(text_set_fill, TextCtx, .filled_out = &filled, .set = args->set);
    const embed_word every = mask.lanes_below(&(ScrutMaskCfg){.bytes = MMGR_SWAR_BYTES});
    unsigned long long place = args->at;
    while (place < args->size)
    {
        const size_t bytes = ((args->size - place) < MMGR_SWAR_BYTES) ? (size_t)(args->size - place) : MMGR_SWAR_BYTES;
        embed_word loaded = 0u;
        memor.cpy(&(MemoriaCfg){.dst = &loaded, .src = args->text + place, .bytes = bytes});
        const embed_word hit =
            every &
            ~EMBED_CALL(text_word_of_class, TextCtx, .loaded = loaded, .classes = args->classes, .filled = &filled) &
            mask.lanes_below(&(ScrutMaskCfg){.bytes = bytes});
        if (hit != 0u)
        {
            return place + lane.first(&(ScrutLaneCfg){.mask = hit});
        }
        place += bytes;
    }
    return (args->at < args->size) ? args->size : args->at;
}

unsigned long long cli_text_back_end_class(const CliTextAtSizeClassesCfg *args)
{
    CliTextSet filled;
    EMBED_CALL(text_set_fill, TextCtx, .filled_out = &filled, .set = args->set);
    const embed_word every = mask.lanes_below(&(ScrutMaskCfg){.bytes = MMGR_SWAR_BYTES});
    unsigned long long place = args->at;
    while (place != 0ULL)
    {
        const size_t bytes = (place < MMGR_SWAR_BYTES) ? (size_t)place : MMGR_SWAR_BYTES;
        embed_word loaded = 0u;
        memor.cpy(&(MemoriaCfg){.dst = &loaded, .src = args->text + (place - bytes), .bytes = bytes});
        const embed_word hit =
            every &
            ~EMBED_CALL(text_word_of_class, TextCtx, .loaded = loaded, .classes = args->classes, .filled = &filled) &
            mask.lanes_below(&(ScrutMaskCfg){.bytes = bytes});
        if (hit != 0u)
        {
            return (place - bytes) + lane.last(&(ScrutLaneCfg){.mask = hit}) + 1ULL;
        }
        place -= bytes;
    }
    return 0ULL;
}

unsigned long long cli_text_to_pair(const CliTextAtSizeFirstCfg *args)
{
    const char set[2] = {args->first, '\0'};
    unsigned long long place = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->text,
                                          .at = args->at, .size = args->size, .set = set);
    while (((place + 1ULL) < args->size) && (args->text[place + 1ULL] != args->second))
    {
        place = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->text,
                           .at = place + 1ULL, .size = args->size, .set = set);
    }
    return ((place + 1ULL) < args->size) ? place : args->size;
}

unsigned long long cli_text_number(const CliTextAtSizeCfg *args)
{
    unsigned long long *end = args->end;
    const unsigned long long stop =
        EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = args->text, .at = args->at,
                   .size = args->size, .classes = CLI_TEXT_DIGITS, .set = "");
    if (end != NULL)
    {
        *end = stop;
    }
    const unsigned long long digits = stop - args->at;
    if (digits > 19ULL)
    {
        return ~0ULL;
    }
    char read[20];
    memor.cpy(&(MemoriaCfg){.dst = read, .src = args->text + args->at, .bytes = (size_t)digits});
    read[digits] = '\0';
    return cellul.to_ulong(&(TransfiguroCfg){.src = read});
}

void cli_text_swap(const CliTextSizeFromCfg *args)
{
    // Explicit casts read the bytes as unsigned before they are repeated into every lane
    const embed_word required = MMGR_SWAR_ONES * (embed_word)(uint8_t)args->from;
    const embed_word flip = MMGR_SWAR_ONES * (embed_word)((uint8_t)args->from ^ (uint8_t)args->to);
    unsigned long long at = 0ULL;
    while (at < args->size)
    {
        const size_t bytes = ((args->size - at) < MMGR_SWAR_BYTES) ? (size_t)(args->size - at) : MMGR_SWAR_BYTES;
        embed_word loaded = 0u;
        memor.cpy(&(MemoriaCfg){.dst = &loaded, .src = args->text_out + at, .bytes = bytes});
        const embed_word hit = lane.has_zero(&(ScrutLaneCfg){.word = loaded ^ required}) &
                               mask.lanes_below(&(ScrutMaskCfg){.bytes = bytes});
        if (hit != 0u)
        {
            loaded ^= mask.spread(&(ScrutMaskCfg){.mask = hit}) & flip;
            memor.cpy(&(MemoriaCfg){.dst = args->text_out + at, .src = &loaded, .bytes = bytes});
        }
        at += bytes;
    }
}

void cli_text_lower(const CliTextSizeFromCfg *args)
{
    unsigned long long at = 0ULL;
    while (at < args->size)
    {
        const size_t bytes = ((args->size - at) < MMGR_SWAR_BYTES) ? (size_t)(args->size - at) : MMGR_SWAR_BYTES;
        embed_word loaded = 0u;
        memor.cpy(&(MemoriaCfg){.dst = &loaded, .src = args->text_out + at, .bytes = bytes});
        const embed_word folded = word.fold_lower(&(ScrutWordCfg){.word = loaded});
        memor.cpy(&(MemoriaCfg){.dst = args->text_out + at, .src = &folded, .bytes = bytes});
        at += bytes;
    }
}

void cli_text_raise(const CliTextSizeFromCfg *args)
{
    const embed_word flip = MMGR_SWAR_ONES * (embed_word)0x20u;
    unsigned long long at = 0ULL;
    while (at < args->size)
    {
        const size_t bytes = ((args->size - at) < MMGR_SWAR_BYTES) ? (size_t)(args->size - at) : MMGR_SWAR_BYTES;
        embed_word loaded = 0u;
        memor.cpy(&(MemoriaCfg){.dst = &loaded, .src = args->text_out + at, .bytes = bytes});
        const embed_word hit = lane.ge(&(ScrutLaneCfg){.word = loaded, .byte = 'a'}) &
                               lane.le(&(ScrutLaneCfg){.word = loaded, .byte = 'z'});
        loaded ^= mask.spread(&(ScrutMaskCfg){.mask = hit}) & flip;
        memor.cpy(&(MemoriaCfg){.dst = args->text_out + at, .src = &loaded, .bytes = bytes});
        at += bytes;
    }
}

unsigned long long cli_text_lowered_into(const CliTextOutCapacityCfg *args)
{
    const unsigned long long whole = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL});
    const unsigned long long length = (whole < args->capacity) ? whole : (args->capacity - 1ULL);
    memor.cpy(&(MemoriaCfg){.dst = args->out_capacity, .src = args->text, .bytes = (size_t)length});
    args->out_capacity[length] = '\0';
    EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .text_out = args->out_capacity, .size = length);
    return length;
}

CliTextLine cli_text_line_at(const CliTextAtCfg *args)
{
    const char *const newline =
        memor.chr(&(MemoriaCfg){.src = args->text + args->at, .val = '\n', .bytes = args->size - args->at});
    if (newline == NULL)
    {
        return (CliTextLine){args->size, args->size, args->size};
    }
    // a newline found lies in the text, at or past `at`
    const unsigned long long stop = (unsigned long long)(newline - args->text);
    return (CliTextLine){stop, ((stop > args->at) && (args->text[stop - 1ULL] == '\r')) ? stop - 1ULL : stop,
                         stop + 1ULL};
}

void cli_text_copied_or(const CliTextOutCfg *args)
{
    const char *const from = (args->text != NULL) ? args->text : args->otherwise;
    memor.cpy(&(MemoriaCfg){.dst = args->out_capacity,
                            .src = from,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}) + 1u});
}

void cli_text_save(const CliTextSizeCfg *args)
{
    for (unsigned long long at =
             EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = args->text_out, .at = 0ULL,
                        .size = args->size, .classes = args->classes, .set = args->set);
         at < args->size;
         at = EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = args->text_out,
                         .at = at + 1ULL, .size = args->size, .classes = args->classes, .set = args->set))
    {
        args->text_out[at] = args->other;
    }
}

unsigned long long cli_text_count(const CliTextSizeByteCfg *args)
{
    // Explicit cast reads the byte as unsigned before it is repeated into every lane
    const embed_word required = MMGR_SWAR_ONES * (embed_word)(uint8_t)args->byte;
    unsigned long long count = 0ULL;
    unsigned long long at = 0ULL;
    while (at < args->size)
    {
        const size_t bytes = ((args->size - at) < MMGR_SWAR_BYTES) ? (size_t)(args->size - at) : MMGR_SWAR_BYTES;
        embed_word loaded = 0u;
        memor.cpy(&(MemoriaCfg){.dst = &loaded, .src = args->text + at, .bytes = bytes});
        const embed_word hit = lane.has_zero(&(ScrutLaneCfg){.word = loaded ^ required}) &
                               mask.lanes_below(&(ScrutMaskCfg){.bytes = bytes});
        count += lane.count(&(ScrutLaneCfg){.mask = hit});
        at += bytes;
    }
    return count;
}

int cli_text_ascii(const CliTextSizeByteCfg *args)
{
    unsigned long long at = 0ULL;
    while (at < args->size)
    {
        const size_t bytes = ((args->size - at) < MMGR_SWAR_BYTES) ? (size_t)(args->size - at) : MMGR_SWAR_BYTES;
        embed_word loaded = 0u;
        memor.cpy(&(MemoriaCfg){.dst = &loaded, .src = args->text + at, .bytes = bytes});
        if ((loaded & MMGR_VERBUM_SCRUTOR_HIGH) != 0u)
        {
            return 0;
        }
        at += bytes;
    }
    return 1;
}

unsigned long long cli_text_append_swap(const CliTextFromCfg *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL});
    const size_t start = args->out->at;
    const size_t reserved = mmgr_line_put_n(args->out, args->text, (size_t)size);
    if ((args->out->out != NULL) && (reserved != 0u))
    {
        EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = args->out->out + start, .size = reserved,
                   .from = args->from, .to = args->to);
    }
    return size;
}

void cli_text_append_mark(const CliTextSizeSetCfg *args)
{
    EMBED_CALL(cli_text_size_set.append_marked_n, CliTextSizeSetCfg, .out = args->out, .text = args->text,
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL}), .set = args->set,
               .mark = args->mark);
}

void cli_text_append_marked_n(const CliTextSizeSetCfg *args)
{
    unsigned long long at = 0ULL;
    while (at < args->size)
    {
        const unsigned long long stop = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                   .text = args->text, .at = at, .size = args->size, .set = args->set);
        mmgr_line_put_n(args->out, args->text + at, (size_t)(stop - at));
        if (stop < args->size)
        {
            mmgr_line_ch(args->out, args->mark);
            mmgr_line_ch(args->out, args->text[stop]);
        }
        at = stop + 1ULL;
    }
}

void cli_text_append_double(const CliTextSetCfg *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL});
    unsigned long long at = 0ULL;
    while (at < size)
    {
        const unsigned long long stop = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                   .text = args->text, .at = at, .size = size, .set = args->set);
        mmgr_line_put_n(args->out, args->text + at, (size_t)(stop - at));
        if (stop < size)
        {
            mmgr_line_ch(args->out, args->text[stop]);
            mmgr_line_ch(args->out, args->text[stop]);
        }
        at = stop + 1ULL;
    }
}

void cli_text_append_lower(const CliTextOutNameCfg *args)
{
    const size_t start = args->out->at;
    const size_t reserved = mmgr_line_put_n(args->out, args->text,
                                            cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL}));
    if ((args->out->out != NULL) && (reserved != 0u))
    {
        EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .text_out = args->out->out + start, .size = reserved);
    }
}

void cli_text_append_save(const CliTextClassesCfg *args)
{
    const size_t start = args->out->at;
    const size_t reserved = mmgr_line_put_n(args->out, args->text,
                                            cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL}));
    if ((args->out->out != NULL) && (reserved != 0u))
    {
        EMBED_CALL(cli_text_size.save, CliTextSizeCfg, .text_out = args->out->out + start, .size = reserved,
                   .classes = args->classes, .set = args->set, .other = args->other);
    }
}

// the value of a hexadecimal digit, 16 where the byte is none
EMBED_INLINE unsigned int text_hex_get(const TextCtx *args)
{
    if ((args->byte >= '0') && (args->byte <= '9'))
    {
        return (unsigned int)(args->byte - '0');
    }
    if ((args->byte >= 'a') && (args->byte <= 'f'))
    {
        return (unsigned int)(args->byte - 'a') + 10u;
    }
    if ((args->byte >= 'A') && (args->byte <= 'F'))
    {
        return (unsigned int)(args->byte - 'A') + 10u;
    }
    return 16u;
}

void cli_text_append_unescaped(const CliTextSizeSetCfg *args)
{
    unsigned long long at = 0ULL;
    while (at < args->size)
    {
        const unsigned long long stop = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                   .text = args->text, .at = at, .size = args->size, .set = "%");
        mmgr_line_put_n(args->out, args->text + at, (size_t)(stop - at));
        at = stop;
        if (stop < args->size)
        {
            const unsigned int high =
                ((stop + 1ULL) < args->size) ? EMBED_CALL(text_hex_get, TextCtx, .byte = args->text[stop + 1ULL]) : 16u;
            const unsigned int low =
                ((stop + 2ULL) < args->size) ? EMBED_CALL(text_hex_get, TextCtx, .byte = args->text[stop + 2ULL]) : 16u;
            const int escaped = (high < 16u) && (low < 16u);
            // two hexadecimal digits make a byte, below 256
            mmgr_line_ch(args->out, escaped ? (char)((high << 4u) | low) : '%');
            at += escaped ? 3ULL : 1ULL;
        }
    }
}

void cli_text_append_replace(const CliTextSetCfg *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL});
    unsigned long long at = 0ULL;
    while (at < size)
    {
        const unsigned long long stop = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                   .text = args->text, .at = at, .size = size, .set = args->set);
        mmgr_line_put_n(args->out, args->text + at, (size_t)(stop - at));
        if (stop < size)
        {
            mmgr_line_put(args->out, args->with);
        }
        at = stop + 1ULL;
    }
}

void cli_text_append_fill(const CliTextOutNameCfg *args)
{
    const size_t size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL});
    const size_t name_size = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL});
    const char *at = args->text;
    const char *const end = args->text + size;
    while (at < end)
    {
        const char *const found = cellul.find(&(CatenaFinitaCfg){
            .src = at, .cap = (size_t)(end - at) + 1u, .other = args->name, .other_cap = name_size + 1u});
        const char *const stop = (found != NULL) ? found : end;
        // a name found lies in the text, at or past `at`
        mmgr_line_put_n(args->out, at, (size_t)(stop - at));
        if (found == NULL)
        {
            return;
        }
        mmgr_line_put(args->out, args->value);
        at = found + name_size;
    }
}

unsigned long long cli_text_prefix(const CliTextSizeFromU64sCfg *args)
{
    unsigned long long at = 0ULL;
    int going = 1;
    while (going && (at < args->size))
    {
        at = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = args->text, .at = at,
                        .size = args->size, .set = CLI_TEXT_WHITE);
        if (at >= args->size)
        {
            going = 0;
            continue;
        }
        unsigned long long length = 0ULL;
        if (!EMBED_CALL(cli_text_table.white, CliTextCfg,
                        .letter = EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = args->text, .size = args->size,
                                             .at = at, .length = &length)))
        {
            going = 0;
            continue;
        }
        at += length;
    }
    return at;
}

unsigned long long cli_text_trimmed_end(const CliTextSizeByteCfg *args)
{
    unsigned long long end = args->size;
    while (end > 0u)
    {
        end = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = args->text, .at = end,
                         .set = CLI_TEXT_WHITE);
        if (end == 0ULL)
        {
            break;
        }
        unsigned long long length = 0ULL;
        if (!EMBED_CALL(cli_text_table.white, CliTextCfg,
                        .letter = EMBED_CALL(cli_text_length.letter_before, CliTextLengthCfg, .text = args->text,
                                             .end_value = end, .length = &length)))
        {
            break;
        }
        end -= length;
    }
    return end;
}

void cli_text_trim(const CliTextSizeFromU64sCfg *args)
{
    unsigned long long *from = args->from_u64s;
    unsigned long long *to = args->to_u64s;
    *from = cli_text_size_from_u_64s.lead(args);
    *to = *from + EMBED_CALL(cli_text_size_byte.trimmed_end, CliTextSizeByteCfg, .text = args->text + *from,
                             .size = args->size - *from);
}

int cli_text_word(const CliTextSizeLengthCfg *args)
{
    unsigned long long *at = args->at_pointer;
    unsigned long long *word_text = args->word_text;
    unsigned long long *length = args->length;
    *at += EMBED_CALL(cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg, .text = args->text + *at,
                      .size = args->size - *at);
    if (*at >= args->size)
    {
        return 0;
    }
    *word_text = *at;
    while (*at < args->size)
    {
        unsigned long long letter = 0ULL;
        if (EMBED_CALL(cli_text_table.white, CliTextCfg,
                       .letter = EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = args->text, .size = args->size,
                                            .at = *at, .length = &letter)))
        {
            break;
        }
        *at += letter;
    }
    *length = *at - *word_text;
    return 1;
}

unsigned long long cli_text_letters(const CliTextSizeByteCfg *args)
{
    unsigned long long count = 0ULL;
    for (unsigned long long at = 0ULL; at < args->size;)
    {
        unsigned long long length = 0ULL;
        (void)EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = args->text, .size = args->size, .at = at,
                         .length = &length);
        at += length;
        count += 1u;
    }
    return count;
}

unsigned long long cli_text_units(const CliTextSizeByteCfg *args)
{
    unsigned long long count = 0ULL;
    for (unsigned long long at = 0ULL; at < args->size;)
    {
        unsigned long long length = 0ULL;
        const unsigned int letter = EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = args->text, .size = args->size,
                                               .at = at, .length = &length);
        at += length;
        count += (letter >= 0x10000u) ? 2u : 1u;
    }
    return count;
}

int cli_text_starts(const CliTextSizePrefixCfg *args)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->prefix, .cap = CLI_CELLS_POOL});
    return (length <= args->size) &&
           (cellul.diff(&(CatenaFinitaCfg){.src = args->text, .other = args->prefix, .cap = length}) == length);
}

int cli_text_ends(const CliTextSizeSuffixCfg *args)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->suffix, .cap = CLI_CELLS_POOL});
    return (length <= args->size) &&
           (cellul.diff(&(CatenaFinitaCfg){
                .src = args->text + args->size - length, .other = args->suffix, .cap = length}) == length);
}

int cli_text_find(const CliTextSizeAtCfg *args)
{
    unsigned long long *at = args->at_pointer;
    // a wanted text of no bytes is at the start; find would measure one of 0 as terminated
    const char *const found = (args->size == 0ULL) ? args->text
                                                   : cellul.find(&(CatenaFinitaCfg){.src = args->text,
                                                                                    .cap = (size_t)args->length_value,
                                                                                    .other = args->required,
                                                                                    .other_cap = (size_t)args->size,
                                                                                    .other_len = (size_t)args->size});
    if (found == NULL)
    {
        return 0;
    }
    *at = (unsigned long long)(found - args->text);
    return 1;
}

int cli_text_quoted_end(const CliTextAtSizeCfg *args)
{
    unsigned long long *end = args->end;
    const char *const rest = args->text + args->at;
    const unsigned long long left = args->size - args->at;
    unsigned long long found = 0ULL;
    if (EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = rest, .size = left, .prefix = "\"\"\"") ||
        EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = rest, .size = left, .prefix = "'''"))
    {
        *end = EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = rest + 3u, .length_value = left - 3u,
                          .required = rest, .size = 3u, .at_pointer = &found)
                   ? args->at + 3u + found + 3u
                   : args->size;
        return 1;
    }
    if ((left == 0ULL) || ((rest[0] != '"') && (rest[0] != '\'')))
    {
        return 0;
    }
    unsigned long long walk = args->at + 1u;
    while ((walk < args->size) && (args->text[walk] != rest[0]) && (args->text[walk] != '\n'))
    {
        walk += (args->text[walk] == '\\') ? 2u : 1u;
    }
    *end = (walk + 1u < args->size) ? walk + 1u : args->size;
    return 1;
}

int cli_text_find_last(const CliTextSizeAtCfg *args)
{
    unsigned long long *at = args->at_pointer;
    if (args->size > args->length_value)
    {
        return 0;
    }
    if (args->size == 0ULL)
    {
        *at = args->length_value;
        return 1;
    }
    // the places the wanted text can start at, its first byte found by the word scan from the last of them back, and
    // the whole compared only where that byte stands
    unsigned long long positions = args->length_value - args->size + 1ULL;
    for (const char *start = EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->text,
                                        .length_value = positions, .byte = args->required[0]);
         start != NULL; start = EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->text,
                                           .length_value = positions, .byte = args->required[0]))
    {
        if (cellul.diff(&(CatenaFinitaCfg){.src = start, .other = args->required, .cap = (size_t)args->size}) ==
            (size_t)args->size)
        {
            *at = (unsigned long long)(start - args->text);
            return 1;
        }
        positions = (unsigned long long)(start - args->text);
    }
    return 0;
}

int cli_text_order(const CliTextSizeOneCfg *args)
{
    const char *other = args->other_pointer;
    // `one` with its terminator: `other` differs from it there at the latest, so the first difference is the order
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->one, .cap = CLI_CELLS_POOL}) + 1u;
    const embed_iword order = memor.cmp(&(MemoriaCfg){.src = args->one, .other = other, .bytes = length});
    return (order > 0) - (order < 0);
}

void cli_paths_sort(const CliTextPathsCfg *args)
{
    for (unsigned int at = 1u; at < args->count; at += 1u)
    {
        char pivot[CLI_PATH_MAX_LEN];
        memor.cpy(
            &(MemoriaCfg){.dst = pivot,
                          .src = args->paths[at],
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->paths[at], .cap = CLI_CELLS_POOL}) + 1u});
        unsigned int place = at;
        while ((place > 0u) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = args->paths[place - 1u],
                                           .other_pointer = pivot) > 0))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = args->paths[place],
                .src = args->paths[place - 1u],
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->paths[place - 1u], .cap = CLI_CELLS_POOL}) + 1u});
            place -= 1u;
        }
        memor.cpy(&(MemoriaCfg){.dst = args->paths[place],
                                .src = pivot,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = pivot, .cap = CLI_CELLS_POOL}) + 1u});
    }
}

int cli_text_utf8_valid(const CliTextSizeKindCfg *args)
{
    const unsigned char *text = args->text_bytes;
    unsigned long long at = 0ULL;
    while (at < args->size)
    {
        if (((at + MMGR_SWAR_BYTES) <= args->size) &&
            ((word.load(&(ScrutWordCfg){.at = text + at}) & MMGR_VERBUM_SCRUTOR_HIGH) == 0u))
        {
            at += MMGR_SWAR_BYTES;
            continue;
        }
        unsigned int letter = 0u;
        const unsigned long long step =
            EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg, .text_bytes = text,
                       .length = args->size, .at = at, .letter = &letter);
        if ((step == 1ULL) && (text[at] >= 0x80u))
        {
            return 0;
        }
        at += step;
    }
    return 1;
}

int cli_text_same(const CliTextSizeOneCfg *args)
{
    const char *other = args->other_pointer;
    return cellul.diff(&(CatenaFinitaCfg){.src = args->one, .other = other, .cap = (size_t)args->size}) ==
           (size_t)args->size;
}

// Walks the aligned words the text stands in from the one containing its first byte, a word at a time: each word tested
// against every byte of the set in every lane at once and the lanes the set contains joined, the lanes before the text
// masked off, and the first lane that stops the run taken: its terminator, or a byte of the set where `in_set` and
// one outside it where not. Every load is an aligned word containing a byte of the text, so nothing outside those words
// is read
EMBED_INLINE unsigned long long text_run(const TextCtx *args)
{
    // Explicit casts read the addresses as integers, so a word's place and the text's start compare as numbers
    const uintptr_t first = (uintptr_t)args->text;
    uintptr_t word_at = first & ~(uintptr_t)(MMGR_SWAR_BYTES - 1u);
    embed_word before = mask.lanes_below(&(ScrutMaskCfg){.bytes = (size_t)(first - word_at)});
    for (;;)
    {
        const embed_word loaded = word.load_al(&(ScrutWordCfg){.at = (const void *)word_at});
        embed_word match_mask = 0u;
        for (const char *letter = args->set; *letter != '\0'; letter += 1)
        {
            // Explicit cast reads the set's byte as unsigned before it is repeated into every lane
            match_mask |= lane.eq(&(ScrutLaneCfg){.word = loaded, .byte = (uint8_t)*letter});
        }
        const embed_word ended = lane.has_zero(&(ScrutLaneCfg){.word = loaded});
        const embed_word stops =
            (ended | (args->in_set ? match_mask : (~match_mask & MMGR_VERBUM_SCRUTOR_HIGH))) & ~before;
        if (stops != 0u)
        {
            return (unsigned long long)(word_at + lane.first(&(ScrutLaneCfg){.mask = stops}) - first);
        }
        word_at += MMGR_SWAR_BYTES;
        before = 0u;
    }
}

unsigned long long cli_text_upto(const CliTextSetCfg *args)
{
    return EMBED_CALL(text_run, TextCtx, .text = args->text, .set = args->set, .in_set = 1);
}

unsigned long long cli_text_over(const CliTextSetCfg *args)
{
    return EMBED_CALL(text_run, TextCtx, .text = args->text, .set = args->set, .in_set = 0);
}

// Walks the aligned words the text stands in, from the one containing its last byte back to the one containing its
// first, a word at a time: each word tested against the byte in every lane at once, its lanes outside the text masked
// off, and the last lane that contains the byte taken. Every load is an aligned word containing a byte of the text, so
// nothing outside those words is read
const char *cli_text_last(const CliTextByteCfg *args)
{
    if (args->length_value == 0ULL)
    {
        return NULL;
    }
    // Explicit casts read the addresses as integers, so a word's place and the text's bounds compare as numbers
    const uintptr_t first = (uintptr_t)args->text;
    const uintptr_t end = first + (uintptr_t)args->length_value;
    const embed_word every = mask.lanes_below(&(ScrutMaskCfg){.bytes = MMGR_SWAR_BYTES});
    // Explicit cast reads the byte as unsigned before it is repeated into every lane
    const embed_word broadcast = MMGR_SWAR_ONES * (embed_word)(uint8_t)args->byte;
    uintptr_t word_at = (end - 1u) & ~(uintptr_t)(MMGR_SWAR_BYTES - 1u);
    embed_word below_mask = mask.lanes_below(&(ScrutMaskCfg){.bytes = (size_t)(end - word_at)});
    for (;;)
    {
        if (word_at < first)
        {
            below_mask &= ~mask.lanes_below(&(ScrutMaskCfg){.bytes = (size_t)(first - word_at)});
        }
        const embed_word loaded = word.load_al(&(ScrutWordCfg){.at = (const void *)word_at});
        const embed_word hit = lane.has_zero(&(ScrutLaneCfg){.word = loaded ^ broadcast}) & below_mask;
        if (hit != 0u)
        {
            return (const char *)(word_at + lane.last(&(ScrutLaneCfg){.mask = hit}));
        }
        if (word_at <= first)
        {
            return NULL;
        }
        word_at -= MMGR_SWAR_BYTES;
        below_mask = every;
    }
}
