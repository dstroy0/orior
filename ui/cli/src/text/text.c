// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// text.c: a text of UTF-8 read character by character. A byte that starts no character, or a character cut short, is read as one
// character of its own byte
#include "text.h"
#include "json_tree.h"
#include "regexp.h"

#include "../../../includes/utf8/utf8.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

unsigned int cli_text_character(const CliTextAtCfg *args)
{
    return EMBED_CALL(utf8_at.character, Utf8AtCfg, .text = args->text, .at = args->at, .size = args->size,
                      .length = args->length);
}

unsigned int cli_text_character_before(const CliTextLengthCfg *args)
{
    return EMBED_CALL(utf8_end.character_before, Utf8EndCfg, .text = args->text, .end = args->end_value,
                      .length = args->length);
}

int cli_text_white(const CliTextCfg *args)
{
    return (args->character == ' ') || ((args->character >= '\t') && (args->character <= '\r')) || (args->character == 0x85u) ||
           (args->character == 0xA0u) || (args->character == 0x1680u) ||
           ((args->character >= 0x2000u) && (args->character <= 0x200Au)) || (args->character == 0x2028u) ||
           (args->character == 0x2029u) || (args->character == 0x202Fu) || (args->character == 0x205Fu) ||
           (args->character == 0x3000u);
}

int cli_text_alphabetic(const CliTextCfg *args)
{
    unsigned int character = args->character;
    if (character < 0x80u)
    {
        return ((character >= 'a') && (character <= 'z')) || ((character >= 'A') && (character <= 'Z'));
    }
    return EMBED_CALL(cli_regexp_character.property, CliRegexpLetterCfg, .property = CLI_REGEXP_CHARACTER, .character = character);
}

int cli_text_alphanumeric(const CliTextCfg *args)
{
    unsigned int character = args->character;
    if (character < 0x80u)
    {
        return EMBED_CALL(cli_text_table.alphabetic, CliTextCfg, .character = character) ||
               ((character >= '0') && (character <= '9'));
    }
    return EMBED_CALL(cli_regexp_character.property, CliRegexpLetterCfg, .property = CLI_REGEXP_CHARACTER,
                      .character = character) ||
           EMBED_CALL(cli_regexp_character.property, CliRegexpLetterCfg, .property = CLI_REGEXP_NUMBER, .character = character);
}

typedef struct
{
    const char *set;
    embed_word loaded;
    const Utf8Set *filled;
    const char *text;
    unsigned long long at;
    unsigned long long size;
    int in;
    unsigned int classes;
    char byte;
    int in_set;
} TextCtx;
unsigned long long cli_text_to_any(const CliTextAtSizeClassesCfg *args)
{
    return EMBED_CALL(utf8_set.to_any, Utf8SetCfg, .text = args->text, .at = args->at, .size = args->size,
                      .set = args->set);
}

unsigned long long cli_text_end_any(const CliTextAtSizeClassesCfg *args)
{
    return EMBED_CALL(utf8_set.end_any, Utf8SetCfg, .text = args->text, .at = args->at, .size = args->size,
                      .set = args->set);
}

unsigned long long cli_text_back_any(const CliTextAtSizeClassesCfg *args)
{
    return EMBED_CALL(utf8_set.back_any, Utf8SetCfg, .text = args->text, .at = args->at, .set = args->set);
}

unsigned long long cli_text_back_end(const CliTextAtSizeClassesCfg *args)
{
    return EMBED_CALL(utf8_set.back_end, Utf8SetCfg, .text = args->text, .at = args->at, .set = args->set);
}

