// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// rows.c: the rows: a number's halves, a number read, put and added, the hash, a row passed to the run with its who, a
// record's row and a text's, the text slots grown, and a text taken
#include "../../../../src/platform/platform.h"
#include "../../../parsers/who.h"
#include "krep_records_internal.h"
#include "memoria_operor/memoria_operor.h"

// the 32-bit word of the four little-endian bytes at `bytes`, the head's version
unsigned int krep_half_of(const unsigned char *bytes)
{
    return (unsigned int)bytes[0] | ((unsigned int)bytes[1] << 8u) | ((unsigned int)bytes[2] << 16u) |
           ((unsigned int)bytes[3] << 24u);
}

// `half` as four little-endian bytes into `bytes`, the head's version
void krep_half_put(unsigned char *bytes, unsigned int half)
{
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        bytes[at] = (unsigned char)(half >> (8u * at));
    }
}

// the number at `*at` of the `size` bytes at `bytes` into `value`, `*at` moved past it: 1, or 0 where it runs past
// the end or past ten bytes
int krep_number_read(const unsigned char *bytes, unsigned long long size, unsigned long long *at,
                     unsigned long long *value)
{
    *value = 0ULL;
    for (unsigned int taken = 0u; (taken < KREP_NUMBER_MAX) && (*at < size); taken += 1u)
    {
        const unsigned char byte = bytes[*at];
        *at += 1ULL;
        *value |= (unsigned long long)(byte & 0x7fu) << (7u * taken);
        if ((byte & 0x80u) == 0u)
        {
            return 1;
        }
    }
    return 0;
}

// `value` written into `bytes`, which has room for KREP_NUMBER_MAX, seven bits a byte: the bytes it took
static unsigned int krep_number_put(unsigned char *bytes, unsigned long long value)
{
    unsigned int taken = 0u;
    do
    {
        const unsigned char low = (unsigned char)(value & 0x7fULL);
        value >>= 7u;
        bytes[taken] = (unsigned char)(low | ((value != 0ULL) ? 0x80u : 0u));
        taken += 1u;
    } while (value != 0ULL);
    return taken;
}

// `value` written at `at` into `out`, which has room for `cap`, seven bits a byte: the offset after it, or `cap` where
// it did not fit
size_t krep_number_added(char *out, size_t cap, size_t at, unsigned long long value)
{
    unsigned char bytes[KREP_NUMBER_MAX];
    const unsigned int taken = krep_number_put(bytes, value);
    return verba_textus.put_n(
        &(VerbaTextusCfg){.out = out, .cap = cap, .at = at, .text = (const char *)bytes, .text_len = taken});
}

// a 64-bit FNV-1a hash of `length` bytes at `bytes`
static unsigned long long krep_hash(const char *bytes, unsigned long long length)
{
    unsigned long long hash = 0xcbf29ce484222325ULL;
    for (unsigned long long at = 0ULL; at < length; at += 1ULL)
    {
        hash = (hash ^ (unsigned char)bytes[at]) * 0x100000001b3ULL;
    }
    return hash;
}

// A row of the table of `file` of the key `key`, of the who that two words `who` carry, and of the change `change`. It
// holds its `count` numbers from `numbers`, then `size` bytes at `bytes` filled out to a whole word with zeros. It goes
// at `priority`.
// The result is its sequence where it is critical, and 1 where it is ordinary. It is 0 where the pass failed, and the
// file then belongs to no table
unsigned long long krep_row_passed(KrepFile *file, unsigned long long key, const unsigned long long *who,
                                   unsigned int change, const unsigned long long *numbers, unsigned int count,
                                   const void *bytes, unsigned long long size, unsigned int priority)
{
    static const unsigned char s_zeros[8] = {0u};
    const unsigned long long opening[KREP_ROW_OPENING] = {key, who[0], who[1], change};
    static char s_row_bytes[QRY_WRITER_RING_BYTES / 2ULL];
    size_t row = verba_textus.put_n(&(VerbaTextusCfg){.out = s_row_bytes,
                                                      .cap = sizeof(s_row_bytes),
                                                      .at = 0u,
                                                      .text = (const char *)opening,
                                                      .text_len = sizeof(opening)});
    row = verba_textus.put_n(&(VerbaTextusCfg){.out = s_row_bytes,
                                               .cap = sizeof(s_row_bytes),
                                               .at = row,
                                               .text = (const char *)numbers,
                                               .text_len = (size_t)count * sizeof(unsigned long long)});
    row = verba_textus.put_n(&(VerbaTextusCfg){
        .out = s_row_bytes, .cap = sizeof(s_row_bytes), .at = row, .text = (const char *)bytes, .text_len = size});
    row = verba_textus.put_n(&(VerbaTextusCfg){.out = s_row_bytes,
                                               .cap = sizeof(s_row_bytes),
                                               .at = row,
                                               .text = (const char *)s_zeros,
                                               .text_len = (8u - (size % 8u)) % 8u});
    const unsigned long long sequence = (!verba_finis.ok(&(VerbaFinisCfg){.cap = sizeof(s_row_bytes), .at = row}))
                                            ? 0ULL
                                            : qry_member_append(file->table, s_row_bytes, row, priority);
    file->key = (sequence != 0ULL) ? file->key : 0ULL;
    return sequence;
}

