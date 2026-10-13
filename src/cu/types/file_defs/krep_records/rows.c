// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// rows.c: the rows: a number's halves, a number read, put and added, the hash, a row handed to the run with its who, a record's row and a text's, the text slots grown, and a text taken
#include "krep_internal.h"

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
    *value = 0ull;
    for (unsigned int taken = 0u; (taken < KREP_NUMBER_MOST) && (*at < size); taken += 1u)
    {
        const unsigned char byte = bytes[*at];
        *at += 1ull;
        *value |= (unsigned long long)(byte & 0x7fu) << (7u * taken);
        if ((byte & 0x80u) == 0u)
        {
            return 1;
        }
    }
    return 0;
}

// `value` written into `bytes`, which holds KREP_NUMBER_MOST, seven bits a byte: the bytes it took
static unsigned int krep_number_put(unsigned char *bytes, unsigned long long value)
{
    unsigned int taken = 0u;
    do
    {
        const unsigned char low = (unsigned char)(value & 0x7full);
        value >>= 7u;
        bytes[taken] = (unsigned char)(low | ((value != 0ull) ? 0x80u : 0u));
        taken += 1u;
    } while (value != 0ull);
    return taken;
}

// `value` added to `text`, seven bits a byte
void krep_number_added(QryText *text, unsigned long long value)
{
    unsigned char bytes[KREP_NUMBER_MOST];
    qry_text_added(text, bytes, krep_number_put(bytes, value));
}

// a 64-bit FNV-1a hash of `length` bytes at `bytes`
static unsigned long long krep_hash(const char *bytes, unsigned long long length)
{
    unsigned long long hash = 0xcbf29ce484222325ull;
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        hash = (hash ^ (unsigned char)bytes[at]) * 0x100000001b3ull;
    }
    return hash;
}

// A row of `file`'s table of the key `key`, of the who two words `who` hold and of the change `change`: its `count`
// numbers from `numbers`, then `size` bytes at `bytes` filled out to a whole word with zeros, handed at `priority`.
// Its sequence where it is critical, 1 where it is ordinary, or 0 where it could not be handed, and the file then no
// table's
unsigned long long krep_row_handed(KrepFile *file, unsigned long long key, const unsigned long long *who,
                                          unsigned int change, const unsigned long long *numbers, unsigned int count,
                                          const void *bytes, unsigned long long size, unsigned int priority)
{
    static const unsigned char s_zeros[8] = {0u};
    const unsigned long long opening[KREP_ROW_OPENING] = {key, who[0], who[1], change};
    QryText row = {0};
    qry_text_added(&row, opening, sizeof(opening));
    qry_text_added(&row, numbers, (unsigned long long)count * sizeof(unsigned long long));
    qry_text_added(&row, bytes, size);
    qry_text_added(&row, s_zeros, (8ull - (size % 8ull)) % 8ull);
    const unsigned long long sequence =
        row.failed ? 0ull : qry_hand(file->run, file->table, row.bytes, row.size, priority);
    qry_text_freed(&row);
    file->key = (sequence != 0ull) ? file->key : 0ull;
    return sequence;
}

// the two words of the who in force over the record at `at` of `file` into `words`: a who record's own, or the
// nearest one's before it, zeros where none is
void krep_who_words_at(const KrepFile *file, unsigned int at, unsigned long long *words)
{
    words[0] = 0ull;
    words[1] = 0ull;
    if ((at < file->records) && (file->record[at].code == KREP_RECORD_WHO))
    {
        if (file->record[at].count == 2u)
        {
            words[0] = file->record[at].word[0];
            words[1] = file->record[at].word[1];
        }
        return;
    }
    QryWho who;
    if (krep_who_at(file, at, &who))
    {
        qry_who_words(who, words);
    }
}

