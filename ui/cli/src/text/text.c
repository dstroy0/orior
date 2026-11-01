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

unsigned int cli_text_letter(const char *text, unsigned long long size, unsigned long long at,
                             unsigned long long *length)
{
    const unsigned char lead = (unsigned char)text[at];
    const unsigned long long want = (lead < 0xc0u) ? 1u : (lead < 0xe0u) ? 2u : (lead < 0xf0u) ? 3u : 4u;
    if ((want == 1u) || (at + want > size))
    {
        *length = 1u;
        return lead;
    }
    unsigned int letter = lead & ((want == 2u) ? 0x1fu : (want == 3u) ? 0x0fu : 0x07u);
    for (unsigned long long next = 1ull; next < want; next += 1u)
    {
        const unsigned char more = (unsigned char)text[at + next];
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

unsigned int cli_text_letter_before(const char *text, unsigned long long end, unsigned long long *length)
{
    unsigned long long start = end - 1u;
    while ((start > 0u) && (end - start < 4u) && (((unsigned char)text[start] & 0xc0u) == 0x80u))
    {
        start -= 1u;
    }
    unsigned long long read = 0ull;
    const unsigned int letter = cli_text_letter(text, end, start, &read);
    if (start + read != end)
    {
        *length = 1u;
        return (unsigned char)text[end - 1u];
    }
    *length = read;
    return letter;
}

int cli_text_white(unsigned int letter)
{
    return (letter == ' ') || ((letter >= '\t') && (letter <= '\r')) || (letter == 0x85u) || (letter == 0xA0u) ||
           (letter == 0x1680u) || ((letter >= 0x2000u) && (letter <= 0x200Au)) || (letter == 0x2028u) ||
           (letter == 0x2029u) || (letter == 0x202Fu) || (letter == 0x205Fu) || (letter == 0x3000u);
}

int cli_text_alphabetic(unsigned int letter)
{
    if (letter < 0x80u)
    {
        return ((letter >= 'a') && (letter <= 'z')) || ((letter >= 'A') && (letter <= 'Z'));
    }
    return cli_regexp_property(CLI_REGEXP_LETTER, letter);
}

int cli_text_alphanumeric(unsigned int letter)
{
    if (letter < 0x80u)
    {
        return cli_text_alphabetic(letter) || ((letter >= '0') && (letter <= '9'));
    }
    return cli_regexp_property(CLI_REGEXP_LETTER, letter) || cli_regexp_property(CLI_REGEXP_NUMBER, letter);
}

// the most bytes of a set a walk tests for
#define CLI_TEXT_SET_MOST 16u

// A set's bytes each filled across a word, and across a lane where the build has one
typedef struct
{
    unsigned int count;
    embed_word word[CLI_TEXT_SET_MOST];
#if MMGR_VECTOR_BYTES != 0u
    MmgrVector lane[CLI_TEXT_SET_MOST];
#endif
} CliTextSet;

static void text_set_filled(CliTextSet *filled, const char *set)
{
    const size_t count = cellul.len(&(CatenaFinitaCfg){.src = set, .cap = CLI_TEXT_SET_MOST});
    // a set is at most CLI_TEXT_SET_MOST bytes, which an unsigned int stores
    filled->count = (unsigned int)count;
    for (unsigned int at = 0u; at < filled->count; at += 1u)
    {
        // Explicit cast reads the byte as unsigned before it is repeated into every lane
        const uint8_t byte = (uint8_t)set[at];
        filled->word[at] = MMGR_SWAR_ONES * (embed_word)byte;
#if MMGR_VECTOR_BYTES != 0u
        filled->lane[at] = mmgr_vector_fill(byte);
#endif
    }
}

// the lanes of a word that the set holds
static embed_word text_word_in(embed_word loaded, const CliTextSet *filled)
{
    embed_word hit = 0u;
    for (unsigned int at = 0u; at < filled->count; at += 1u)
    {
        hit |= lane.has_zero(&(ScrutLaneCfg){.word = loaded ^ filled->word[at]});
    }
    return hit;
}

// the first place from `at` below `size` whose byte the set holds where `in`, or does not hold where not, else `size`,
// the widest lane first, then a word at a time, and the last bytes loaded into a word of their own
static unsigned long long text_walked(const char *text, unsigned long long at, unsigned long long size, const char *set,
                                      int in)
{
    if (at >= size)
    {
        return at;
    }
    // Explicit cast reads the text's bytes as unsigned
    const uint8_t *const bytes = (const uint8_t *)text;
    CliTextSet filled;
    text_set_filled(&filled, set);
    unsigned long long place = at;
#if MMGR_VECTOR_BYTES != 0u
    while ((size - place) >= MMGR_VECTOR_BYTES)
    {
        const MmgrVector loaded = mmgr_vector_load(bytes + place);
        MmgrVectorMask in_lanes = 0u;
        for (unsigned int one = 0u; one < filled.count; one += 1u)
        {
            in_lanes |= mmgr_vector_equal_lanes(loaded, filled.lane[one]);
        }
        const MmgrVectorMask hit = in ? in_lanes : (MmgrVectorMask)(EMBED_LANE_MASK_ALL & ~in_lanes);
        if (hit != 0u)
        {
            return place + mmgr_vector_mask_first(hit);
        }
        place += MMGR_VECTOR_BYTES;
    }
#endif
    const embed_word every = mask.lanes_below(&(ScrutMaskCfg){.bytes = MMGR_SWAR_BYTES});
    while ((size - place) >= MMGR_SWAR_BYTES)
    {
        const embed_word found = text_word_in(word.load(&(ScrutWordCfg){.at = bytes + place}), &filled);
        const embed_word hit = in ? found : (every & ~found);
        if (hit != 0u)
        {
            return place + lane.first(&(ScrutLaneCfg){.mask = hit});
        }
        place += MMGR_SWAR_BYTES;
    }
    if (place < size)
    {
        embed_word loaded = 0u;
        memor.cpy(&(MemoriaCfg){.dst = &loaded, .src = bytes + place, .bytes = (size_t)(size - place)});
        const embed_word found = text_word_in(loaded, &filled);
        const embed_word hit =
            (in ? found : (every & ~found)) & mask.lanes_below(&(ScrutMaskCfg){.bytes = (size_t)(size - place)});
        if (hit != 0u)
        {
            return place + lane.first(&(ScrutLaneCfg){.mask = hit});
        }
    }
    return size;
}

// the place past the last byte before `at` whose byte the set holds where `in`, or does not hold where not, else 0, a
// word at a time from the end, the first bytes loaded into a word of their own
static unsigned long long text_walked_back(const char *text, unsigned long long at, const char *set, int in)
{
    // Explicit cast reads the text's bytes as unsigned
    const uint8_t *const bytes = (const uint8_t *)text;
    CliTextSet filled;
    text_set_filled(&filled, set);
    const embed_word every = mask.lanes_below(&(ScrutMaskCfg){.bytes = MMGR_SWAR_BYTES});
    unsigned long long place = at;
    while (place >= MMGR_SWAR_BYTES)
    {
        const embed_word found =
            text_word_in(word.load(&(ScrutWordCfg){.at = bytes + (place - MMGR_SWAR_BYTES)}), &filled);
        const embed_word hit = in ? found : (every & ~found);
        if (hit != 0u)
        {
            return (place - MMGR_SWAR_BYTES) + lane.last(&(ScrutLaneCfg){.mask = hit}) + 1ull;
        }
        place -= MMGR_SWAR_BYTES;
    }
    if (place != 0ull)
    {
        embed_word loaded = 0u;
        memor.cpy(&(MemoriaCfg){.dst = &loaded, .src = bytes, .bytes = (size_t)place});
        const embed_word found = text_word_in(loaded, &filled);
        const embed_word hit =
            (in ? found : (every & ~found)) & mask.lanes_below(&(ScrutMaskCfg){.bytes = (size_t)place});
        if (hit != 0u)
        {
            return lane.last(&(ScrutLaneCfg){.mask = hit}) + 1ull;
        }
    }
    return 0ull;
}

unsigned long long cli_text_to_any(const char *text, unsigned long long at, unsigned long long size, const char *set)
{
    return text_walked(text, at, size, set, 1);
}

unsigned long long cli_text_past_any(const char *text, unsigned long long at, unsigned long long size, const char *set)
{
    return text_walked(text, at, size, set, 0);
}

unsigned long long cli_text_back_any(const char *text, unsigned long long at, const char *set)
{
    return text_walked_back(text, at, set, 1);
}

unsigned long long cli_text_back_past(const char *text, unsigned long long at, const char *set)
{
    return text_walked_back(text, at, set, 0);
}

// the lanes of a word that hold a byte of `classes` or of the set
static embed_word text_word_of_class(embed_word loaded, unsigned int classes, const CliTextSet *filled)
{
    embed_word hit = text_word_in(loaded, filled);
    if ((classes & CLI_TEXT_LETTERS) != 0u)
    {
        hit |= lane.alpha(&(ScrutLaneCfg){.word = loaded});
    }
    if ((classes & CLI_TEXT_DIGITS) != 0u)
    {
        hit |= lane.ge(&(ScrutLaneCfg){.word = loaded, .byte = '0'}) &
               lane.le(&(ScrutLaneCfg){.word = loaded, .byte = '9'});
    }
    if ((classes & CLI_TEXT_PAST_ASCII) != 0u)
    {
        hit |= loaded & MMGR_VERBUM_SCRUTOR_HIGH;
    }
    if ((classes & CLI_TEXT_UPPER) != 0u)
    {
        hit |= lane.ge(&(ScrutLaneCfg){.word = loaded, .byte = 'A'}) &
               lane.le(&(ScrutLaneCfg){.word = loaded, .byte = 'Z'});
    }
    if ((classes & CLI_TEXT_LOWER) != 0u)
    {
        hit |= lane.ge(&(ScrutLaneCfg){.word = loaded, .byte = 'a'}) &
               lane.le(&(ScrutLaneCfg){.word = loaded, .byte = 'z'});
    }
    return hit;
}

unsigned long long cli_text_past_class(const char *text, unsigned long long at, unsigned long long size,
                                       unsigned int classes, const char *set)
{
    CliTextSet filled;
    text_set_filled(&filled, set);
    const embed_word every = mask.lanes_below(&(ScrutMaskCfg){.bytes = MMGR_SWAR_BYTES});
    unsigned long long place = at;
    while (place < size)
    {
        const size_t bytes = ((size - place) < MMGR_SWAR_BYTES) ? (size_t)(size - place) : MMGR_SWAR_BYTES;
        embed_word loaded = 0u;
        memor.cpy(&(MemoriaCfg){.dst = &loaded, .src = text + place, .bytes = bytes});
        const embed_word hit =
            every & ~text_word_of_class(loaded, classes, &filled) & mask.lanes_below(&(ScrutMaskCfg){.bytes = bytes});
        if (hit != 0u)
        {
            return place + lane.first(&(ScrutLaneCfg){.mask = hit});
        }
        place += bytes;
    }
    return (at < size) ? size : at;
}

unsigned long long cli_text_back_past_class(const char *text, unsigned long long at, unsigned int classes,
                                            const char *set)
{
    CliTextSet filled;
    text_set_filled(&filled, set);
    const embed_word every = mask.lanes_below(&(ScrutMaskCfg){.bytes = MMGR_SWAR_BYTES});
    unsigned long long place = at;
    while (place != 0ull)
    {
        const size_t bytes = (place < MMGR_SWAR_BYTES) ? (size_t)place : MMGR_SWAR_BYTES;
        embed_word loaded = 0u;
        memor.cpy(&(MemoriaCfg){.dst = &loaded, .src = text + (place - bytes), .bytes = bytes});
        const embed_word hit =
            every & ~text_word_of_class(loaded, classes, &filled) & mask.lanes_below(&(ScrutMaskCfg){.bytes = bytes});
        if (hit != 0u)
        {
            return (place - bytes) + lane.last(&(ScrutLaneCfg){.mask = hit}) + 1ull;
        }
        place -= bytes;
    }
    return 0ull;
}

unsigned long long cli_text_to_pair(const char *text, unsigned long long at, unsigned long long size, char first,
                                    char second)
{
    const char set[2] = {first, '\0'};
    unsigned long long place = cli_text_to_any(text, at, size, set);
    while (((place + 1ull) < size) && (text[place + 1ull] != second))
    {
        place = cli_text_to_any(text, place + 1ull, size, set);
    }
    return ((place + 1ull) < size) ? place : size;
}

unsigned long long cli_text_number(const char *text, unsigned long long at, unsigned long long size,
                                   unsigned long long *end)
{
    const unsigned long long stop = cli_text_past_class(text, at, size, CLI_TEXT_DIGITS, "");
    if (end != NULL)
    {
        *end = stop;
    }
    const unsigned long long digits = stop - at;
    if (digits > 19ull)
    {
        return ~0ull;
    }
    char read[20];
    memor.cpy(&(MemoriaCfg){.dst = read, .src = text + at, .bytes = (size_t)digits});
    read[digits] = '\0';
    return cellul.to_ulong(&(TransfiguroCfg){.src = read});
}

void cli_text_swapped(char *text, unsigned long long size, char from, char to)
{
    // Explicit casts read the bytes as unsigned before they are repeated into every lane
    const embed_word wanted = MMGR_SWAR_ONES * (embed_word)(uint8_t)from;
    const embed_word flip = MMGR_SWAR_ONES * (embed_word)((uint8_t)from ^ (uint8_t)to);
    unsigned long long at = 0ull;
    while (at < size)
    {
        const size_t bytes = ((size - at) < MMGR_SWAR_BYTES) ? (size_t)(size - at) : MMGR_SWAR_BYTES;
        embed_word loaded = 0u;
        memor.cpy(&(MemoriaCfg){.dst = &loaded, .src = text + at, .bytes = bytes});
        const embed_word hit =
            lane.has_zero(&(ScrutLaneCfg){.word = loaded ^ wanted}) & mask.lanes_below(&(ScrutMaskCfg){.bytes = bytes});
        if (hit != 0u)
        {
            loaded ^= mask.spread(&(ScrutMaskCfg){.mask = hit}) & flip;
            memor.cpy(&(MemoriaCfg){.dst = text + at, .src = &loaded, .bytes = bytes});
        }
        at += bytes;
    }
}

void cli_text_lowered(char *text, unsigned long long size)
{
    unsigned long long at = 0ull;
    while (at < size)
    {
        const size_t bytes = ((size - at) < MMGR_SWAR_BYTES) ? (size_t)(size - at) : MMGR_SWAR_BYTES;
        embed_word loaded = 0u;
        memor.cpy(&(MemoriaCfg){.dst = &loaded, .src = text + at, .bytes = bytes});
        const embed_word folded = word.fold_lower(&(ScrutWordCfg){.word = loaded});
        memor.cpy(&(MemoriaCfg){.dst = text + at, .src = &folded, .bytes = bytes});
        at += bytes;
    }
}

void cli_text_raised(char *text, unsigned long long size)
{
    const embed_word flip = MMGR_SWAR_ONES * (embed_word)0x20u;
    unsigned long long at = 0ull;
    while (at < size)
    {
        const size_t bytes = ((size - at) < MMGR_SWAR_BYTES) ? (size_t)(size - at) : MMGR_SWAR_BYTES;
        embed_word loaded = 0u;
        memor.cpy(&(MemoriaCfg){.dst = &loaded, .src = text + at, .bytes = bytes});
        const embed_word hit = lane.ge(&(ScrutLaneCfg){.word = loaded, .byte = 'a'}) &
                               lane.le(&(ScrutLaneCfg){.word = loaded, .byte = 'z'});
        loaded ^= mask.spread(&(ScrutMaskCfg){.mask = hit}) & flip;
        memor.cpy(&(MemoriaCfg){.dst = text + at, .src = &loaded, .bytes = bytes});
        at += bytes;
    }
}

unsigned long long cli_text_lowered_into(char *out, unsigned long long room, const char *text)
{
    const unsigned long long whole = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    const unsigned long long length = (whole < room) ? whole : (room - 1ull);
    memor.cpy(&(MemoriaCfg){.dst = out, .src = text, .bytes = (size_t)length});
    out[length] = '\0';
    cli_text_lowered(out, length);
    return length;
}

CliTextLine cli_text_line_at(const char *text, unsigned long long at, unsigned long long size)
{
    const char *const newline = memor.chr(&(MemoriaCfg){.src = text + at, .val = '\n', .bytes = size - at});
    if (newline == NULL)
    {
        return (CliTextLine){size, size, size};
    }
    // a newline found lies in the text, at or past `at`
    const unsigned long long stop = (unsigned long long)(newline - text);
    return (CliTextLine){stop, ((stop > at) && (text[stop - 1ull] == '\r')) ? stop - 1ull : stop, stop + 1ull};
}

void cli_text_copied_or(char *out, const char *text, const char *otherwise)
{
    const char *const from = (text != NULL) ? text : otherwise;
    memor.cpy(&(MemoriaCfg){
        .dst = out, .src = from, .bytes = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}) + 1u});
}