// the two words of the who in force over the record at `at` of `file`, into `words`. They are the own words of a who
// record, or those of the nearest one before it, zeros where none is
void krep_who_words_at(const KrepFile *file, unsigned int at, unsigned long long *words)
{
    words[0] = 0ULL;
    words[1] = 0ULL;
    if ((at < file->records) && (file->record[at].code == KREP_RECORD_WHO))
    {
        if (file->record[at].count == 2u)
        {
            words[0] = file->record[at].word[0];
            words[1] = file->record[at].word[1];
        }
        return;
    }
    Who who;
    if (krep_who_at(file, at, &who))
    {
        who_words(who, words);
    }
}

// the row of the record at `at` of `file`, put there, of the who two words `who` carry, passed to its table
void krep_record_row(KrepFile *file, unsigned int at, const unsigned long long *who)
{
    const KrepRecord *const entry = &file->record[at];
    const unsigned long long numbers[3] = {at, entry->code, entry->count};
    krep_row_passed(file, file->key, who, KREP_ROW_PUT, numbers, 3u, entry->word,
                    (unsigned long long)entry->count * sizeof(unsigned long long), QRY_ORDINARY);
}

// the row of the text of the number `number` of `file`, of no who, passed to its table
void krep_text_row(KrepFile *file, unsigned int number)
{
    static const unsigned long long s_none[2] = {0ULL, 0ULL};
    const unsigned long long numbers[2] = {number, file->text_length[number - 1u]};
    krep_row_passed(file, file->key, s_none, KREP_ROW_TEXT, numbers, 2u, file->text[number - 1u],
                    file->text_length[number - 1u], QRY_ORDINARY);
}

// 1 where the text numbered `number` of `file` is the `length` bytes at `bytes`
static int krep_text_is(const KrepFile *file, unsigned int number, const char *bytes, unsigned long long length)
{
    // a text is at max the bytes of a file read whole, which a size_t counts
    return (file->text_length[number - 1u] == length) &&
           ((length == 0ULL) || (EMBED_CALL(memor.cmp, MemoriaCfg, .src = file->text[number - 1u], .other = bytes,
                                            .bytes = (size_t)length) == 0));
}

// the order of two places, by the hash of the bytes: below 0, 0 or above 0
static int krep_place_order(const void *left, const void *right)
{
    const KrepTextPlace *const one = (const KrepTextPlace *)left;
    const KrepTextPlace *const other = (const KrepTextPlace *)right;
    return (one->hash < other->hash) ? -1 : ((one->hash > other->hash) ? 1 : 0);
}

// the number of the text of `length` bytes at `bytes` in `file`, 0 where the file has none: the run first, then the
// order from the first place of the hash, which a binary search finds
unsigned int krep_text_found(const KrepFile *file, const char *bytes, unsigned long long length)
{
    const unsigned long long hash = krep_hash(bytes, length);
    for (unsigned int each = 0u; each < file->text_run_count; each += 1u)
    {
        if ((file->text_run[each].hash == hash) && krep_text_is(file, file->text_run[each].number, bytes, length))
        {
            return file->text_run[each].number;
        }
    }
    unsigned int low = 0u;
    unsigned int high = file->text_ordered;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (file->text_order[middle].hash < hash)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    for (unsigned int each = low; (each < file->text_ordered) && (file->text_order[each].hash == hash); each += 1u)
    {
        if (krep_text_is(file, file->text_order[each].number, bytes, length))
        {
            return file->text_order[each].number;
        }
    }
    return 0u;
}