// the lanes of a word that hold a byte of `classes` or of the set
EMBED_INLINE embed_word text_word_of_class(const TextCtx *args)
{
    embed_word hit = EMBED_CALL(utf8_word.in, Utf8WordCfg, .loaded = args->loaded, .filled = args->filled);
    if ((args->classes & CLI_TEXT_CHARACTERS) != 0u)
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
    Utf8Set filled;
    EMBED_CALL(utf8_set_fill_table.fill, Utf8SetFillCfg, .filled_out = &filled, .set = args->set);
    unsigned long long place = args->at;
    while (place < args->size)
    {
        const embed_word bytes =
            ((args->size - place) < MMGR_SWAR_BYTES) ? (embed_word)(args->size - place) : MMGR_SWAR_BYTES;
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
    Utf8Set filled;
    EMBED_CALL(utf8_set_fill_table.fill, Utf8SetFillCfg, .filled_out = &filled, .set = args->set);
    const embed_word every = mask.lanes_below(&(ScrutMaskCfg){.bytes = MMGR_SWAR_BYTES});
    unsigned long long place = args->at;
    while (place < args->size)
    {
        const embed_word bytes =
            ((args->size - place) < MMGR_SWAR_BYTES) ? (embed_word)(args->size - place) : MMGR_SWAR_BYTES;
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
    Utf8Set filled;
    EMBED_CALL(utf8_set_fill_table.fill, Utf8SetFillCfg, .filled_out = &filled, .set = args->set);
    const embed_word every = mask.lanes_below(&(ScrutMaskCfg){.bytes = MMGR_SWAR_BYTES});
    unsigned long long place = args->at;
    while (place != 0ULL)
    {
        const embed_word bytes = (place < MMGR_SWAR_BYTES) ? (embed_word)place : MMGR_SWAR_BYTES;
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
    memor.cpy(&(MemoriaCfg){.dst = read, .src = args->text + args->at, .bytes = (embed_word)digits});
    read[digits] = '\0';
    return cellul.to_ulong(&(TransfiguroCfg){.src = read});
}

// The low nibbles of a word's first `count` lanes folded into one number, the first lane in memory the max
// significant: pairs of lanes into sixteen bits, pairs of those into thirty two, then the two halves. The lanes past
// `count` hold 0 and fall off the low end
EMBED_INLINE unsigned long long text_nibbles_packed(embed_word valued, unsigned long long count)
{
#if EMBED_BIG_ENDIAN
    const embed_word pairs = (((valued >> 8u) & 0x000F000F000F000Fu) << 4u) | (valued & 0x000F000F000F000Fu);
    const embed_word quads = (((pairs >> 16u) & 0x000000FF000000FFu) << 8u) | (pairs & 0x000000FF000000FFu);
    const embed_word whole = (((quads >> 32u) & 0xFFFFu) << 16u) | (quads & 0xFFFFu);
#else
    const embed_word pairs = ((valued & 0x000F000F000F000Fu) << 4u) | ((valued >> 8u) & 0x000F000F000F000Fu);
    const embed_word quads = ((pairs & 0x000000FF000000FFu) << 8u) | ((pairs >> 16u) & 0x000000FF000000FFu);
    const embed_word whole = ((quads & 0xFFFFu) << 16u) | ((quads >> 32u) & 0xFFFFu);
#endif
    return whole >> (4ULL * (MMGR_SWAR_BYTES - count));
}

// A word of the text at a time: each lane classed a numeral, a character a through f in either case or neither, the
// digits' run cut at the first lane that is neither, each digit lane valued with no borrow between lanes, the run's
// nibbles packed onto the value
unsigned long long cli_text_hex(const CliTextAtSizeCfg *args)
{
    EMBED_STATIC_ASSERT(MMGR_SWAR_BYTES == 8u, "the nibble packing folds eight lanes");
    unsigned long long value = 0ULL;
    unsigned long long at = args->at;
    while (at < args->size)
    {
        const embed_word bytes =
            ((args->size - at) < MMGR_SWAR_BYTES) ? (embed_word)(args->size - at) : MMGR_SWAR_BYTES;
        embed_word loaded = 0u;
        memor.cpy(&(MemoriaCfg){.dst = &loaded, .src = args->text + at, .bytes = bytes});
        const embed_word folded = word.fold_lower(&(ScrutWordCfg){.word = loaded});
        const embed_word within = mask.lanes_below(&(ScrutMaskCfg){.bytes = bytes});
        const embed_word numerals = lane.ge(&(ScrutLaneCfg){.word = loaded, .byte = '0'}) &
                                    lane.le(&(ScrutLaneCfg){.word = loaded, .byte = '9'}) & within;
        const embed_word characters = lane.ge(&(ScrutLaneCfg){.word = folded, .byte = 'a'}) &
                                   lane.le(&(ScrutLaneCfg){.word = folded, .byte = 'f'}) & within;
        const embed_word other = within & ~(numerals | characters);
        const embed_word run = (other == 0u) ? within : mask.before(&(ScrutMaskCfg){.mask = other});
        const unsigned long long count = lane.count(&(ScrutLaneCfg){.mask = run});
        const embed_word numeral_bytes = mask.spread(&(ScrutMaskCfg){.mask = numerals & run});
        const embed_word character_bytes = mask.spread(&(ScrutMaskCfg){.mask = characters & run});
        // Explicit casts repeat each digit class's base into every lane: a numeral is its byte less '0', a character its
        // lower case byte less 'a' plus ten
        const embed_word valued =
            ((loaded & numeral_bytes) - ((MMGR_SWAR_ONES * (embed_word)'0') & numeral_bytes)) |
            ((folded & character_bytes) - ((MMGR_SWAR_ONES * (embed_word)('a' - 10)) & character_bytes));
        value = (value << (4ULL * count)) | text_nibbles_packed(valued, count);
        at += count;
        if (count < bytes)
        {
            break;
        }
    }
    if (args->end != NULL)
    {
        *args->end = at;
    }
    return ((at - args->at) > 16ULL) ? ~0ULL : value;
}

void cli_text_swap(const CliTextSizeFromCfg *args)
{
    // Explicit casts read the bytes as unsigned before they are repeated into every lane
    const embed_word required = MMGR_SWAR_ONES * (embed_word)(embed_u8)args->from;
    const embed_word flip = MMGR_SWAR_ONES * (embed_word)((embed_u8)args->from ^ (embed_u8)args->to);
    unsigned long long at = 0ULL;
    while (at < args->size)
    {
        const embed_word bytes =
            ((args->size - at) < MMGR_SWAR_BYTES) ? (embed_word)(args->size - at) : MMGR_SWAR_BYTES;
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
        const embed_word bytes =
            ((args->size - at) < MMGR_SWAR_BYTES) ? (embed_word)(args->size - at) : MMGR_SWAR_BYTES;
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
        const embed_word bytes =
            ((args->size - at) < MMGR_SWAR_BYTES) ? (embed_word)(args->size - at) : MMGR_SWAR_BYTES;
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
    const unsigned long long whole = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES});
    const unsigned long long length = (whole < args->capacity) ? whole : (args->capacity - 1ULL);
    memor.cpy(&(MemoriaCfg){.dst = args->out_capacity, .src = args->text, .bytes = (embed_word)length});
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
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_POOL_BYTES}) + 1u});
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
    const embed_word required = MMGR_SWAR_ONES * (embed_word)(embed_u8)args->byte;
    unsigned long long count = 0ULL;
    unsigned long long at = 0ULL;
    while (at < args->size)
    {
        const embed_word bytes =
            ((args->size - at) < MMGR_SWAR_BYTES) ? (embed_word)(args->size - at) : MMGR_SWAR_BYTES;
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
        const embed_word bytes =
            ((args->size - at) < MMGR_SWAR_BYTES) ? (embed_word)(args->size - at) : MMGR_SWAR_BYTES;
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
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES});
    const embed_word start = args->out->at;
    const embed_word reserved = mmgr_line_put_n(args->out, args->text, (embed_word)size);
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
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES}), .set = args->set,
               .mark = args->mark);
}