void cli_text_kept(char *text, unsigned long long size, unsigned int classes, const char *set, char other)
{
    for (unsigned long long at = cli_text_past_class(text, 0ull, size, classes, set); at < size;
         at = cli_text_past_class(text, at + 1ull, size, classes, set))
    {
        text[at] = other;
    }
}

unsigned long long cli_text_counted(const char *text, unsigned long long size, char byte)
{
    // Explicit cast reads the byte as unsigned before it is repeated into every lane
    const embed_word wanted = MMGR_SWAR_ONES * (embed_word)(uint8_t)byte;
    unsigned long long count = 0ull;
    unsigned long long at = 0ull;
    while (at < size)
    {
        const size_t bytes = ((size - at) < MMGR_SWAR_BYTES) ? (size_t)(size - at) : MMGR_SWAR_BYTES;
        embed_word loaded = 0u;
        memor.cpy(&(MemoriaCfg){.dst = &loaded, .src = text + at, .bytes = bytes});
        const embed_word hit =
            lane.has_zero(&(ScrutLaneCfg){.word = loaded ^ wanted}) & mask.lanes_below(&(ScrutMaskCfg){.bytes = bytes});
        count += lane.count(&(ScrutLaneCfg){.mask = hit});
        at += bytes;
    }
    return count;
}

