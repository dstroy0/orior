// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// krep_records.c: a k-file of records read, its records and texts kept, and written back whole (krep_records.h)
#include "krep_records.h"

#include "krep_head.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the most bytes a number takes, seven bits a byte
#define KREP_NUMBER_MOST 10u

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

int krep_write_by(const KrepFile *file, const char *path, QryWho who)
{
    // the file put together in memory and handed whole to the run's writer, which writes it in the file's place
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
    return qry_text_filed_by(qry_run(), path, &text, who) != 0ull;
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
    free(file->record[at].word);
    memmove(&file->record[at], &file->record[at + 1u], (size_t)(file->records - at - 1u) * sizeof(KrepRecord));
    file->records -= 1u;
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