void cli_text_append_marked_n(const CliTextSizeSetCfg *args)
{
    unsigned long long at = 0ULL;
    while (at < args->size)
    {
        const unsigned long long stop = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                   .text = args->text, .at = at, .size = args->size, .set = args->set);
        mmgr_line_put_n(args->out, args->text + at, (embed_word)(stop - at));
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
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES});
    unsigned long long at = 0ULL;
    while (at < size)
    {
        const unsigned long long stop = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                   .text = args->text, .at = at, .size = size, .set = args->set);
        mmgr_line_put_n(args->out, args->text + at, (embed_word)(stop - at));
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
    const embed_word start = args->out->at;
    const embed_word reserved = mmgr_line_put_n(
        args->out, args->text, cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES}));
    if ((args->out->out != NULL) && (reserved != 0u))
    {
        EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .text_out = args->out->out + start, .size = reserved);
    }
}

void cli_text_append_save(const CliTextClassesCfg *args)
{
    const embed_word start = args->out->at;
    const embed_word reserved = mmgr_line_put_n(
        args->out, args->text, cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES}));
    if ((args->out->out != NULL) && (reserved != 0u))
    {
        EMBED_CALL(cli_text_size.save, CliTextSizeCfg, .text_out = args->out->out + start, .size = reserved,
                   .classes = args->classes, .set = args->set, .other = args->other);
    }
}