int cli_text_ascii(const char *text, unsigned long long size)
{
    unsigned long long at = 0ull;
    while (at < size)
    {
        const size_t bytes = ((size - at) < MMGR_SWAR_BYTES) ? (size_t)(size - at) : MMGR_SWAR_BYTES;
        embed_word loaded = 0u;
        memor.cpy(&(MemoriaCfg){.dst = &loaded, .src = text + at, .bytes = bytes});
        if ((loaded & MMGR_VERBUM_SCRUTOR_HIGH) != 0u)
        {
            return 0;
        }
        at += bytes;
    }
    return 1;
}

unsigned long long cli_text_put_swapped(VerbaLinea *out, const char *text, char from, char to)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    const size_t start = out->at;
    const size_t taken = mmgr_line_put_n(out, text, (size_t)size);
    if ((out->out != NULL) && (taken != 0u))
    {
        cli_text_swapped(out->out + start, taken, from, to);
    }
    return size;
}

void cli_text_put_marked(VerbaLinea *out, const char *text, const char *set, char mark)
{
    cli_text_put_marked_n(out, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), set, mark);
}

void cli_text_put_marked_n(VerbaLinea *out, const char *text, unsigned long long size, const char *set, char mark)
{
    unsigned long long at = 0ull;
    while (at < size)
    {
        const unsigned long long stop = cli_text_to_any(text, at, size, set);
        mmgr_line_put_n(out, text + at, (size_t)(stop - at));
        if (stop < size)
        {
            mmgr_line_ch(out, mark);
            mmgr_line_ch(out, text[stop]);
        }
        at = stop + 1ull;
    }
}