// the text numbered `number`, of `length` bytes at `bytes`, noted: into the run, the run sorted once and merged into
// the order from its end first where it is full, the order's cell grown first where it has no room for the run. 1, or
// 0 where there was no memory
static int krep_text_noted(KrepFile *file, unsigned int number, const char *bytes, unsigned long long length)
{
    if (file->text_run_count == KREP_TEXT_RUN)
    {
        if ((file->text_ordered + file->text_run_count) > file->text_order_room)
        {
            const unsigned int grown = (file->text_order_room == 0u) ? 1024u : (2u * file->text_order_room);
            KrepTextPlace *const larger = (KrepTextPlace *)platform_cell_grown(
                file->text_order, (size_t)file->text_order_room * sizeof(KrepTextPlace),
                (size_t)grown * sizeof(KrepTextPlace), platform_place_g_program_owner, 0);
            if (larger == NULL)
            {
                return 0;
            }
            file->text_order = larger;
            file->text_order_room = grown;
        }
        (void)EMBED_CALL(memor.sort, MemoriaCfg, .dst = file->text_run, .bytes = sizeof(KrepTextPlace),
                         .scratch = file->text_run_room, .count = file->text_run_count, .order = krep_place_order);
        unsigned int from_order = file->text_ordered;
        unsigned int from_run = file->text_run_count;
        unsigned int to = file->text_ordered + file->text_run_count;
        while (from_run != 0u)
        {
            const int order_first =
                (from_order != 0u) && (file->text_order[from_order - 1u].hash > file->text_run[from_run - 1u].hash);
            to -= 1u;
            if (order_first)
            {
                from_order -= 1u;
                file->text_order[to] = file->text_order[from_order];
            }
            else
            {
                from_run -= 1u;
                file->text_order[to] = file->text_run[from_run];
            }
        }
        file->text_ordered += file->text_run_count;
        file->text_run_count = 0u;
    }
    file->text_run[file->text_run_count] = (KrepTextPlace){krep_hash(bytes, length), number};
    file->text_run_count += 1u;
    return 1;
}

// the text of `length` bytes at `bytes`, taken as the next number, whether `file` gives it one or not. The result is
// the number, or 0 where there was no memory for it
unsigned long long krep_text_taken(KrepFile *file, const char *bytes, unsigned long long length)
{
    if (file->texts == file->text_room)
    {
        const unsigned int grown = (file->text_room == 0u) ? 256u : (2u * file->text_room);
        char **const larger =
            (char **)platform_cell_grown(file->text, (size_t)file->text_room * sizeof(char *),
                                         (size_t)grown * sizeof(char *), platform_place_g_program_owner, 0);
        if (larger == NULL)
        {
            return 0ULL;
        }
        file->text = larger;
        unsigned long long *const lengths = (unsigned long long *)platform_cell_grown(
            file->text_length, (size_t)file->text_room * sizeof(unsigned long long),
            (size_t)grown * sizeof(unsigned long long), platform_place_g_program_owner, 0);
        if (lengths == NULL)
        {
            return 0ULL;
        }
        file->text_length = lengths;
        file->text_room = grown;
    }
    // a text is at max the bytes of a file read whole, which a size_t counts
    char *const copy = (char *)platform_cell_taken((size_t)length + 1u, platform_place_g_program_owner, 0);
    if (copy == NULL)
    {
        return 0ULL;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = copy, .src = bytes, .bytes = (size_t)length);
    copy[length] = '\0';
    const int given = krep_text_found(file, bytes, length) != 0u;
    file->text[file->texts] = copy;
    file->text_length[file->texts] = length;
    file->texts += 1u;
    if (!given && !krep_text_noted(file, file->texts, bytes, length))
    {
        return 0ULL;
    }
    if (file->key != 0ULL)
    {
        krep_text_row(file, file->texts);
    }
    return file->texts;
}