void cli_text_append_unescaped(const CliTextSizeSetCfg *args)
{
    unsigned long long at = 0ULL;
    while (at < args->size)
    {
        const unsigned long long stop = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                   .text = args->text, .at = at, .size = args->size, .set = "%");
        mmgr_line_put_n(args->out, args->text + at, (embed_word)(stop - at));
        at = stop;
        if (stop < args->size)
        {
            const unsigned int high = ((stop + 1ULL) < args->size) ? EMBED_CALL(cli_text_byte.hex_digit, CliTextByteCfg,
                                                                                .byte = args->text[stop + 1ULL])
                                                                   : 16u;
            const unsigned int low = ((stop + 2ULL) < args->size) ? EMBED_CALL(cli_text_byte.hex_digit, CliTextByteCfg,
                                                                               .byte = args->text[stop + 2ULL])
                                                                  : 16u;
            const int escaped = (high < 16u) && (low < 16u);
            // two hexadecimal digits make a byte, below 256
            mmgr_line_ch(args->out, escaped ? (char)((high << 4u) | low) : '%');
            at += escaped ? 3ULL : 1ULL;
        }
    }
}

void cli_text_append_replace(const CliTextSetCfg *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES});
    unsigned long long at = 0ULL;
    while (at < size)
    {
        const unsigned long long stop = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                   .text = args->text, .at = at, .size = size, .set = args->set);
        mmgr_line_put_n(args->out, args->text + at, (embed_word)(stop - at));
        if (stop < size)
        {
            mmgr_line_put(args->out, args->with);
        }
        at = stop + 1ULL;
    }
}