void cli_text_put_doubled(VerbaLinea *out, const char *text, const char *set)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    unsigned long long at = 0ull;
    while (at < size)
    {
        const unsigned long long stop = cli_text_to_any(text, at, size, set);
        mmgr_line_put_n(out, text + at, (size_t)(stop - at));
        if (stop < size)
        {
            mmgr_line_ch(out, text[stop]);
            mmgr_line_ch(out, text[stop]);
        }
        at = stop + 1ull;
    }
}

void cli_text_put_lowered(VerbaLinea *out, const char *text)
{
    const size_t start = out->at;
    const size_t taken = mmgr_line_put_n(out, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
    if ((out->out != NULL) && (taken != 0u))
    {
        cli_text_lowered(out->out + start, taken);
    }
}

void cli_text_put_kept(VerbaLinea *out, const char *text, unsigned int classes, const char *set, char other)
{
    const size_t start = out->at;
    const size_t taken = mmgr_line_put_n(out, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
    if ((out->out != NULL) && (taken != 0u))
    {
        cli_text_kept(out->out + start, taken, classes, set, other);
    }
}

// the value of a hexadecimal digit, 16 where the byte is none
static unsigned int text_hex_of(char byte)
{
    return ((byte >= '0') && (byte <= '9'))   ? (unsigned int)(byte - '0')
           : ((byte >= 'a') && (byte <= 'f')) ? ((unsigned int)(byte - 'a') + 10u)
           : ((byte >= 'A') && (byte <= 'F')) ? ((unsigned int)(byte - 'A') + 10u)
                                              : 16u;
}

void cli_text_put_unescaped(VerbaLinea *out, const char *text, unsigned long long size)
{
    unsigned long long at = 0ull;
    while (at < size)
    {
        const unsigned long long stop = cli_text_to_any(text, at, size, "%");
        mmgr_line_put_n(out, text + at, (size_t)(stop - at));
        at = stop;
        if (stop < size)
        {
            const unsigned int high = ((stop + 1ull) < size) ? text_hex_of(text[stop + 1ull]) : 16u;
            const unsigned int low = ((stop + 2ull) < size) ? text_hex_of(text[stop + 2ull]) : 16u;
            const int escaped = (high < 16u) && (low < 16u);
            // two hexadecimal digits make a byte, below 256
            mmgr_line_ch(out, escaped ? (char)((high << 4u) | low) : '%');
            at += escaped ? 3ull : 1ull;
        }
    }
}

void cli_text_put_replaced(VerbaLinea *out, const char *text, const char *set, const char *with)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    unsigned long long at = 0ull;
    while (at < size)
    {
        const unsigned long long stop = cli_text_to_any(text, at, size, set);
        mmgr_line_put_n(out, text + at, (size_t)(stop - at));
        if (stop < size)
        {
            mmgr_line_put(out, with);
        }
        at = stop + 1ull;
    }
}

void cli_text_put_filled(VerbaLinea *out, const char *text, const char *name, const char *value)
{
    const size_t size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    const size_t name_size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    const char *at = text;
    const char *const end = text + size;
    while (at < end)
    {
        const char *const found = cellul.find(
            &(CatenaFinitaCfg){.src = at, .cap = (size_t)(end - at) + 1u, .other = name, .other_cap = name_size + 1u});
        const char *const stop = (found != NULL) ? found : end;
        // a name found lies in the text, at or past `at`
        mmgr_line_put_n(out, at, (size_t)(stop - at));
        if (found == NULL)
        {
            return;
        }
        mmgr_line_put(out, value);
        at = found + name_size;
    }
}

unsigned long long cli_text_lead(const char *text, unsigned long long size)
{
    unsigned long long at = 0ull;
    while (at < size)
    {
        at = cli_text_past_any(text, at, size, CLI_TEXT_WHITE);
        if (at >= size)
        {
            break;
        }
        unsigned long long length = 0ull;
        if (!cli_text_white(cli_text_letter(text, size, at, &length)))
        {
            break;
        }
        at += length;
    }
    return at;
}

unsigned long long cli_text_trimmed_end(const char *text, unsigned long long size)
{
    unsigned long long end = size;
    while (end > 0u)
    {
        end = cli_text_back_past(text, end, CLI_TEXT_WHITE);
        if (end == 0ull)
        {
            break;
        }
        unsigned long long length = 0ull;
        if (!cli_text_white(cli_text_letter_before(text, end, &length)))
        {
            break;
        }
        end -= length;
    }
    return end;
}

void cli_text_trimmed(const char *text, unsigned long long size, unsigned long long *from, unsigned long long *to)
{
    *from = cli_text_lead(text, size);
    *to = *from + cli_text_trimmed_end(text + *from, size - *from);
}

int cli_text_word(const char *text, unsigned long long size, unsigned long long *at, unsigned long long *word,
                  unsigned long long *length)
{
    *at += cli_text_lead(text + *at, size - *at);
    if (*at >= size)
    {
        return 0;
    }
    *word = *at;
    while (*at < size)
    {
        unsigned long long letter = 0ull;
        if (cli_text_white(cli_text_letter(text, size, *at, &letter)))
        {
            break;
        }
        *at += letter;
    }
    *length = *at - *word;
    return 1;
}

unsigned long long cli_text_letters(const char *text, unsigned long long size)
{
    unsigned long long count = 0ull;
    for (unsigned long long at = 0ull; at < size;)
    {
        unsigned long long length = 0ull;
        (void)cli_text_letter(text, size, at, &length);
        at += length;
        count += 1u;
    }
    return count;
}

unsigned long long cli_text_units(const char *text, unsigned long long size)
{
    unsigned long long count = 0ull;
    for (unsigned long long at = 0ull; at < size;)
    {
        unsigned long long length = 0ull;
        const unsigned int letter = cli_text_letter(text, size, at, &length);
        at += length;
        count += (letter >= 0x10000u) ? 2u : 1u;
    }
    return count;
}

int cli_text_starts(const char *text, unsigned long long size, const char *prefix)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = prefix, .cap = CLI_CELLS_POOL});
    return (length <= size) && (cellul.diff(&(CatenaFinitaCfg){.src = text, .other = prefix, .cap = length}) == length);
}