// the row of the record at `at` of `file`, put there, of the who two words `who` hold, handed to its table
void krep_record_row(KrepFile *file, unsigned int at, const unsigned long long *who)
{
    const KrepRecord *const held = &file->record[at];
    const unsigned long long numbers[3] = {at, held->code, held->count};
    krep_row_handed(file, file->key, who, KREP_ROW_PUT, numbers, 3u, held->word,
                    (unsigned long long)held->count * sizeof(unsigned long long), QRY_ORDINARY);
}

// the row of the text of the number `number` of `file`, of no who, handed to its table
void krep_text_row(KrepFile *file, unsigned int number)
{
    static const unsigned long long s_none[2] = {0ull, 0ull};
    const unsigned long long numbers[2] = {number, file->text_length[number - 1u]};
    krep_row_handed(file, file->key, s_none, KREP_ROW_TEXT, numbers, 2u, file->text[number - 1u],
                    file->text_length[number - 1u], QRY_ORDINARY);
}

// the slot of the text of `length` bytes at `bytes` in `file`'s table: the one holding it, or the empty one it would
// be put in
unsigned int krep_text_slot(const KrepFile *file, const char *bytes, unsigned long long length)
{
    unsigned int at = (unsigned int)(krep_hash(bytes, length) & (file->text_slots - 1u));
    while (file->text_slot[at] != 0u)
    {
        const unsigned int held = file->text_slot[at] - 1u;
        if ((file->text_length[held] == length) && (memcmp(file->text[held], bytes, (size_t)length) == 0))
        {
            return at;
        }
        at = (at + 1u) & (file->text_slots - 1u);
    }
    return at;
}

// `file`'s table of texts grown to hold twice the texts it holds and more: 1, or 0 where there was no memory
static int krep_text_slots_grown(KrepFile *file)
{
    unsigned int slots = (file->text_slots == 0u) ? 1024u : file->text_slots;
    while (slots <= (2u * (file->texts + 1u)))
    {
        slots *= 2u;
    }
    if (slots == file->text_slots)
    {
        return 1;
    }
    unsigned int *const table = (unsigned int *)calloc(slots, sizeof(unsigned int));
    if (table == NULL)
    {
        return 0;
    }
    free(file->text_slot);
    file->text_slot = table;
    file->text_slots = slots;
    for (unsigned int number = 0u; number < file->texts; number += 1u)
    {
        file->text_slot[krep_text_slot(file, file->text[number], file->text_length[number])] = number + 1u;
    }
    return 1;
}

// the text of `length` bytes at `bytes` taken as the next number, whether `file` gives it one or not: the number, or 0
// where there was no memory for it
unsigned long long krep_text_taken(KrepFile *file, const char *bytes, unsigned long long length)
{
    if (!krep_text_slots_grown(file))
    {
        return 0ull;
    }
    if (file->texts == file->text_room)
    {
        const unsigned int grown = (file->text_room == 0u) ? 256u : (2u * file->text_room);
        char **const larger = (char **)realloc(file->text, (size_t)grown * sizeof(char *));
        if (larger == NULL)
        {
            return 0ull;
        }
        file->text = larger;
        unsigned long long *const lengths =
            (unsigned long long *)realloc(file->text_length, (size_t)grown * sizeof(unsigned long long));
        if (lengths == NULL)
        {
            return 0ull;
        }
        file->text_length = lengths;
        file->text_room = grown;
    }
    // a text is at most the bytes of a file read whole, which a size_t counts
    char *const copy = (char *)malloc((size_t)length + 1u);
    if (copy == NULL)
    {
        return 0ull;
    }
    memcpy(copy, bytes, (size_t)length);
    copy[length] = '\0';
    const unsigned int slot = krep_text_slot(file, bytes, length);
    file->text[file->texts] = copy;
    file->text_length[file->texts] = length;
    file->texts += 1u;
    if (file->text_slot[slot] == 0u)
    {
        file->text_slot[slot] = file->texts;
    }
    if (file->key != 0ull)
    {
        krep_text_row(file, file->texts);
    }
    return file->texts;
}