void cli_text_append_fill(const CliTextOutNameCfg *args)
{
    const embed_word size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES});
    const embed_word name_size = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES});
    const char *at = args->text;
    const char *const end = args->text + size;
    while (at < end)
    {
        const char *const found = cellul.find(&(CatenaFinitaCfg){
            .src = at, .cap = (embed_word)(end - at) + 1u, .other = args->name, .other_cap = name_size + 1u});
        const char *const stop = (found != NULL) ? found : end;
        // a name found lies in the text, at or past `at`
        mmgr_line_put_n(args->out, at, (embed_word)(stop - at));
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
                        .character = EMBED_CALL(cli_text_at.character, CliTextAtCfg, .text = args->text, .size = args->size,
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
    int going = 1;
    while (going && (end > 0u))
    {
        end = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = args->text, .at = end,
                         .set = CLI_TEXT_WHITE);
        if (end == 0ULL)
        {
            going = 0;
            continue;
        }
        unsigned long long length = 0ULL;
        if (!EMBED_CALL(cli_text_table.white, CliTextCfg,
                        .character = EMBED_CALL(cli_text_length.character_before, CliTextLengthCfg, .text = args->text,
                                             .end_value = end, .length = &length)))
        {
            going = 0;
            continue;
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
        unsigned long long character = 0ULL;
        if (EMBED_CALL(cli_text_table.white, CliTextCfg,
                       .character = EMBED_CALL(cli_text_at.character, CliTextAtCfg, .text = args->text, .size = args->size,
                                            .at = *at, .length = &character)))
        {
            break;
        }
        *at += character;
    }
    *length = *at - *word_text;
    return 1;
}

unsigned long long cli_text_characters(const CliTextSizeByteCfg *args)
{
    unsigned long long count = 0ULL;
    for (unsigned long long at = 0ULL; at < args->size;)
    {
        unsigned long long length = 0ULL;
        (void)EMBED_CALL(cli_text_at.character, CliTextAtCfg, .text = args->text, .size = args->size, .at = at,
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
        const unsigned int character = EMBED_CALL(cli_text_at.character, CliTextAtCfg, .text = args->text, .size = args->size,
                                               .at = at, .length = &length);
        at += length;
        count += (character >= 0x10000u) ? 2u : 1u;
    }
    return count;
}

int cli_text_starts(const CliTextSizePrefixCfg *args)
{
    const embed_word length = cellul.len(&(CatenaFinitaCfg){.src = args->prefix, .cap = CLI_POOL_BYTES});
    return (length <= args->size) &&
           (cellul.diff(&(CatenaFinitaCfg){.src = args->text, .other = args->prefix, .cap = length}) == length);
}

int cli_text_ends(const CliTextSizeSuffixCfg *args)
{
    const embed_word length = cellul.len(&(CatenaFinitaCfg){.src = args->suffix, .cap = CLI_POOL_BYTES});
    return (length <= args->size) &&
           (cellul.diff(&(CatenaFinitaCfg){
                .src = args->text + args->size - length, .other = args->suffix, .cap = length}) == length);
}

int cli_text_find(const CliTextSizeAtCfg *args)
{
    unsigned long long *at = args->at_pointer;
    // a wanted text of no bytes is at the start; find would measure one of 0 as terminated
    const char *const found = (args->size == 0ULL)
                                  ? args->text
                                  : cellul.find(&(CatenaFinitaCfg){.src = args->text,
                                                                   .cap = (embed_word)args->length_value,
                                                                   .other = args->required,
                                                                   .other_cap = (embed_word)args->size,
                                                                   .other_len = (embed_word)args->size});
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
    // the whole compared only where that byte is
    unsigned long long positions = args->length_value - args->size + 1ULL;
    for (const char *start = EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->text,
                                        .length_value = positions, .byte = args->required[0]);
         start != NULL; start = EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->text,
                                           .length_value = positions, .byte = args->required[0]))
    {
        if (cellul.diff(&(CatenaFinitaCfg){.src = start, .other = args->required, .cap = (embed_word)args->size}) ==
            (embed_word)args->size)
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
    const embed_word length = cellul.len(&(CatenaFinitaCfg){.src = args->one, .cap = CLI_POOL_BYTES}) + 1u;
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
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->paths[at], .cap = CLI_POOL_BYTES}) + 1u});
        unsigned int place = at;
        while ((place > 0u) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = args->paths[place - 1u],
                                           .other_pointer = pivot) > 0))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = args->paths[place],
                .src = args->paths[place - 1u],
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->paths[place - 1u], .cap = CLI_POOL_BYTES}) + 1u});
            place -= 1u;
        }
        memor.cpy(&(MemoriaCfg){.dst = args->paths[place],
                                .src = pivot,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = pivot, .cap = CLI_POOL_BYTES}) + 1u});
    }
}