int cli_text_ends(const char *text, unsigned long long size, const char *suffix)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = suffix, .cap = CLI_CELLS_POOL});
    return (length <= size) &&
           (cellul.diff(&(CatenaFinitaCfg){.src = text + size - length, .other = suffix, .cap = length}) == length);
}

int cli_text_find(const char *text, unsigned long long length, const char *wanted, unsigned long long size,
                  unsigned long long *at)
{
    // a wanted text of no bytes is at the start; find would measure one of 0 as terminated
    const char *const found = (size == 0ull) ? text
                                             : cellul.find(&(CatenaFinitaCfg){.src = text,
                                                                              .cap = (size_t)length,
                                                                              .other = wanted,
                                                                              .other_cap = (size_t)size,
                                                                              .other_len = (size_t)size});
    if (found == NULL)
    {
        return 0;
    }
    *at = (unsigned long long)(found - text);
    return 1;
}

int cli_text_quoted_end(const char *text, unsigned long long size, unsigned long long at, unsigned long long *end)
{
    const char *const rest = text + at;
    const unsigned long long left = size - at;
    unsigned long long found = 0ull;
    if (cli_text_starts(rest, left, "\"\"\"") || cli_text_starts(rest, left, "'''"))
    {
        *end = cli_text_find(rest + 3u, left - 3u, rest, 3u, &found) ? at + 3u + found + 3u : size;
        return 1;
    }
    if ((left == 0ull) || ((rest[0] != '"') && (rest[0] != '\'')))
    {
        return 0;
    }
    unsigned long long walk = at + 1u;
    while ((walk < size) && (text[walk] != rest[0]) && (text[walk] != '\n'))
    {
        walk += (text[walk] == '\\') ? 2u : 1u;
    }
    *end = (walk + 1u < size) ? walk + 1u : size;
    return 1;
}

