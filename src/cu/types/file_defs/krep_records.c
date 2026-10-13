// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// krep_records.c: a k-file of records read, its records and texts kept, and written back whole (krep_records.h)
#include "krep_records.h"

#include "krep_head.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the most bytes a number takes, seven bits a byte
#define KREP_NUMBER_MOST 10u
// the words of a row of a file's table before the change's own numbers: the file's key, the who's two words and the
// change
#define KREP_ROW_OPENING 4u

// the 32-bit word of the four little-endian bytes at `bytes`, the head's version
static unsigned int krep_half_of(const unsigned char *bytes)
{
    return (unsigned int)bytes[0] | ((unsigned int)bytes[1] << 8u) | ((unsigned int)bytes[2] << 16u) |
           ((unsigned int)bytes[3] << 24u);
}

// `half` as four little-endian bytes into `bytes`, the head's version
static void krep_half_put(unsigned char *bytes, unsigned int half)
{
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        bytes[at] = (unsigned char)(half >> (8u * at));
    }
}

// the number at `*at` of the `size` bytes at `bytes` into `value`, `*at` moved past it: 1, or 0 where it runs past
// the end or past ten bytes
static int krep_number_read(const unsigned char *bytes, unsigned long long size, unsigned long long *at,
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
static void krep_number_added(QryText *text, unsigned long long value)
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
static unsigned long long krep_row_handed(KrepFile *file, unsigned long long key, const unsigned long long *who,
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
static void krep_who_words_at(const KrepFile *file, unsigned int at, unsigned long long *words)
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
static void krep_record_row(KrepFile *file, unsigned int at, const unsigned long long *who)
{
    const KrepRecord *const held = &file->record[at];
    const unsigned long long numbers[3] = {at, held->code, held->count};
    krep_row_handed(file, file->key, who, KREP_ROW_PUT, numbers, 3u, held->word,
                    (unsigned long long)held->count * sizeof(unsigned long long), QRY_ORDINARY);
}

// the row of the text of the number `number` of `file`, of no who, handed to its table
static void krep_text_row(KrepFile *file, unsigned int number)
{
    static const unsigned long long s_none[2] = {0ull, 0ull};
    const unsigned long long numbers[2] = {number, file->text_length[number - 1u]};
    krep_row_handed(file, file->key, s_none, KREP_ROW_TEXT, numbers, 2u, file->text[number - 1u],
                    file->text_length[number - 1u], QRY_ORDINARY);
}

// the slot of the text of `length` bytes at `bytes` in `file`'s table: the one holding it, or the empty one it would
// be put in
static unsigned int krep_text_slot(const KrepFile *file, const char *bytes, unsigned long long length)
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
static unsigned long long krep_text_taken(KrepFile *file, const char *bytes, unsigned long long length)
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

void krep_open(KrepFile *file, const char *kind)
{
    krep_free(file);
    memcpy(file->kind, kind, 4u);
}

int krep_read(KrepFile *file, const char *path, const char *kind, char *error, size_t room)
{
    krep_open(file, kind);
    unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    if (!qry_file_read(qry_run(), path, &bytes, &size))
    {
        return 1;
    }
    if ((size < KREP_HEAD_BYTES) || (memcmp(bytes, KREP_MAGIC, 8u) != 0))
    {
        free(bytes);
        return 2;
    }
    const char *stopped = NULL;
    if (memcmp(bytes + 8u, kind, 4u) != 0)
    {
        stopped = "is not of the kind read";
    }
    else if (krep_half_of(bytes + 12u) != KREP_VERSION)
    {
        stopped = "is of another version";
    }
    unsigned long long at = KREP_HEAD_BYTES;
    static unsigned long long s_words[1u << 16u];
    while ((stopped == NULL) && (at < size))
    {
        unsigned long long code = 0ull;
        unsigned long long count = 0ull;
        if (!krep_number_read(bytes, size, &at, &code) || !krep_number_read(bytes, size, &at, &count) ||
            (code > 0xffffffffull))
        {
            stopped = "holds a record that runs past its end";
            break;
        }
        if (code == KREP_RECORD_TEXT)
        {
            // the count of a text is its bytes, each one of the file's
            if (count > (size - at))
            {
                stopped = "holds a text that runs past its end";
                break;
            }
            if (krep_text_taken(file, (const char *)(bytes + at), count) == 0ull)
            {
                stopped = "has a text there was no memory for";
            }
            at += count;
            continue;
        }
        // a record's count is at most its numbers, each at least one of the file's bytes
        if (count > (size - at))
        {
            stopped = "holds a record that runs past its end";
            break;
        }
        unsigned long long *const owned = (count > (sizeof(s_words) / sizeof(s_words[0])))
                                              ? (unsigned long long *)malloc((size_t)count * sizeof(unsigned long long))
                                              : NULL;
        unsigned long long *const words = (owned != NULL) ? owned : s_words;
        if ((count > (sizeof(s_words) / sizeof(s_words[0]))) && (owned == NULL))
        {
            stopped = "has a record there was no memory for";
            break;
        }
        for (unsigned long long word = 0ull; (stopped == NULL) && (word < count); word += 1ull)
        {
            stopped = krep_number_read(bytes, size, &at, &words[word]) ? NULL : "holds a record that runs past its end";
        }
        // a code and a count read past those above are at most what an unsigned int holds
        if ((stopped == NULL) && !krep_record_add(file, (unsigned int)code, words, (unsigned int)count))
        {
            stopped = "has a record there was no memory for";
        }
        free(owned);
    }
    free(bytes);
    if (stopped != NULL)
    {
        snprintf(error, room, "%s %s", path, stopped);
        krep_open(file, kind);
        return 0;
    }
    return 1;
}

// `file` put together in memory and handed whole to the run's writer as the file at `path`, written by `who`, which
// writes it in the file's place: the sequence of its bytes' blob, or 0 where it could not be written
static unsigned long long krep_filed(const KrepFile *file, const char *path, QryWho who)
{
    QryText text = {0};
    unsigned char head[KREP_HEAD_BYTES];
    memcpy(head, KREP_MAGIC, 8u);
    memcpy(head + 8u, file->kind, 4u);
    krep_half_put(head + 12u, KREP_VERSION);
    qry_text_added(&text, head, sizeof(head));
    for (unsigned int number = 0u; number < file->texts; number += 1u)
    {
        krep_number_added(&text, KREP_RECORD_TEXT);
        krep_number_added(&text, file->text_length[number]);
        qry_text_added(&text, file->text[number], file->text_length[number]);
    }
    for (unsigned int at = 0u; at < file->records; at += 1u)
    {
        const KrepRecord *const held = &file->record[at];
        krep_number_added(&text, held->code);
        krep_number_added(&text, held->count);
        for (unsigned int place = 0u; place < held->count; place += 1u)
        {
            krep_number_added(&text, held->word[place]);
        }
    }
    return qry_text_filed_by(qry_run(), path, &text, who);
}

int krep_write_by(const KrepFile *file, const char *path, QryWho who)
{
    if (file->key == 0ull)
    {
        return krep_filed(file, path, who) != 0ull;
    }
    // the file its rows put together written, the view of its table, and not the one held: its bytes' blob linked to
    // the head row of the rows it was written from
    KrepFile view;
    memset(&view, 0, sizeof(view));
    const unsigned long long written = krep_viewed(file, &view) ? krep_filed(&view, path, who) : 0ull;
    krep_free(&view);
    if (written != 0ull)
    {
        const unsigned long long link[3] = {written, file->key, QRY_LINK_WRITTEN_FROM};
        qry_row_handed_by(file->run, QRY_LINKS_TABLE, link, 3u, QRY_ORDINARY, who);
    }
    return written != 0ull;
}

void krep_free(KrepFile *file)
{
    for (unsigned int at = 0u; at < file->records; at += 1u)
    {
        free(file->record[at].word);
    }
    for (unsigned int number = 0u; number < file->texts; number += 1u)
    {
        free(file->text[number]);
    }
    free(file->record);
    free(file->text);
    free(file->text_length);
    free(file->text_slot);
    memset(file, 0, sizeof(*file));
}

int krep_record_put(KrepFile *file, unsigned int at, unsigned int code, const unsigned long long *words,
                    unsigned int count)
{
    if (at > file->records)
    {
        return 0;
    }
    if (file->records == file->room)
    {
        const unsigned int grown = (file->room == 0u) ? 1024u : (2u * file->room);
        KrepRecord *const larger = (KrepRecord *)realloc(file->record, (size_t)grown * sizeof(KrepRecord));
        if (larger == NULL)
        {
            return 0;
        }
        file->record = larger;
        file->room = grown;
    }
    unsigned long long *const copy = (unsigned long long *)malloc(((size_t)count + 1u) * sizeof(unsigned long long));
    if (copy == NULL)
    {
        return 0;
    }
    if (count != 0u)
    {
        memcpy(copy, words, (size_t)count * sizeof(unsigned long long));
    }
    memmove(&file->record[at + 1u], &file->record[at], (size_t)(file->records - at) * sizeof(KrepRecord));
    file->record[at].code = code;
    file->record[at].count = count;
    file->record[at].word = copy;
    file->records += 1u;
    if (file->key != 0ull)
    {
        unsigned long long who[2];
        krep_who_words_at(file, at, who);
        krep_record_row(file, at, who);
    }
    return 1;
}

int krep_record_add(KrepFile *file, unsigned int code, const unsigned long long *words, unsigned int count)
{
    return krep_record_put(file, file->records, code, words, count);
}

void krep_record_drop(KrepFile *file, unsigned int at)
{
    if (at >= file->records)
    {
        return;
    }
    if (file->key != 0ull)
    {
        unsigned long long who[2];
        krep_who_words_at(file, at, who);
        const unsigned long long numbers[1] = {at};
        krep_row_handed(file, file->key, who, KREP_ROW_DROP, numbers, 1u, NULL, 0ull, QRY_ORDINARY);
    }
    free(file->record[at].word);
    memmove(&file->record[at], &file->record[at + 1u], (size_t)(file->records - at - 1u) * sizeof(KrepRecord));
    file->records -= 1u;
}

int krep_record_set(KrepFile *file, unsigned int at, const unsigned long long *words, unsigned int count)
{
    if (at >= file->records)
    {
        return 0;
    }
    unsigned long long *const copy = (unsigned long long *)malloc(((size_t)count + 1u) * sizeof(unsigned long long));
    if (copy == NULL)
    {
        return 0;
    }
    if (count != 0u)
    {
        memcpy(copy, words, (size_t)count * sizeof(unsigned long long));
    }
    free(file->record[at].word);
    file->record[at].word = copy;
    file->record[at].count = count;
    if (file->key != 0ull)
    {
        unsigned long long who[2];
        krep_who_words_at(file, at, who);
        const unsigned long long numbers[2] = {at, count};
        krep_row_handed(file, file->key, who, KREP_ROW_SET, numbers, 2u, copy,
                        (unsigned long long)count * sizeof(unsigned long long), QRY_ORDINARY);
    }
    return 1;
}

int krep_tabled_by(KrepFile *file, QryBuffer *buffer, const char *table, const char *path, QryWho who)
{
    file->key = 0ull;
    char name[QRY_NAME_LONGEST + 1u];
    if ((buffer == NULL) || !qry_file_named(path, name, sizeof(name)))
    {
        return 0;
    }
    file->run = buffer;
    file->table = table;
    unsigned long long words[2];
    qry_who_words(who, words);
    const unsigned long long length = strlen(name);
    const unsigned long long head[3] = {krep_half_of((const unsigned char *)file->kind), KREP_VERSION, length};
    file->key = krep_row_handed(file, 0ull, words, KREP_ROW_HEAD, head, 3u, name, length, QRY_CRITICAL);
    for (unsigned int number = 1u; (file->key != 0ull) && (number <= file->texts); number += 1u)
    {
        krep_text_row(file, number);
    }
    // each record's who the who in force over it, carried down the records from each who record
    words[0] = 0ull;
    words[1] = 0ull;
    for (unsigned int at = 0u; (file->key != 0ull) && (at < file->records); at += 1u)
    {
        const KrepRecord *const held = &file->record[at];
        if (held->code == KREP_RECORD_WHO)
        {
            words[0] = (held->count == 2u) ? held->word[0] : 0ull;
            words[1] = (held->count == 2u) ? held->word[1] : 0ull;
        }
        krep_record_row(file, at, words);
    }
    return file->key != 0ull;
}

// the file the rows among `bytes`, `size` of them, of the key `key` put together into `view`, which stands open and
// empty of its kind: the rows joined, each whole words, its opening, the numbers its change gives, then its bytes or
// its words. 1, or 0 where a row does not fit the file it puts together or the bytes are not whole rows
static int krep_rows_viewed(const unsigned char *bytes, unsigned long long size, unsigned long long key, KrepFile *view)
{
    const unsigned long long words = size / sizeof(unsigned long long);
    unsigned long long at = 0ull;
    int held = ((size % sizeof(unsigned long long)) == 0ull);
    while (held && ((at + KREP_ROW_OPENING) <= words))
    {
        unsigned long long opening[KREP_ROW_OPENING];
        memcpy(opening, bytes + (at * sizeof(unsigned long long)), sizeof(opening));
        at += KREP_ROW_OPENING;
        const unsigned long long change = opening[3];
        const unsigned int given = ((change == KREP_ROW_HEAD) || (change == KREP_ROW_PUT))   ? 3u
                                   : ((change == KREP_ROW_TEXT) || (change == KREP_ROW_SET)) ? 2u
                                   : (change == KREP_ROW_DROP)                               ? 1u
                                                                                             : 0u;
        if ((given == 0u) || ((at + given) > words))
        {
            held = 0;
            break;
        }
        unsigned long long number[3] = {0ull, 0ull, 0ull};
        memcpy(number, bytes + (at * sizeof(unsigned long long)), given * sizeof(unsigned long long));
        at += given;
        // a head's and a text's bytes, filled out to whole words, and a put's and a set's words
        const unsigned long long following = ((change == KREP_ROW_HEAD) || (change == KREP_ROW_TEXT))
                                                 ? ((number[given - 1u] + 7ull) / 8ull)
                                             : (change == KREP_ROW_PUT) ? number[2]
                                             : (change == KREP_ROW_SET) ? number[1]
                                                                        : 0ull;
        if (following > (words - at))
        {
            held = 0;
            break;
        }
        const unsigned char *const after = bytes + (at * sizeof(unsigned long long));
        at += following;
        // a head's key is 0 and is no file's; a row of another file's key is passed over
        if (opening[0] != key)
        {
            continue;
        }
        // a place, a code and a count read here are each at most what an unsigned int holds, as held to it
        const int counted =
            (number[0] <= 0xffffffffull) && (number[1] <= 0xffffffffull) && (number[2] <= 0xffffffffull);
        switch (change)
        {
        case KREP_ROW_TEXT:
            held = (number[0] == (view->texts + 1ull)) &&
                   (krep_text_taken(view, (const char *)after, number[1]) == number[0]);
            break;
        case KREP_ROW_PUT:
            held = counted && (number[0] <= view->records) &&
                   krep_record_put(view, (unsigned int)number[0], (unsigned int)number[1],
                                   (const unsigned long long *)(const void *)after, (unsigned int)number[2]);
            break;
        case KREP_ROW_SET:
            held = counted && (number[0] < view->records) &&
                   krep_record_set(view, (unsigned int)number[0], (const unsigned long long *)(const void *)after,
                                   (unsigned int)number[1]);
            break;
        case KREP_ROW_DROP:
            held = (number[0] < view->records);
            if (held)
            {
                krep_record_drop(view, (unsigned int)number[0]);
            }
            break;
        default:
            break;
        }
    }
    return held && (at == words);
}

int krep_viewed(const KrepFile *file, KrepFile *view)
{
    krep_open(view, file->kind);
    unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    if ((file->key == 0ull) || !qry_joined(file->run, file->table, &bytes, &size))
    {
        return 0;
    }
    const int held = krep_rows_viewed(bytes, size, file->key, view);
    free(bytes);
    if (!held)
    {
        krep_open(view, file->kind);
    }
    return held;
}

// the rows of one table gathered from a finished .qry (krep_row_gathered): the table's name; the name of the file
// sought, NULL for the first the table holds; the rows' bytes joined in the order of their sequences, their count and
// the room they have; the key of the file's head row and the file's kind, 0 and empty until the head is seen; and
// whether memory ran out
typedef struct
{
    const char *table;
    const char *name;
    unsigned char *bytes;
    unsigned long long size;
    unsigned long long room;
    unsigned long long key;
    char kind[5];
    int failed;
} KrepGathered;

// the words of a head row before its name: the opening, then the kind, the version and the name's length
#define KREP_HEAD_WORDS (KREP_ROW_OPENING + 3u)

// one blob of the .qry, handed by qry_file_each in the order of the sequences: a row of the table gathered whole, and
// the first head row of the file sought, or of any file where none is sought, giving the key and the kind
static void krep_row_gathered(void *context, const char *name, unsigned long long sequence, const unsigned char *bytes,
                              unsigned long long size)
{
    KrepGathered *const gathered = (KrepGathered *)context;
    if (gathered->failed || (strcmp(name, gathered->table) != 0))
    {
        return;
    }
    const unsigned long long words = size / sizeof(unsigned long long);
    unsigned long long opening[KREP_HEAD_WORDS] = {0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull};
    if (words >= KREP_HEAD_WORDS)
    {
        memcpy(opening, bytes, sizeof(opening));
    }
    if ((gathered->key == 0ull) && (words >= KREP_HEAD_WORDS) && (opening[3] == KREP_ROW_HEAD))
    {
        const unsigned long long length = opening[KREP_HEAD_WORDS - 1u];
        const unsigned long long named_at = KREP_HEAD_WORDS * sizeof(unsigned long long);
        const int sought = (gathered->name == NULL) ||
                           ((length == strlen(gathered->name)) && (length <= (size - named_at)) &&
                            (memcmp(bytes + named_at, gathered->name, (size_t)length) == 0));
        if (sought)
        {
            gathered->key = sequence;
            for (unsigned int at = 0u; at < 4u; at += 1u)
            {
                // the kind's four bytes stand the low first in its word
                gathered->kind[at] = (char)((opening[KREP_ROW_OPENING] >> (8u * at)) & 0xffull);
            }
            gathered->kind[4] = '\0';
        }
    }
    if ((gathered->size + size) > gathered->room)
    {
        unsigned long long room = (gathered->room != 0ull) ? gathered->room : 65536ull;
        while (room < (gathered->size + size))
        {
            room *= 2ull;
        }
        // a row's bytes fit a size_t: every blob of a .qry is mapped whole
        unsigned char *const grown = (unsigned char *)realloc(gathered->bytes, (size_t)room);
        if (grown == NULL)
        {
            gathered->failed = 1;
            return;
        }
        gathered->bytes = grown;
        gathered->room = room;
    }
    memcpy(gathered->bytes + gathered->size, bytes, (size_t)size);
    gathered->size += size;
}

int krep_viewed_at(const char *path, const char *table, const char *name, KrepFile *view, char *error, size_t room)
{
    KrepGathered gathered = {table, name, NULL, 0ull, 0ull, 0ull, "", 0};
    const int read = qry_file_each(path, krep_row_gathered, &gathered, error, room);
    int held = read && !gathered.failed && (gathered.key != 0ull);
    if (read && gathered.failed)
    {
        snprintf(error, room, "%s: no memory for the rows of %s", path, table);
    }
    else if (read && (gathered.key == 0ull))
    {
        snprintf(error, room, "%s: the table %s holds no head of %s", path, table, (name != NULL) ? name : "a file");
    }
    krep_open(view, held ? gathered.kind : "\0\0\0\0");
    if (held && !krep_rows_viewed(gathered.bytes, gathered.size, gathered.key, view))
    {
        snprintf(error, room, "%s: a row of %s does not fit the file it puts together", path, table);
        krep_open(view, gathered.kind);
        held = 0;
    }
    free(gathered.bytes);
    return held;
}

unsigned long long krep_text_number(KrepFile *file, const char *bytes, unsigned long long length)
{
    if ((file->text_slots != 0u) && (file->text_slot[krep_text_slot(file, bytes, length)] != 0u))
    {
        return file->text_slot[krep_text_slot(file, bytes, length)];
    }
    return krep_text_taken(file, bytes, length);
}

const char *krep_text(const KrepFile *file, unsigned long long number, unsigned long long *length)
{
    if ((number == 0ull) || (number > file->texts))
    {
        *length = 0ull;
        return NULL;
    }
    *length = file->text_length[number - 1u];
    return file->text[number - 1u];
}

int krep_who_at(const KrepFile *file, unsigned int at, QryWho *who)
{
    for (unsigned int before = (at < file->records) ? at : file->records; before != 0u; before -= 1u)
    {
        const KrepRecord *const held = &file->record[before - 1u];
        if (held->code != KREP_RECORD_WHO)
        {
            continue;
        }
        // a who record of no words says no who is in force from it on
        if (held->count != 2u)
        {
            return 0;
        }
        *who = qry_who_of_words(held->word);
        return 1;
    }
    return 0;
}

int krep_who_same(QryWho one, QryWho other)
{
    return (one.source == other.source) && (one.line == other.line) && (one.program == other.program);
}