int cli_text_utf8_valid(const CliTextSizeTypeCfg *args)
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
        unsigned int character = 0u;
        const unsigned long long step =
            EMBED_CALL(cli_json_tree_length.lossy_character, CliJsonTreeLengthCfg, .text_bytes = text,
                       .length = args->size, .at = at, .character = &character);
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
    return cellul.diff(&(CatenaFinitaCfg){.src = args->one, .other = other, .cap = (embed_word)args->size}) ==
           (embed_word)args->size;
}

// a byte with an ASCII capital made its small character
EMBED_INLINE char text_folded(char byte)
{
    return ((byte >= 'A') && (byte <= 'Z')) ? (char)(byte + ('a' - 'A')) : byte;
}

int cli_text_same_folded(const CliTextSizeOneCfg *args)
{
    const char *other = args->other_pointer;
    if (cellul.len(&(CatenaFinitaCfg){.src = other, .cap = CLI_POOL_BYTES}) != args->size)
    {
        return 0;
    }
    for (unsigned long long at = 0ULL; at < args->size; at += 1ULL)
    {
        if (text_folded(args->one[at]) != text_folded(other[at]))
        {
            return 0;
        }
    }
    return 1;
}

int cli_text_before_folded(const CliTextSizeOneCfg *args)
{
    const char *one = args->one;
    const char *other = args->other_pointer;
    for (;; one += 1, other += 1)
    {
        const char left = text_folded(one[0]);
        const char right = text_folded(other[0]);
        if ((left != right) || (left == '\0'))
        {
            return (unsigned char)left < (unsigned char)right;
        }
    }
}

unsigned int cli_text_hex_digit(const CliTextByteCfg *args)
{
    // a digit's distance from the first of its run is below 16 where the digit is one
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

// Walks the aligned words the text is in from the one containing its first byte, a word at a time: each word tested
// against every byte of the set in every lane at once and the lanes the set contains joined, the lanes before the text
// masked off, and the first lane that stops the run taken: its terminator, or a byte of the set where `in_set` and
// one outside it where not. Every load is an aligned word containing a byte of the text, so nothing outside those words
// is read
EMBED_INLINE unsigned long long text_run(const TextCtx *args)
{
    // Explicit casts read the addresses as integers, so a word's place and the text's start compare as numbers
    const embed_word first = (embed_word)args->text;
    embed_word word_at = first & ~(embed_word)(MMGR_SWAR_BYTES - 1u);
    embed_word before = mask.lanes_below(&(ScrutMaskCfg){.bytes = (embed_word)(first - word_at)});
    for (;;)
    {
        const embed_word loaded = word.load_al(&(ScrutWordCfg){.at = (const void *)word_at});
        embed_word match_mask = 0u;
        for (const char *character = args->set; *character != '\0'; character += 1)
        {
            // Explicit cast reads the set's byte as unsigned before it is repeated into every lane
            match_mask |= lane.eq(&(ScrutLaneCfg){.word = loaded, .byte = (embed_u8)*character});
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

// Walks the aligned words the text is in, from the one containing its last byte back to the one containing its
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
    const embed_word first = (embed_word)args->text;
    const embed_word end = first + (embed_word)args->length_value;
    const embed_word every = mask.lanes_below(&(ScrutMaskCfg){.bytes = MMGR_SWAR_BYTES});
    // Explicit cast reads the byte as unsigned before it is repeated into every lane
    const embed_word broadcast = MMGR_SWAR_ONES * (embed_word)(embed_u8)args->byte;
    embed_word word_at = (end - 1u) & ~(embed_word)(MMGR_SWAR_BYTES - 1u);
    embed_word below_mask = mask.lanes_below(&(ScrutMaskCfg){.bytes = (embed_word)(end - word_at)});
    for (;;)
    {
        if (word_at < first)
        {
            below_mask &= ~mask.lanes_below(&(ScrutMaskCfg){.bytes = (embed_word)(first - word_at)});
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