int cli_text_find_last(const char *text, unsigned long long length, const char *wanted, unsigned long long size,
                       unsigned long long *at)
{
    if (size > length)
    {
        return 0;
    }
    if (size == 0ull)
    {
        *at = length;
        return 1;
    }
    // the places the wanted text can start at, its first byte found by the word scan from the last of them back, and
    // the whole compared only where that byte stands
    unsigned long long places = length - size + 1ull;
    for (const char *start = cli_text_last(text, places, wanted[0]); start != NULL;
         start = cli_text_last(text, places, wanted[0]))
    {
        if (cellul.diff(&(CatenaFinitaCfg){.src = start, .other = wanted, .cap = (size_t)size}) == (size_t)size)
        {
            *at = (unsigned long long)(start - text);
            return 1;
        }
        places = (unsigned long long)(start - text);
    }
    return 0;
}

int cli_text_order(const char *one, const char *other)
{
    // `one` with its terminator: `other` differs from it there at the latest, so the first difference is the order
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = one, .cap = CLI_CELLS_POOL}) + 1u;
    const embed_iword order = memor.cmp(&(MemoriaCfg){.src = one, .other = other, .bytes = length});
    return (order > 0) - (order < 0);
}

void cli_paths_sorted(char (*paths)[CLI_PATH_MAX_LEN], unsigned int count)
{
    for (unsigned int at = 1u; at < count; at += 1u)
    {
        char pivot[CLI_PATH_MAX_LEN];
        memor.cpy(&(MemoriaCfg){.dst = pivot,
                                .src = paths[at],
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = paths[at], .cap = CLI_CELLS_POOL}) + 1u});
        unsigned int place = at;
        while ((place > 0u) && (cli_text_order(paths[place - 1u], pivot) > 0))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = paths[place],
                .src = paths[place - 1u],
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = paths[place - 1u], .cap = CLI_CELLS_POOL}) + 1u});
            place -= 1u;
        }
        memor.cpy(&(MemoriaCfg){.dst = paths[place],
                                .src = pivot,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = pivot, .cap = CLI_CELLS_POOL}) + 1u});
    }
}

int cli_text_utf8_whole(const unsigned char *text, unsigned long long size)
{
    unsigned long long at = 0ull;
    while (at < size)
    {
        if (((at + MMGR_SWAR_BYTES) <= size) &&
            ((word.load(&(ScrutWordCfg){.at = text + at}) & MMGR_VERBUM_SCRUTOR_HIGH) == 0u))
        {
            at += MMGR_SWAR_BYTES;
            continue;
        }
        unsigned int letter = 0u;
        const unsigned long long step = cli_lossy_letter(text, size, at, &letter);
        if ((step == 1ull) && (text[at] >= 0x80u))
        {
            return 0;
        }
        at += step;
    }
    return 1;
}

int cli_text_same(const char *one, const char *other, unsigned long long size)
{
    return cellul.diff(&(CatenaFinitaCfg){.src = one, .other = other, .cap = (size_t)size}) == (size_t)size;
}

// Walks the aligned words the text stands in from the one containing its first byte, a word at a time: each word tested
// against every byte of the set in every lane at once and the lanes the set contains joined, the lanes before the text
// masked off, and the first lane that stops the run taken: its terminator, or a byte of the set where `in_set` and
// one outside it where not. Every load is an aligned word containing a byte of the text, so nothing outside those words
// is read
static unsigned long long text_run(const char *text, const char *set, int in_set)
{
    // Explicit casts read the addresses as integers, so a word's place and the text's start compare as numbers
    const uintptr_t first = (uintptr_t)text;
    uintptr_t word_at = first & ~(uintptr_t)(MMGR_SWAR_BYTES - 1u);
    embed_word before = mask.lanes_below(&(ScrutMaskCfg){.bytes = (size_t)(first - word_at)});
    for (;;)
    {
        const embed_word loaded = word.load_al(&(ScrutWordCfg){.at = (const void *)word_at});
        embed_word match_mask = 0u;
        for (const char *letter = set; *letter != '\0'; letter += 1)
        {
            // Explicit cast reads the set's byte as unsigned before it is repeated into every lane
            match_mask |= lane.eq(&(ScrutLaneCfg){.word = loaded, .byte = (uint8_t)*letter});
        }
        const embed_word ended = lane.has_zero(&(ScrutLaneCfg){.word = loaded});
        const embed_word stops = (ended | (in_set ? match_mask : (~match_mask & MMGR_VERBUM_SCRUTOR_HIGH))) & ~before;
        if (stops != 0u)
        {
            return (unsigned long long)(word_at + lane.first(&(ScrutLaneCfg){.mask = stops}) - first);
        }
        word_at += MMGR_SWAR_BYTES;
        before = 0u;
    }
}

unsigned long long cli_text_upto(const char *text, const char *set)
{
    return text_run(text, set, 1);
}

unsigned long long cli_text_over(const char *text, const char *set)
{
    return text_run(text, set, 0);
}

// Walks the aligned words the text stands in, from the one containing its last byte back to the one containing its
// first, a word at a time: each word tested against the byte in every lane at once, its lanes outside the text masked
// off, and the last lane that contains the byte taken. Every load is an aligned word containing a byte of the text, so
// nothing outside those words is read
const char *cli_text_last(const char *text, unsigned long long length, char byte)
{
    if (length == 0ull)
    {
        return NULL;
    }
    // Explicit casts read the addresses as integers, so a word's place and the text's bounds compare as numbers
    const uintptr_t first = (uintptr_t)text;
    const uintptr_t end = first + (uintptr_t)length;
    const embed_word every = mask.lanes_below(&(ScrutMaskCfg){.bytes = MMGR_SWAR_BYTES});
    // Explicit cast reads the byte as unsigned before it is repeated into every lane
    const embed_word broadcast = MMGR_SWAR_ONES * (embed_word)(uint8_t)byte;
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
